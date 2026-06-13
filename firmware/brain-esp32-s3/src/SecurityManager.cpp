#include "SecurityManager.h"
#include "Config.h" // Assuming pins like LASER_PIN, BUZZER_PIN are here
#include "Globals.h"
#include "TelegramManager.h" // Assuming SensingAP struct is here
#include "GoogleHomeManager.h"
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <Wire.h>


// --- Link to Main System Peripherals (Temporary until they get their own
// managers) ---
extern LiquidCrystal_I2C lcd;
extern Adafruit_NeoPixel LED_RGB;
extern void passiveBuzzerBeep(int durationMs);
extern void passiveBuzzerBeep(int durationMs);

// We will properly route telegram later, but for now we just tell the compiler
// it exists:

// --- Internal Security Variables ---
volatile bool systemArmed = false;
volatile bool isAlarmActive = false;
bool isSirenDetected = false;
int webSoundSensitivity = 50;

// Sound logic
float audioDCOffset = 2048.0;
float ambientNoise = 0.0;
int currentSensitivity = 0;

// CSI/Gait logic
volatile float gaitDisturbanceScore = 0.0;
int activeAPCount = 1;
const float BASE_THRESHOLD = 0.85;
const float MIN_THRESHOLD = 0.55;
const float SENSITIVITY_BOOST = 0.08;
SensingAP sensingTable[MAX_SENSING_APS];
float csiChannelAmplitudes[MAX_SENSING_APS][CSI_CHANNELS];
float alpha = 0.005; // Reduced from 0.05 so it doesn't adapt to walking too fast!

// --- Implementations ---

void resetSystemState() {
  Serial.println("System Armed. Waiting for trip...");
  Serial2.println("S"); // Tell the sub-board
  LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0));
  LED_RGB.show();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Working");
  lcd.setCursor(0, 1);
  lcd.print("For now...");
  if (isAlarmActive) { restoreGoogleHomeNormal(); }
  isAlarmActive = false;
  systemArmed = true;
}

void disarmSystem() {
  systemArmed = false;
  if (isAlarmActive) {
    Serial2.println("S");
    isAlarmActive = false;
    restoreGoogleHomeNormal();
  }
  LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(255, 165, 0)));
  LED_RGB.show();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Disarmed");
  lcd.setCursor(0, 1);
  lcd.print("Welcome Home");
}

void triggerSeinagermos(String reason) {
  if (isAlarmActive)
    return;
  isAlarmActive = true;
  LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0));
  LED_RGB.show();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System");
  lcd.setCursor(0, 1);
  lcd.print("Triggered!");
  Serial2.println("A"); // Tell the sub-board to blast the alarm
  triggerGoogleHomeAlarm();

  strlcpy(alarmReason, reason.c_str(), sizeof(alarmReason));
  pendingAlarmNotification = true;

  Serial.println("Alarm active via: " + reason);
}

// --- CSI Math Functions ---
// Pass the specific AP's background array as a pointer
float calculateGaitScore(int8_t *raw_csi_data, int len, float *ap_background,
                         int ap_index) {
  float current_variance = 0;
  int count = 0;

  for (int i = 0; i < (len - 1) && (i / 2) < CSI_CHANNELS; i += 2) {
    int8_t imag = raw_csi_data[i];
    int8_t real = raw_csi_data[i + 1];
    float amplitude = sqrt((imag * imag) + (real * real));
    int channel_idx = i / 2;

    // Update this specific AP's baseline
    ap_background[channel_idx] =
        (alpha * amplitude) + (1.0 - alpha) * ap_background[channel_idx];

    // Use fabs() for floating-point absolute values
    float dynamic_signal = fabs(amplitude - ap_background[channel_idx]);
    current_variance += dynamic_signal;
    count++;

    // Store per-channel amplitude for the radar visualization
    if (ap_index >= 0 && ap_index < MAX_SENSING_APS) {
      csiChannelAmplitudes[ap_index][channel_idx] = dynamic_signal;
    }
  }

  // Changed divisor from 20.0 to 10.0 to make it twice as sensitive
  float score = (count > 0) ? (current_variance / count) / 10.0 : 0;
  if (score > 1.0)
    score = 1.0;
  return score;
}

