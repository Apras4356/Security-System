#include <Arduino.h>
#include <nvs_flash.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <UniversalTelegramBot.h>
#include <esp_now.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <SPI.h>
#include <MFRC522.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <IRsend.h>

#include "Config.h"
#include "Globals.h"
#include "WebPages.h"
#include "QRScanner.h"

const uint16_t IR_LED_PIN = 4; // Change to whatever pin you used for the clear LED
IRsend irsend(IR_LED_PIN);
const uint32_t IR_POWER    = 0x0x8E7C23D;
const uint32_t IR_VOL_UP   = 0x0x8E732CD;
const uint32_t IR_VOL_DOWN = 0x0xFFFFFFFFFFFFFFFF;
const uint32_t IR_MUTE     = 0x0x8E702FD;
const uint32_t IR_PC_IN    = 0x0x8E78A75;
const uint32_t IR_BT_IN    = 0x0x8E76897;
bool devModeActive = false;
int webSoundSensitivity = 50; 
float audioDCOffset = 2048.0;
float ambientNoise = 0.0;
WebServer server(80);
uint8_t router_mac[6];
volatile float gaitDisturbanceScore = 0.0; 
const float BASE_THRESHOLD = 0.85;
const float MIN_THRESHOLD = 0.55;
const float SENSITIVITY_BOOST = 0.08;
SensingAP sensingTable[MAX_SENSING_APS];

int activeAPCount = 1;
float static_background[CSI_CHANNELS];
float alpha = 0.05;

MFRC522 mfrc522[MAX_ZONES];
AccessZone systemZones[MAX_ZONES];
int zoneCount = 0;
unsigned long zoneOpenTimes[MAX_ZONES] = {0};
const unsigned long RELAY_OPEN_DURATION = 3000;

volatile bool isAlarmActive = false;
int silenceOffset = 3500; 
int currentSensitivity = 0;
bool isSirenDetected = false;
volatile bool systemArmed = true;
volatile bool isEnrollMode = false;

String callMeBotUser = "";
String targetSpeaker = "";
String pendingUID = "";
String telegramToken = "";
String telegramChatID = "";

extern void saveCards();
extern void loadCards();

Preferences preferences;

RFIDCard enrolledCards[MAX_CARDS];
int cardCount = 0;
String rfidLogs[MAX_LOGS];
int logIndex = 0;

Adafruit_NeoPixel LED_RGB(1, 48, NEO_GRBW + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClientSecure secured_client;
UniversalTelegramBot* botPtr;
#define bot (*botPtr)

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
uint8_t lastRemoteMac[6];
bool isGreek = false;

volatile float remoteBatteryVoltage = 0.0;
int getBatteryPercentage(float voltage) {
  if (voltage <= 0.0) return 0;
  
  int v = voltage * 100; 
  
  if (v >= 340) return 100;
  if (v >= 330) return map(v, 330, 340, 80, 100);
  if (v >= 320) return map(v, 320, 330, 20, 80);
  if (v >= 300) return map(v, 300, 320, 0, 20);
  
  return 0;
}

float calculateGaitScore(int8_t* raw_csi_data, int len);

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

float calculateGaitScore(int8_t* raw_csi_data, int len) {
    float current_variance = 0;
    int count = 0;

    // Iterate by 2 because data is interleaved I/Q pairs (Imaginary, Real)
    for (int i = 0; i < (len - 1) && (i / 2) < CSI_CHANNELS; i += 2) {
        int8_t imag = raw_csi_data[i];
        int8_t real = raw_csi_data[i + 1];
        
        // Calculate true mathematical magnitude of the subcarrier
        float amplitude = sqrt((imag * imag) + (real * real));
        
        int channel_idx = i / 2;

        // Apply EMA filter
        static_background[channel_idx] = (alpha * amplitude) + (1.0 - alpha) * static_background[channel_idx];
        
        // Find variance
        float dynamic_signal = abs(amplitude - static_background[channel_idx]);

        current_variance += dynamic_signal;
        count++;
    }

    // You may need to adjust the '/ 20.0' divider now that the math is correct
    float score = (count > 0) ? (current_variance / count) / 20.0 : 0; 
    if (score > 1.0) score = 1.0;
    
    return score;
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
    
    preferences.begin("gait_mesh", false);
    preferences.putInt("ap_count", activeAPCount);
    String key = "mac_" + String(activeAPCount-1);
    preferences.putBytes(key.c_str(), new_mac, 6);
    preferences.end();
    
    Serial.println("New AP added to Sensing Mesh.");
}


void passiveBuzzerBeep(int durationMs) {
    digitalWrite(BUZZER_PIN, HIGH);
    vTaskDelay(durationMs / portTICK_PERIOD_MS);
    digitalWrite(BUZZER_PIN, LOW);
}

void logRFIDEvent(String nameOrUID, String status) {
  struct tm timeinfo;
  String timeString = "Time Syncing...";
  if (getLocalTime(&timeinfo)) {
    char timeFmt[30];
    strftime(timeFmt, sizeof(timeFmt), "%d/%m/%Y %H:%M:%S", &timeinfo);
    timeString = String(timeFmt);
  }
  rfidLogs[logIndex] = timeString + " | " + nameOrUID + " | " + status;
  logIndex = (logIndex + 1) % MAX_LOGS;
}

void sendNotification(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    message.replace(" ", "%20");
    String url = "https://api.callmebot.com/text.php?user=" + callMeBotUser + "&text=" + message;
  }
}

