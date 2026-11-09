#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ESPmDNS.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include <esp_wifi_types.h>
#include <nvs_flash.h>
#include <time.h>

#include "Config.h"
#include "Globals.h"
#include "IRManager.h"
#include "QRScanner.h"
#include "RFIDManager.h"
#include "SecurityManager.h"
#include "TelegramManager.h"
#include "WebPages.h"
#include "GoogleHomeManager.h"

bool devModeActive = false;
WebServer server(80);
uint8_t router_mac[6];
WiFiUDP stateUdp;

extern void saveCards();
extern void loadCards();
bool laserEnabled = true;
bool smokeEnabled = true;
bool wifiDetectionEnabled = true;

String autoArmTime = "";
String autoDisarmTime = "";
bool autoScheduleEnabled = false;
bool autoArmAllDay = false;
uint8_t activeWeekdays = 0;
String specificArmDates = "";
int lastAutoArmDay = -1;
int lastAutoDisarmDay = -1;

Preferences preferences;

Adafruit_NeoPixel LED_RGB(1, 48, NEO_GRBW + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27, 16, 2);

TaskHandle_t SecurityTask;
TaskHandle_t CommunicationTask;
TaskHandle_t TelegramTask;
unsigned long lastBotCheck = 0;

typedef struct struct_message {
  char uid[30];
  float voltage;
  uint32_t counter;
  uint32_t authHash;
} struct_message;

inline uint32_t calculateAuthHash(const char* uid, uint32_t counter) {
  uint32_t hash = 2166136261u;
  const char* secret = ESPNOW_SECRET_KEY;
  while (*secret) {
    hash ^= (uint8_t)*secret++;
    hash *= 16777619u;
  }
  const char* p = uid;
  while (*p) {
    hash ^= (uint8_t)*p++;
    hash *= 16777619u;
  }
  uint8_t* c = (uint8_t*)&counter;
  for (int i = 0; i < 4; i++) {
    hash ^= c[i];
    hash *= 16777619u;
  }
  return hash;
}

struct_message incomingRead;
volatile bool newRfidAvailable = false;
char espNowUID[30] = "";
volatile int pendingArmTimer = -1;
int armingDelaySeconds = 10;
String sinricAppKey = "";
String sinricAppSecret = "";
String sinricDeviceId = "";
String webAdminUser = "admin";
String webAdminPass = "admin";
uint8_t lastRemoteMac[6];
uint8_t pairedOutsideMac[6] = {0};
bool hasPairedOutsideMac = false;
uint32_t lastRemoteCounter = 0;
bool isGreek = false;

volatile float remoteBatteryVoltage = 0.0;
volatile bool triggerLowBatteryWarning = false;
volatile float lowBatteryVoltage = 0.0;
int getBatteryPercentage(float voltage) {
  if (voltage <= 0.0)
    return 0;

  int v = voltage * 100;

  if (v >= 340)
    return 100;
  if (v >= 330)
    return map(v, 330, 340, 80, 100);
  if (v >= 320)
    return map(v, 320, 330, 20, 80);
  if (v >= 300)
    return map(v, 300, 320, 0, 20);

  return 0;
}

