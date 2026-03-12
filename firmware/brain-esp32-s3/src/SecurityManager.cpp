#include "SecurityManager.h"
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"  // Assuming pins like LASER_PIN, BUZZER_PIN are here
#include "Globals.h"
#include "TelegramManager.h" // Assuming SensingAP struct is here

// --- Link to Main System Peripherals (Temporary until they get their own managers) ---
extern LiquidCrystal_I2C lcd;
extern Adafruit_NeoPixel LED_RGB;
extern void passiveBuzzerBeep(int durationMs);
extern void sendNotification(String message);

// We will properly route telegram later, but for now we just tell the compiler it exists:


// --- Internal Security Variables ---
volatile bool systemArmed = true;
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
float static_background[CSI_CHANNELS];
float alpha = 0.05;

// --- Implementations ---

void resetSystemState() {  
    Serial.println("System Armed. Waiting for trip...");
    Serial2.println("S"); // Tell the sub-board
    LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0)); 
    LED_RGB.show();
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Working"); lcd.setCursor(0, 1); lcd.print("For now...");
    isAlarmActive = false; 
    systemArmed = true;
}

void disarmSystem() {
    systemArmed = false;
    if (isAlarmActive) { Serial2.println("S"); isAlarmActive = false; }
    LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(255, 165, 0))); 
    LED_RGB.show();
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("System Disarmed"); lcd.setCursor(0, 1); lcd.print("Welcome Home");
}

void triggerSeinagermos(String reason) {
    if(isAlarmActive) return;
    isAlarmActive = true;
    LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); 
    LED_RGB.show();
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("System"); lcd.setCursor(0, 1); lcd.print("Triggered!");
    Serial2.println("A"); // Tell the sub-board to blast the alarm
    
    sendNotification("SECURITY ALERT: " + reason);
    sendTelegramMessage("⚠️ ALARM TRIPPED: " + reason, "");

    Serial.println("Alarm active via: " + reason);
}

// --- CSI Math Functions ---
float calculateGaitScore(int8_t* raw_csi_data, int len) {
    float current_variance = 0;
    int count = 0;
    for (int i = 0; i < (len - 1) && (i / 2) < CSI_CHANNELS; i += 2) {
        int8_t imag = raw_csi_data[i];
        int8_t real = raw_csi_data[i + 1];
        float amplitude = sqrt((imag * imag) + (real * real));
        int channel_idx = i / 2;
        static_background[channel_idx] = (alpha * amplitude) + (1.0 - alpha) * static_background[channel_idx];
        float dynamic_signal = abs(amplitude - static_background[channel_idx]);
        current_variance += dynamic_signal;
        count++;
    }
    float score = (count > 0) ? (current_variance / count) / 20.0 : 0; 
    if (score > 1.0) score = 1.0;
    return score;
}

void csi_rx_callback(void *ctx, wifi_csi_info_t *data) {
    if (!systemArmed) return;
    for (int i = 0; i < MAX_SENSING_APS; i++) {
        if (sensingTable[i].active && memcmp(data->mac, sensingTable[i].mac, 6) == 0) {
            float new_score = calculateGaitScore(data->buf, data->len);
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

void addSensingAP(uint8_t* new_mac) {
    if (activeAPCount >= MAX_SENSING_APS) {
        Serial.println("Mesh Full!");
        return;
    }
    for(int i=0; i < activeAPCount; i++) {
        if(memcmp(sensingTable[i].mac, new_mac, 6) == 0) return;
    }
    memcpy(sensingTable[activeAPCount].mac, new_mac, 6);
    sensingTable[activeAPCount].active = true;
    sensingTable[activeAPCount].lastScore = 0.0;
    activeAPCount++;
    
    Preferences prefs;
    prefs.begin("gait_mesh", false);
    prefs.putInt("ap_count", activeAPCount);
    String key = "mac_" + String(activeAPCount-1);
    prefs.putBytes(key.c_str(), new_mac, 6);
    prefs.end();
    Serial.println("New AP added to Sensing Mesh.");
}

void setupSecurity() {
    // Setup the CSI callback
    wifi_csi_config_t csi_config = {}; 
    esp_err_t csi_err = esp_wifi_set_csi(true);
    if (csi_err == ESP_OK) {
        esp_wifi_set_csi_config(&csi_config);
        esp_wifi_set_csi_rx_cb(csi_rx_callback, NULL);
        Serial.println("Gait Analysis Engine Started.");
    } else {
        Serial.printf("CSI Error: %s\n", esp_err_to_name(csi_err));
    }
}

// --- The RTOS Task ---
void securityLoop(void * pvParameters) {
    extern bool devModeActive; // Pull this from main for now
    
    for(;;) {
        int sensorState = digitalRead(LASER_PIN);
        int buttonState = digitalRead(RESET_BUTTON);
        int soundLevel = analogRead(SOUND_PIN);
        int potValue = analogRead(POT_PIN);

        currentSensitivity = map(potValue, 0, 4095, 100, 2000);
        
        audioDCOffset = (0.005 * soundLevel) + (0.995 * audioDCOffset);
        float currentVolume = abs(soundLevel - audioDCOffset);
        ambientNoise = (0.01 * currentVolume) + (0.99 * ambientNoise);
        float multiplier = webSoundSensitivity / 10.0; 
        float dynamicThreshold = (ambientNoise * multiplier) + 150.0;

        if (devModeActive) {
            Serial.print("Volume:"); Serial.print(currentVolume);
            Serial.print(", Threshold:"); Serial.print(dynamicThreshold);
            Serial.print(", AmbientNoise:"); Serial.println(ambientNoise);
        }

        if (currentVolume > dynamicThreshold) {
            digitalWrite(SOUND_LED_PIN, HIGH);
            isSirenDetected = true;
        } else {
            digitalWrite(SOUND_LED_PIN, LOW);
            isSirenDetected = false;
        }
        
        if (systemArmed && !isAlarmActive) {
            if (sensorState == HIGH) triggerSeinagermos("Laser Tripwire Broken");

            float dynamicCsiThreshold = BASE_THRESHOLD - ((activeAPCount - 1) * SENSITIVITY_BOOST);
            if (dynamicCsiThreshold < MIN_THRESHOLD) dynamicCsiThreshold = MIN_THRESHOLD;

            if (gaitDisturbanceScore > dynamicCsiThreshold) {
                String reason = "Gait Mesh Triggered (" + String(activeAPCount) + " APs)";
                triggerSeinagermos(reason);
            }
        }

        if (buttonState == LOW && !systemArmed && !isAlarmActive) {
            lcd.clear(); lcd.setCursor(0, 0); lcd.print("Arming Mesh...");
            
            // Assuming armingDelaySeconds is in Config.h
            for (int i = 10; i > 0; i--) { 
                lcd.setCursor(0, 1); lcd.print(String(i) + "s | " + String(activeAPCount) + " APs active");
                LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 255, 255))); LED_RGB.show();
                vTaskDelay(500 / portTICK_PERIOD_MS);
                LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 0, 0))); LED_RGB.show();
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
            passiveBuzzerBeep(600); 
            resetSystemState();
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}