void csi_rx_callback(void *ctx, wifi_csi_info_t *data) {
  if (!systemArmed) {
    gaitDisturbanceScore = 0;
    for (int i = 0; i < MAX_SENSING_APS; i++) {
      sensingTable[i].lastScore = 0;
      for (int c = 0; c < CSI_CHANNELS; c++) {
        csiChannelAmplitudes[i][c] = 0;
      }
    }
    return;
  }

  for (int i = 0; i < MAX_SENSING_APS; i++) {
    if (sensingTable[i].active &&
        memcmp(data->mac, sensingTable[i].mac, 6) == 0) {

      // Throttling PER AP, not globally!
      static unsigned long last_csi_times[MAX_SENSING_APS] = {0};
      if (millis() - last_csi_times[i] < 50) return;
      last_csi_times[i] = millis();

      // Pass the correct baseline array to the math function
      float new_score = calculateGaitScore(
          data->buf, data->len, sensingTable[i].static_background, i);

      sensingTable[i].lastScore = new_score;
      float aggregate = 0;
      int count = 0;

      for (int j = 0; j < MAX_SENSING_APS; j++) {
        if (sensingTable[j].active) {
          aggregate += sensingTable[j].lastScore;
          count++;
        }
      }
      gaitDisturbanceScore = (count > 0) ? (aggregate / count) : 0;
      break;
    }
  }
}

void addSensingAP(uint8_t *new_mac) {
  if (activeAPCount >= MAX_SENSING_APS) {
    Serial.println("Mesh Full!");
    return;
  }
  for (int i = 0; i < activeAPCount; i++) {
    if (memcmp(sensingTable[i].mac, new_mac, 6) == 0)
      return;
  }
  memcpy(sensingTable[activeAPCount].mac, new_mac, 6);
  sensingTable[activeAPCount].active = true;
  sensingTable[activeAPCount].lastScore = 0.0;
  activeAPCount++;

  Preferences prefs;
  prefs.begin("gait_mesh", false);
  prefs.putInt("ap_count", activeAPCount);
  String key = "mac_" + String(activeAPCount - 1);
  prefs.putBytes(key.c_str(), new_mac, 6);
  prefs.end();
  Serial.println("New AP added to Sensing Mesh.");
}

void removeSensingAP(int index) {
  if (index <= 0 || index >= activeAPCount) return; // Cannot delete router at index 0

  for (int i = index; i < activeAPCount - 1; i++) {
    sensingTable[i] = sensingTable[i+1];
  }
  activeAPCount--;

  Preferences prefs;
  prefs.begin("gait_mesh", false);
  prefs.putInt("ap_count", activeAPCount);
  for (int i = 1; i < activeAPCount; i++) {
    String key = "mac_" + String(i);
    prefs.putBytes(key.c_str(), sensingTable[i].mac, 6);
  }
  prefs.end();
  Serial.println("AP removed from Sensing Mesh.");
}

void loadSensingAPs() {
  Preferences prefs;
  prefs.begin("gait_mesh", true);
  int savedCount = prefs.getInt("ap_count", 0);

  // Start from index 1 because index 0 is the router (set in main.cpp setup)
  for (int i = 1; i < savedCount && i < MAX_SENSING_APS; i++) {
    String key = "mac_" + String(i);
    if (prefs.getBytes(key.c_str(), sensingTable[i].mac, 6) == 6) {
      sensingTable[i].active = true;
      sensingTable[i].lastScore = 0.0;
      activeAPCount = i + 1;
    }
  }
  prefs.end();
  Serial.println("Loaded " + String(activeAPCount) +
                 " Sensing APs from memory.");
}

void setupSecurity() {
  loadSensingAPs();
  pinMode(MQ2_PIN, INPUT);

  esp_err_t csi_err = esp_wifi_set_csi(true);

  if (csi_err == ESP_OK) {
    // Explicitly set the safe default flags for ESP32-S3
    wifi_csi_config_t csi_config = {.lltf_en = true,
                                    .htltf_en = true,
                                    .stbc_htltf2_en = true,
                                    .ltf_merge_en = true,
                                    .channel_filter_en = false,
                                    .manu_scale = false,
                                    .shift = false};

    esp_wifi_set_csi_config(&csi_config);
    esp_wifi_set_csi_rx_cb(csi_rx_callback, NULL);
    Serial.println("Gait Analysis Engine Started.");
  } else {
    Serial.printf("CSI Error: %s\n", esp_err_to_name(csi_err));
  }
}