void passiveBuzzerBeep(int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  vTaskDelay(durationMs / portTICK_PERIOD_MS);
  digitalWrite(BUZZER_PIN, LOW);
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len < sizeof(struct_message)) return;

  struct_message tempMsg;
  memcpy(&tempMsg, incomingData, sizeof(tempMsg));

  // 1. Authenticate packet signature
  if (tempMsg.authHash != calculateAuthHash(tempMsg.uid, tempMsg.counter)) {
    Serial.println("⛔ ESP-NOW: Rejected packet with invalid auth signature!");
    return;
  }

  // 2. Replay attack protection (counter must increase; allow 1 for fresh reboot)
  if (tempMsg.counter <= lastRemoteCounter && tempMsg.counter != 1) {
    Serial.printf("⛔ ESP-NOW: Rejected replayed packet! Counter: %u <= %u\n", tempMsg.counter, lastRemoteCounter);
    return;
  }
  lastRemoteCounter = tempMsg.counter;

  // 3. Sender MAC verification
  if (hasPairedOutsideMac) {
    if (memcmp(mac, pairedOutsideMac, 6) != 0) {
      Serial.printf("⛔ ESP-NOW: Rejected packet from unauthorized MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      return;
    }
  } else {
    // Auto-pair with first authenticated packet
    memcpy(pairedOutsideMac, mac, 6);
    hasPairedOutsideMac = true;
    preferences.begin("sys_prefs", false);
    preferences.putBytes("out_mac", pairedOutsideMac, 6);
    preferences.end();
    Serial.printf("✅ ESP-NOW: Paired with Outside Module MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  memcpy(lastRemoteMac, mac, 6);
  incomingRead = tempMsg;
  remoteBatteryVoltage = incomingRead.voltage;
  strlcpy(espNowUID, incomingRead.uid, sizeof(espNowUID));
  newRfidAvailable = true;

  // Just set the flags here, do NOT send the message!
  if (incomingRead.voltage > 0.0 && incomingRead.voltage <= 3.1) {
    triggerLowBatteryWarning = true;
    lowBatteryVoltage = incomingRead.voltage;
  }
}

void pollSmartBulb() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "T" || command == "t") {
      triggerSeinagermos("MANUAL TEST: PC Serial Trip");
    }
  }
  if (Serial2.available() > 0) {
    String command = Serial2.readStringUntil('\n');
    command.trim();
    if (command == "T" || command == "t") {
      triggerSeinagermos("MANUAL TEST: Mouth ESP32 Button");
    }
  }
}