void resetSystemState() {  
  Serial.println("System Armed. Waiting for trip...");
  Serial2.println("S");
  LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0)); LED_RGB.show();
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Working"); lcd.setCursor(0, 1); lcd.print("For now...");
  isAlarmActive = false; systemArmed = true;
}

void disarmSystem() {
  systemArmed = false;
  if (isAlarmActive) { Serial2.println("S"); isAlarmActive = false; }
  LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(255, 165, 0))); LED_RGB.show();
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("System Disarmed"); lcd.setCursor(0, 1); lcd.print("Welcome Home");
}

void triggerSeinagermos(String reason) {
  if(isAlarmActive) return;
  isAlarmActive = true;
  LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); LED_RGB.show();
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("System"); lcd.setCursor(0, 1); lcd.print("Triggered!");
  Serial2.println("A");
  sendNotification("SECURITY ALERT: " + reason);
  bot.sendMessage(telegramChatID, "⚠️ ALARM TRIPPED: " + reason, "");
  Serial.println("Alarm active via: " + reason);
}


void openAccessPoint(int zoneID) {
  if (zoneID < 0 || zoneID >= zoneCount) return;
  
  if (systemZones[zoneID].isActive) {
    Serial.println("Unlocking Access Point: " + systemZones[zoneID].name);
    int pin = systemZones[zoneID].relayPin;
    if (pin >= 0) {
      digitalWrite(pin, HIGH);
      zoneOpenTimes[zoneID] = millis();
    }
  }
}

void manageRelays() {
  for (int i = 0; i < zoneCount; i++) {
    if (systemZones[i].isActive && systemZones[i].relayPin >= 0) {
      if (zoneOpenTimes[i] > 0 && (millis() - zoneOpenTimes[i] >= RELAY_OPEN_DURATION)) {
        digitalWrite(systemZones[i].relayPin, LOW);
        zoneOpenTimes[i] = 0;
        Serial.println("Auto-Locking Access Point: " + systemZones[i].name);
      }
    }
  }
}

void loadZones() {
  preferences.begin("sys_zones", true);
  zoneCount = preferences.getInt("zCount", 0);
  
  for (int i = 0; i < MAX_ZONES; i++) {
    String keyBase = "z" + String(i);
    systemZones[i].isActive = preferences.getBool((keyBase + "a").c_str(), false);
    
    if (systemZones[i].isActive) {
      systemZones[i].name = preferences.getString((keyBase + "n").c_str(), "Unnamed");
      systemZones[i].ssPin = preferences.getInt((keyBase + "s").c_str(), -1);
      systemZones[i].relayPin = preferences.getInt((keyBase + "r").c_str(), -1);
      
      if (systemZones[i].ssPin >= 0) {
        mfrc522[i].PCD_Init(systemZones[i].ssPin, LOCAL_RST_PIN);
      }
      if (systemZones[i].relayPin >= 0) {
        pinMode(systemZones[i].relayPin, OUTPUT);
        digitalWrite(systemZones[i].relayPin, LOW);
      }
    }
  }
  preferences.end();
  Serial.println("System Zones Loaded from Memory.");
}

