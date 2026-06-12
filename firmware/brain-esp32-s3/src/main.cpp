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


bool devModeActive = false;
WebServer server(80);
uint8_t router_mac[6];

extern void saveCards();
extern void loadCards();
bool laserEnabled = true;

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
} struct_message;

struct_message incomingRead;
volatile bool newRfidAvailable = false;
char espNowUID[30] = "";
volatile int pendingArmTimer = -1;
uint8_t lastRemoteMac[6];
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
  memcpy(lastRemoteMac, mac, 6);
  memcpy(&incomingRead, incomingData, sizeof(incomingRead));
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
      triggerSeinagermos("MANUAL TEST: Virtual Bulb Trip");
    }
  }
}

void communicationLoop(void *pvParameters) {
  for (;;) {
    pollSmartBulb();
    digitalWrite(WIFI_LED, WiFi.status() == WL_CONNECTED);
    server.handleClient();
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
  telegramToken = preferences.getString("bot_token", BOT_TOKEN);
  telegramChatID = preferences.getString("chat_id", CHAT_ID);
  callMeBotUser = preferences.getString("cmb_user", "");
  targetSpeaker = preferences.getString("bt_speaker", "");
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

  bool res = wm.autoConnect("SecuritySystem-Setup", "12345678");

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

  vTaskDelay(50 / portTICK_PERIOD_MS);
}