void communicationLoop(void *pvParameters) {
  unsigned long lastStateBroadcast = 0;
  for (;;) {
    pollSmartBulb();
    digitalWrite(WIFI_LED, WiFi.status() == WL_CONNECTED);
    server.handleClient();

    if (millis() - lastStateBroadcast > 1000) {
      stateUdp.beginPacket("255.255.255.255", 8889);
      if (systemArmed) {
        stateUdp.print("STATE:ARMED:" RADAR_UDP_TOKEN);
      } else {
        stateUdp.print("STATE:DISARMED:" RADAR_UDP_TOKEN);
      }
      stateUdp.endPacket();
      lastStateBroadcast = millis();
    }
    
    static unsigned long lastScheduleCheck = 0;
    if (autoScheduleEnabled && millis() - lastScheduleCheck > 10000) {
      lastScheduleCheck = millis();
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char dateStr[6];
        strftime(dateStr, sizeof(dateStr), "%d/%m", &timeinfo);
        String todayDate = String(dateStr);
        
        bool isSpecificDate = (specificArmDates.indexOf(todayDate) >= 0);
        
        // Map timeinfo.tm_wday (0=Sun, 1=Mon) to our bitmask (bit0=Mon ... bit6=Sun)
        int ourDay = (timeinfo.tm_wday == 0) ? 6 : timeinfo.tm_wday - 1;
        bool isDayEnabled = (activeWeekdays & (1 << ourDay)) > 0;
        
        char timeStr[6];
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
        String nowTime = String(timeStr);
        
        if (isSpecificDate || isDayEnabled) {
          if (autoArmTime != "" && nowTime == autoArmTime && lastAutoArmDay != timeinfo.tm_yday) {
            lastAutoArmDay = timeinfo.tm_yday;
            if (!systemArmed && !isAlarmActive) {
              Serial.println("AUTO ARM TRIGGERED");
              pendingArmTimer = 1;
            }
          }
          if (!isSpecificDate && !autoArmAllDay && autoDisarmTime != "" && nowTime == autoDisarmTime && lastAutoDisarmDay != timeinfo.tm_yday) {
            lastAutoDisarmDay = timeinfo.tm_yday;
            if (systemArmed) {
              Serial.println("AUTO DISARM TRIGGERED");
              disarmSystem();
            }
          }
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  if(!LittleFS.begin(true)){
    Serial.println("LittleFS Mount Failed");
  }

  // 2. Hardware Setup
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  pinMode(LASER_PIN, INPUT);
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SOUND_LED_PIN, OUTPUT);
  digitalWrite(SOUND_LED_PIN, LOW);
  pinMode(WIFI_LED, OUTPUT);

  analogReadResolution(12);

  LED_RGB.begin();
  LED_RGB.setBrightness(150);
  Wire.begin(I2C_SDA, I2C_SCL);
  SPI.begin();

  setupRFID();
  lcd.init();
  lcd.backlight();

  // 3. Load Preferences
  preferences.begin("sys_prefs", true);
  webSoundSensitivity = preferences.getInt("mic_sens", 50);
  laserEnabled = preferences.getBool("laser_en", true);
  smokeEnabled = preferences.getBool("smoke_en", true);
  wifiDetectionEnabled = preferences.getBool("wifi_det", true);
  telegramToken = preferences.getString("bot_token", BOT_TOKEN);
  telegramChatID = preferences.getString("chat_id", CHAT_ID);
  callMeBotUser = preferences.getString("cmb_user", "");
  targetSpeaker = preferences.getString("bt_speaker", "");
  armingDelaySeconds = preferences.getInt("arm_delay", 10);
  sinricAppKey = preferences.getString("sinric_key", "");
  sinricAppSecret = preferences.getString("sinric_sec", "");
  sinricDeviceId = preferences.getString("sinric_dev", "");
  webAdminUser = preferences.getString("web_user", "admin");
  webAdminPass = preferences.getString("web_pass", "admin");
  if (preferences.getBytes("out_mac", pairedOutsideMac, 6) == 6) {
    hasPairedOutsideMac = true;
    Serial.printf("Loaded Paired Outside Module MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  pairedOutsideMac[0], pairedOutsideMac[1], pairedOutsideMac[2],
                  pairedOutsideMac[3], pairedOutsideMac[4], pairedOutsideMac[5]);
  }
  autoArmTime = preferences.getString("auto_arm", "");
  autoDisarmTime = preferences.getString("auto_disarm", "");
  autoScheduleEnabled = preferences.getBool("auto_sch_en", false);
  autoArmAllDay = preferences.getBool("auto_allday", false);
  activeWeekdays = preferences.getUChar("active_wd", 0);
  specificArmDates = preferences.getString("spec_dates", "");
  preferences.end();

  delay(1000);
  if (targetSpeaker != "") {
    Serial2.println("N:" + targetSpeaker);
  }

  // 4. WiFi Connection
  WiFiManager wm;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  bool res = wm.autoConnect("SecuritySystem-Setup", "SecuredESP32Setup!");

  if (!res) {
    Serial.println("Failed to connect and hit timeout");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    delay(3000);
    ESP.restart();
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  setupGoogleHome();

  // --- REVERTED BACK TO HTTP ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting Server");

  // Register your nodes
  setupTelegram();
  setupWebHandlers();
  setupQRHandlers();

  // Start the HTTP Server
  Serial.println("HTTP Server started on port 80.");
  server.begin();
  // --------------------------------

  // 5. Advanced System Initializations
  uint8_t *bssid = WiFi.BSSID();
  if (bssid != NULL) {
    memcpy(router_mac, bssid, 6);
    memcpy(sensingTable[0].mac, bssid, 6);
    sensingTable[0].active = true;
    activeAPCount = 1;
  }

  setupSecurity();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi + CSI Ready");
  setupIR();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    Serial.println("ESP-NOW Initialized");
    esp_now_register_recv_cb(OnDataRecv);
  }

  configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org");
  MDNS.begin("seinagermos");

  // 6. Start RTOS Tasks
  xTaskCreatePinnedToCore(securityLoop, "SecTask", 10000, NULL, 1,
                          &SecurityTask, 1);
  xTaskCreatePinnedToCore(communicationLoop, "CommsTask", 10000, NULL, 1,
                          &CommunicationTask, 0);
  xTaskCreatePinnedToCore(telegramLoop, "TelegramTask", 10000, NULL, 1,
                          &TelegramTask, 1);

  resetSystemState();
}

void loop() {
  // Process ESP-NOW RFID scans from the Outside Module
  if (newRfidAvailable) {
    newRfidAvailable = false;
    processCardStatus(String(espNowUID), true, 0);
  }

  handleRFID();
  handleIRLearning();
  loopGoogleHome();

  vTaskDelay(50 / portTICK_PERIOD_MS);
}