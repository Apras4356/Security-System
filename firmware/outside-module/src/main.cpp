#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // <-- Required to change Wi-Fi channels
#include <SPI.h>
#include <MFRC522.h>

// --- PINS (Adjust to your wiring) ---
#define SS_PIN 21
#define RST_PIN 22
#define TOUCH_PIN 4      // Pin connected to TTP223B I/O
#define BATT_PIN 34      // Pin connected to the middle of the 100k voltage divider
#define BUZZER_PIN 25    // Passive buzzer pin

MFRC522 rfid(SS_PIN, RST_PIN);

// --- RTC MEMORY (Survives Deep Sleep!) ---
// We default to the Broadcast Address
RTC_DATA_ATTR uint8_t mainMacAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
RTC_DATA_ATTR uint8_t mainChannel = 1;
RTC_DATA_ATTR bool isPaired = false;
RTC_DATA_ATTR uint32_t msgCounter = 0;

#define ESPNOW_SECRET_KEY "SecSys_ESPNow_#99!"

// --- DATA STRUCTURES ---
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

struct_message myData;

// --- TWO-WAY VARIABLES ---
volatile bool replyReceived = false;
volatile bool accessGranted = false;

// Callback: Triggers when the Main Board replies
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (len > 0) {
        accessGranted = (incomingData[0] == 1); 
        replyReceived = true;
        
        // LEARN THE MAIN BOARD'S MAC AUTOMATICALLY
        memcpy(mainMacAddress, mac, 6);
    }
}

// --- PASSIVE BUZZER MELODIES ---
void playGoodSound() {
    tone(BUZZER_PIN, 1000, 100); delay(150);
    tone(BUZZER_PIN, 1500, 200); delay(250);
}

void playBadSound() {
    tone(BUZZER_PIN, 300, 300); delay(350);
    tone(BUZZER_PIN, 200, 400); delay(450);
}

void goToSleep() {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_PIN, 1); 
    rfid.PCD_AntennaOff(); 
    Serial.println("Going to sleep now...");
    delay(100); 
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // 1. Wake up reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("First boot or reset. Going to sleep...");
        goToSleep();
    }

    // 2. Initialize Hardware
    SPI.begin();
    rfid.PCD_Init();
    
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        goToSleep();
    }
    esp_now_register_recv_cb(OnDataRecv);

    // 3. Wait for RFID Scan
    Serial.println("Awake! Waiting for card...");
    unsigned long wakeTime = millis();
    bool cardScanned = false;
    String scannedUID = "";

    while (millis() - wakeTime < 5000) {
        if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
            for (byte i = 0; i < rfid.uid.size; i++) {
                scannedUID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
                scannedUID += String(rfid.uid.uidByte[i], HEX);
            }
            scannedUID.trim();
            scannedUID.toUpperCase();
            cardScanned = true;
            tone(BUZZER_PIN, 2500, 50); // Instant chirp
            break; 
        }
        delay(10);
    }

    // 4. Send and Wait for Verdict
    if (cardScanned) {
        int rawAdc = analogRead(BATT_PIN);
        myData.voltage = (rawAdc / 4095.0) * 3.3 * 2.0;
        scannedUID.toCharArray(myData.uid, 30);
        msgCounter++;
        myData.counter = msgCounter;
        myData.authHash = calculateAuthHash(myData.uid, myData.counter);

        esp_now_peer_info_t peerInfo = {};
        peerInfo.encrypt = false;

        // --- ATTEMPT FAST SEND IF ALREADY PAIRED ---
        if (isPaired) {
            esp_wifi_set_channel(mainChannel, WIFI_SECOND_CHAN_NONE);
            memcpy(peerInfo.peer_addr, mainMacAddress, 6);
            esp_now_add_peer(&peerInfo);
            
            esp_now_send(mainMacAddress, (uint8_t *) &myData, sizeof(myData));
            
            unsigned long waitStart = millis();
            while (!replyReceived && (millis() - waitStart < 1500)) delay(10);
            
            if (!replyReceived) {
                Serial.println("Saved connection failed. Router might have changed channels. Re-hunting...");
                isPaired = false; // Fallback to hunting mode
            }
        }

        // --- HUNTING MODE (Auto-Discovery) ---
        if (!isPaired) {
            uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            memcpy(peerInfo.peer_addr, broadcastMac, 6);
            
            for (int ch = 1; ch <= 13; ch++) {
                esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                peerInfo.channel = ch;
                
                if (esp_now_is_peer_exist(broadcastMac)) esp_now_del_peer(broadcastMac);
                esp_now_add_peer(&peerInfo);

                esp_now_send(broadcastMac, (uint8_t *) &myData, sizeof(myData));
                
                // Wait 200ms per channel for the main board to reply
                unsigned long waitStart = millis();
                while (!replyReceived && (millis() - waitStart < 200)) delay(10);
                
                if (replyReceived) {
                    Serial.printf("Main Panel found on Channel %d!\n", ch);
                    mainChannel = ch;
                    isPaired = true;
                    break; // Stop hunting!
                }
            }
        }

        // Evaluate the final response
        if (replyReceived) {
            if (accessGranted) playGoodSound();
            else playBadSound();
        } else {
            Serial.println("Timeout: Main Panel didn't reply.");
            playBadSound(); 
        }
    }

    // 5. Go back to sleep
    goToSleep();
}

void loop() { vTaskDelete(NULL); }