void processCardStatus(String scannedUID, bool isRemote, int zoneID) {
  scannedUID.trim();
  scannedUID.toUpperCase();

  if (isEnrollMode) {
    pendingUID = scannedUID;
    Serial.println("ENROLL MODE: Captured UID " + pendingUID);
    
    passiveBuzzerBeep(100);
    vTaskDelay(150 / portTICK_PERIOD_MS);
    passiveBuzzerBeep(100);
  } else {
    bool isAuthorized = false; 
    bool isRevoked = false; 
    String cardName = "Unknown Card";
    
    for (int i = 0; i < cardCount; i++) {
      if (enrolledCards[i].uid == scannedUID) {
        cardName = enrolledCards[i].name;
        if (enrolledCards[i].isActive && (enrolledCards[i].allowedZones & (1 << zoneID))) {
            isAuthorized = true;
        } else if (!enrolledCards[i].isActive) {
            isRevoked = true;
        }
        break;
      }
    }
    
    if (isAuthorized) {
      if (systemArmed || isAlarmActive) {
        logRFIDEvent(cardName, "AUTHORIZED - Disarmed System");
        disarmSystem();
      } else {
        logRFIDEvent(cardName, "AUTHORIZED - Zone " + String(zoneID));
      }
      
      openAccessPoint(zoneID);
      passiveBuzzerBeep(100);
      
    } else if (isRevoked) {
      logRFIDEvent(cardName, "DENIED (REVOKED)");
      passiveBuzzerBeep(300);
    } else {
      logRFIDEvent("Unknown (" + scannedUID + ")", "DENIED");
      passiveBuzzerBeep(300);
    }
    
    if (isRemote) {
      uint8_t replyMsg = (isAuthorized) ? 1 : 0;
      esp_now_peer_info_t peerInfo = {}; 
      memcpy(peerInfo.peer_addr, lastRemoteMac, 6);
      if (!esp_now_is_peer_exist(lastRemoteMac)) esp_now_add_peer(&peerInfo);
      esp_now_send(lastRemoteMac, &replyMsg, 1);
    }
  }
}


void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(lastRemoteMac, mac, 6);
  memcpy(&incomingRead, incomingData, sizeof(incomingRead));
  remoteBatteryVoltage = incomingRead.voltage;
  strcpy(espNowUID, incomingRead.uid); 
  newRfidAvailable = true;             
  
  Serial.print("ESP-NOW UID: "); Serial.println(espNowUID);
  Serial.print("Remote Battery: "); Serial.println(incomingRead.voltage);

  if (incomingRead.voltage > 0.0 && incomingRead.voltage <= 3.1) {
    bot.sendMessage(telegramChatID, "⚠️ *LOW BATTERY WARNING*\nGate Reader is at " + String(incomingRead.voltage) + "V.\nPlease recharge soon.", "Markdown");
  }

  processCardStatus(String(espNowUID), true, 1);
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


