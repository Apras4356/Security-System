#include "RFIDManager.h"
#include <Preferences.h>
#include <esp_now.h>

// --- Link to Main System Functions & Variables ---
extern void passiveBuzzerBeep(int durationMs);
extern void disarmSystem();
extern volatile bool systemArmed;
extern volatile bool isAlarmActive;
extern uint8_t lastRemoteMac[6];

// --- Internal RFID Variables ---
MFRC522 mfrc522[MAX_ZONES];
AccessZone systemZones[MAX_ZONES];
int zoneCount = 0;
unsigned long zoneOpenTimes[MAX_ZONES] = {0};
const unsigned long RELAY_OPEN_DURATION = 3000;

RFIDCard enrolledCards[MAX_CARDS];
int cardCount = 0;
String rfidLogs[MAX_LOGS];
int logIndex = 0;

volatile bool isEnrollMode = false;
String pendingUID = "";

// --- Implementations ---

void setupRFID() {
    loadZones();
    loadCards(); // Assuming you have this written somewhere, otherwise define it here!
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
    Preferences prefs;
    prefs.begin("sys_zones", true);
    zoneCount = prefs.getInt("zCount", 0);
    
    for (int i = 0; i < MAX_ZONES; i++) {
        String keyBase = "z" + String(i);
        systemZones[i].isActive = prefs.getBool((keyBase + "a").c_str(), false);
        
        if (systemZones[i].isActive) {
            systemZones[i].name = prefs.getString((keyBase + "n").c_str(), "Unnamed");
            systemZones[i].ssPin = prefs.getInt((keyBase + "s").c_str(), -1);
            systemZones[i].relayPin = prefs.getInt((keyBase + "r").c_str(), -1);
            
            if (systemZones[i].ssPin >= 0) {
                // Assuming LOCAL_RST_PIN is in Config.h
                mfrc522[i].PCD_Init(systemZones[i].ssPin, LOCAL_RST_PIN); 
            }
            if (systemZones[i].relayPin >= 0) {
                pinMode(systemZones[i].relayPin, OUTPUT);
                digitalWrite(systemZones[i].relayPin, LOW);
            }
        }
    }
    prefs.end();
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

void initZoneReader(int slot) {
    if (slot >= 0 && slot < MAX_ZONES && systemZones[slot].isActive) {
        if (systemZones[slot].ssPin >= 0) {
            // Initialize the newly saved reader!
            mfrc522[slot].PCD_Init(systemZones[slot].ssPin, LOCAL_RST_PIN);
            Serial.println("Initialized new RFID reader for slot: " + String(slot));
        }
    }
}

// This replaces the big RFID chunk inside your main loop()!
void handleRFID() {
    manageRelays(); // Handle the auto-locking

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
}