// --- The RTOS Task ---
void securityLoop(void *pvParameters) {
  extern bool devModeActive; // Pull this from main for now

  for (;;) {
    int sensorState = digitalRead(LASER_PIN);
    int buttonState = digitalRead(RESET_BUTTON);

    // --- GAS/SMOKE SENSOR ---
    int gasValue = analogRead(MQ2_PIN);
    if (gasValue > 2000) {
      triggerSeinagermos("FIRE/SMOKE DETECTED! (MQ-2 Sensor)");
    }

    // --- IMPROVED MICROPHONE SAMPLING ---
    // Sampling exactly once every 50ms misses 95% of the audio wave.
    // We do a quick 10ms tight loop to catch the peak of the sound wave.
    unsigned long startMillis = millis();
    int maxRead = 0;
    int minRead = 4095;
    while (millis() - startMillis < 10) {
      int r = analogRead(SOUND_PIN);
      if (r > maxRead)
        maxRead = r;
      if (r < minRead)
        minRead = r;
    }

    // The true sound level is the peak-to-peak amplitude.
    // We use the raw maximum to feed into the DC offset tracker.
    int soundLevel = maxRead;

    // --- FIXED MICROPHONE MATH ---
    // 1. Calculate the resting baseline (DC Offset)
    audioDCOffset = (0.005 * soundLevel) + (0.995 * audioDCOffset);

    // 2. Calculate the peak amplitude above the baseline
    float currentVolume = (maxRead - minRead) / 2.0;
    if (currentVolume < 0)
      currentVolume = 0;

    // 3. Track ambient room noise slowly over time
    ambientNoise = (0.01 * currentVolume) + (0.99 * ambientNoise);

    // 4. Use the Web Slider (20 to 150) as the explicit trigger threshold above
    // ambient noise. If the slider is 20 (low), it takes very little noise
    // above ambient to trigger. If the slider is 150 (high), it takes a loud
    // noise to trigger.
    float dynamicThreshold = ambientNoise + webSoundSensitivity;

    if (devModeActive) {
      Serial.print("Volume:");
      Serial.print(currentVolume);
      Serial.print(", Threshold:");
      Serial.print(dynamicThreshold);
      Serial.print(", AmbientNoise:");
      Serial.println(ambientNoise);
    }

    // Trigger the LED if the volume crosses the threshold
    if (currentVolume > dynamicThreshold) {
      digitalWrite(SOUND_LED_PIN, HIGH);
      isSirenDetected = true;
    } else {
      digitalWrite(SOUND_LED_PIN, LOW);
      isSirenDetected = false;
    }
    // -----------------------------

    if (isAlarmActive && isSirenDetected) {
      static unsigned long lastSirenNotif = 0;
      if (millis() - lastSirenNotif > 60000) { // Max once per minute
        sendTelegramMessage("🔊 *Loud Noise / Siren Confirmed While Alarm Active!*", "Markdown");
        lastSirenNotif = millis();
      }
    }

    if (systemArmed && !isAlarmActive) {
      if (laserEnabled && sensorState == HIGH)
        triggerSeinagermos("Laser Tripwire Broken");

      float dynamicCsiThreshold =
          BASE_THRESHOLD - ((activeAPCount - 1) * SENSITIVITY_BOOST);
      if (dynamicCsiThreshold < MIN_THRESHOLD)
        dynamicCsiThreshold = MIN_THRESHOLD;

      if (gaitDisturbanceScore > dynamicCsiThreshold) {
        String reason =
            "Gait Mesh Triggered (" + String(activeAPCount) + " APs)";
        triggerSeinagermos(reason);
      }
    }

    if ((buttonState == LOW || pendingArmTimer > 0) && !systemArmed &&
        !isAlarmActive) {
      int countdown =
          (pendingArmTimer > 0) ? pendingArmTimer : armingDelaySeconds;
      pendingArmTimer = -1; // Reset it immediately

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Arming Mesh...");

      for (int i = countdown; i > 0; i--) {
        lcd.setCursor(0, 1);
        lcd.print(String(i) + "s | " + String(activeAPCount) + " APs active");
        LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 255, 255)));
        LED_RGB.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 0, 0)));
        LED_RGB.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      passiveBuzzerBeep(600);
      resetSystemState();
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}