void securityLoop(void * pvParameters) {
  for(;;) {
    int sensorState = digitalRead(LASER_PIN);
    int buttonState = digitalRead(RESET_BUTTON);
    int soundLevel = analogRead(SOUND_PIN);
    int potValue = analogRead(POT_PIN);

    int soundThreshold = map(potValue, 0, 4095, 100, 2000);
    currentSensitivity = soundThreshold;
    
    // 1. Track the exact "Silence" center point (DC Offset).
    // Uses a very slow Exponential Moving Average (EMA) to find the middle of the audio wave.
    audioDCOffset = (0.005 * soundLevel) + (0.995 * audioDCOffset);
    
    // 2. Find the absolute volume of this exact millisecond
    float currentVolume = abs(soundLevel - audioDCOffset);
    
    // 3. Track the ambient background noise.
    // Adapts moderately fast so it learns the room's noise level within seconds.
    ambientNoise = (0.01 * currentVolume) + (0.99 * ambientNoise);

    // 4. Calculate the Dynamic Threshold based on the Web Slider
    // A slider value of 50 means "trigger when noise is 5.0x louder than the background hum"
    float multiplier = webSoundSensitivity / 10.0; 
    
    // Add +150 as a hard absolute floor so absolute pin-drop silence doesn't cause a divide-by-zero false alarm
    float dynamicThreshold = (ambientNoise * multiplier) + 150.0;

    if (currentVolume > dynamicThreshold) {
      digitalWrite(SOUND_LED_PIN, HIGH);
      isSirenDetected = true;
    } else {
      digitalWrite(SOUND_LED_PIN, LOW);
      isSirenDetected = false;
    }

    // --- DEVELOPER MODE: SERIAL PLOTTER OUTPUT ---
    if (devModeActive) {
      Serial.print("Volume:");
      Serial.print(currentVolume);
      Serial.print(", Threshold:");
      Serial.print(dynamicThreshold);
      Serial.print(", AmbientNoise:");
      Serial.println(ambientNoise);
    }

    if (currentVolume > dynamicThreshold) {
      digitalWrite(SOUND_LED_PIN, HIGH);
      isSirenDetected = true;
    } else {
      digitalWrite(SOUND_LED_PIN, LOW);
      isSirenDetected = false;
    }
    
    if (systemArmed && !isAlarmActive) {
      if (sensorState == HIGH) {
        triggerSeinagermos("Laser Tripwire Broken");
      }

      float dynamicThreshold = BASE_THRESHOLD - ((activeAPCount - 1) * SENSITIVITY_BOOST);
      if (dynamicThreshold < MIN_THRESHOLD) dynamicThreshold = MIN_THRESHOLD;

      if (gaitDisturbanceScore > dynamicThreshold) {
        String reason = "Gait Mesh Triggered (" + String(activeAPCount) + " APs)";
        triggerSeinagermos(reason);
      }
    }

    if (buttonState == LOW && !systemArmed && !isAlarmActive) {
      lcd.clear(); 
      lcd.setCursor(0, 0); 
      lcd.print("Arming Mesh...");
      
      
      for (int i = armingDelaySeconds; i > 0; i--) {
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

void communicationLoop(void * pvParameters) {
  for(;;) {
    pollSmartBulb();      
    digitalWrite(WIFI_LED, WiFi.status() == WL_CONNECTED);
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

void telegramLoop(void * pvParameters) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      
      while(numNewMessages) {
        for (int i = 0; i < numNewMessages; i++) {
          String text = bot.messages[i].text;
          String id = bot.messages[i].chat_id;
          
          if (id != telegramChatID) continue;

          if (text == "/status") {
            String st = systemArmed ? "🟢 ARMED" : "🟠 DISARMED";
            if (isAlarmActive) st = "🔴 ALARM TRIGGERED!";
            
            String battStr = "Waiting for ESP-NOW data...";
            if (remoteBatteryVoltage > 0.0) {
              int pct = getBatteryPercentage(remoteBatteryVoltage);
              battStr = String(remoteBatteryVoltage) + "V (" + String(pct) + "%)";
              if (pct < 15) battStr += " ⚠️ LOW BATT";
            }
            bot.sendMessage(id, "*System Status:* " + st + "\n*Gate Battery:* " + battStr, "Markdown");
          } 
          else if (text == "/arm") {
            if (!systemArmed) {
              resetSystemState(); 
              bot.sendMessage(id, "🔒 *System Armed.* The mesh is now active.", "Markdown");
            } else {
              bot.sendMessage(id, "ℹ️ System is already armed.", "");
            }
          } 
          else if (text == "/disarm") {
            if (systemArmed || isAlarmActive) {
              disarmSystem(); 
              bot.sendMessage(id, "🔓 *System Disarmed.* Welcome home.", "Markdown");
            } else {
              bot.sendMessage(id, "ℹ️ System is already disarmed.", "");
            }
          } 
          else if (text == "/logs") {
            String logMsg = "📋 *Recent System Logs:*\n\n";
            bool hasLogs = false;
            for (int j = 0; j < MAX_LOGS; j++) {
              int idx = (logIndex - 1 - j + MAX_LOGS) % MAX_LOGS;
              if (rfidLogs[idx] != "" && rfidLogs[idx].length() > 2) {
                hasLogs = true;
                logMsg += "• `" + rfidLogs[idx] + "`\n";
              }
            }
            if (!hasLogs) logMsg += "_No logs recorded yet._";
            bot.sendMessage(id, logMsg, "Markdown");
          }
          else if (text == "/cards") {
            String msg = "💳 *Enrolled RFID Cards:*\n\n";
            if (cardCount == 0) {
              msg += "_No cards enrolled yet._";
            } else {
              for (int c = 0; c < cardCount; c++) {
                msg += "👤 *" + enrolledCards[c].name + "*\n";
                msg += "└ UID: `" + enrolledCards[c].uid + "`\n";
                
                String zNames = "";
                bool first = true;
                for(int z = 0; z < zoneCount; z++) {
                  if(systemZones[z].isActive && (enrolledCards[c].allowedZones & (1 << z))) {
                    if(!first) zNames += ", ";
                    zNames += systemZones[z].name;
                    first = false;
                  }
                }
                if(zNames == "") zNames = "None";
                msg += "└ Zones: " + zNames + "\n\n";
              }
            }
            bot.sendMessage(id, msg, "Markdown");
          }
          else if (text == "/zones") {
            String msg = "🚪 *Active Access Zones:*\n\n";
            bool hasZones = false;
            for (int z = 0; z < MAX_ZONES; z++) {
              if (z < zoneCount && systemZones[z].isActive) {
                hasZones = true;
                msg += "📍 *Zone " + String(z) + ": " + systemZones[z].name + "*\n";
                msg += "└ Reader SS Pin: `" + String(systemZones[z].ssPin) + "`\n";
                msg += "└ Relay Pin: `" + String(systemZones[z].relayPin) + "`\n\n";
              }
            }
            if (!hasZones) msg += "_No zones defined yet._";
            bot.sendMessage(id, msg, "Markdown");
          }
          else if (text == "/help" || text == "/cmds" || text == "/start") {
          String helpMsg = "🤖 *Security Bot Commands:*\n";
          helpMsg += "Tap a button below, or type a command:\n";
          helpMsg += "• /status - Check system & battery\n";
          helpMsg += "• /arm - Arm the system\n";
          helpMsg += "• /disarm - Disarm the system\n";
          helpMsg += "• /logs - View recent events\n";
          helpMsg += "• /cards - List authorized users\n";
          helpMsg += "• /zones - List hardware zones";
          
          String keyboardJson = "[[\"/status\", \"/logs\"], [\"/arm\", \"/disarm\"], [\"/cards\", \"/zones\"]]";

          bot.sendMessageWithReplyKeyboard(id, helpMsg, "Markdown", keyboardJson, true);
          }
          else {
            bot.sendMessage(id, "I didn't understand that. Type /help to see the menu.", "");
          }
        }
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
    }
    vTaskDelay(1500 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  server.begin();
  irsend.begin();

  // 1. Initialize Memory
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
  
  loadZones(); 
  lcd.init(); 
  lcd.backlight();
  loadCards();
  
  // 3. Load Preferences
  preferences.begin("sys_prefs", true);
  webSoundSensitivity = preferences.getInt("mic_sens", 50); 
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
  botPtr = new UniversalTelegramBot(telegramToken, secured_client);
  WiFiManager wm;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");

  bool res = wm.autoConnect("SecuritySystem-Setup", "12345678");

  if(!res) {
    Serial.println("Failed to connect and hit timeout");
    lcd.clear(); lcd.setCursor(0,0); lcd.print("WiFi Failed");
    delay(3000);
    ESP.restart();
  } 

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // --- REVERTED BACK TO HTTP ---
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Starting Server");
  
  
  // Register your nodes
  setupWebHandlers();
  setupQRHandlers();
  
  // Start the HTTP Server
  Serial.println("HTTP Server started on port 80.");
  // --------------------------------

  // 5. Advanced System Initializations
  uint8_t* bssid = WiFi.BSSID();
  if (bssid != NULL) {
      memcpy(router_mac, bssid, 6);
      memcpy(sensingTable[0].mac, bssid, 6);
      sensingTable[0].active = true;
      activeAPCount = 1;
  }

  wifi_csi_config_t csi_config = {}; 
  esp_err_t csi_err = esp_wifi_set_csi(true);
  if (csi_err == ESP_OK) {
      esp_wifi_set_csi_config(&csi_config);
      esp_wifi_set_csi_rx_cb(csi_rx_callback, NULL);
      Serial.println("Gait Analysis Engine Started.");
  } else {
      Serial.printf("CSI Error: %s\n", esp_err_to_name(csi_err));
  }
  
  lcd.clear(); 
  lcd.setCursor(0,0); 
  lcd.print("WiFi + CSI Ready"); 

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    Serial.println("ESP-NOW Initialized");
    esp_now_register_recv_cb(OnDataRecv);
  }

  secured_client.setInsecure();
  configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org");
  MDNS.begin("seinagermos");

  // 6. Start RTOS Tasks
  xTaskCreatePinnedToCore(securityLoop, "SecTask", 10000, NULL, 1, &SecurityTask, 1);
  xTaskCreatePinnedToCore(communicationLoop, "CommsTask", 10000, NULL, 1, &CommunicationTask, 0);
  xTaskCreatePinnedToCore(telegramLoop, "TelegramTask", 10000, NULL, 1, &TelegramTask, 0);

  resetSystemState();
}

void loop() {
  
  manageRelays();

  for (int i = 0; i < zoneCount; i++) {
    if (systemZones[i].isActive && systemZones[i].ssPin >= 0) {
      
      if (mfrc522[i].PICC_IsNewCardPresent() && mfrc522[i].PICC_ReadCardSerial()) {
        String scannedUID = "";
        for (byte j = 0; j < mfrc522[i].uid.size; j++) {
          scannedUID += String(mfrc522[i].uid.uidByte[j] < 0x10 ? "0" : "");
          scannedUID += String(mfrc522[i].uid.uidByte[j], HEX);
        }
        scannedUID.toUpperCase();
        
        Serial.println("Card tapped at " + systemZones[i].name + " | UID: " + scannedUID);

        processCardStatus(scannedUID, false, i);
        
        mfrc522[i].PICC_HaltA();
        mfrc522[i].PCD_StopCrypto1();
      }
    }
  }
  vTaskDelay(50 / portTICK_PERIOD_MS); 
}