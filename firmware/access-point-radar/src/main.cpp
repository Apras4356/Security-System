#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>

const char* brainIP = "255.255.255.255"; // Broadcast so the Brain can hear it anywhere
WiFiUDP udp;
WiFiUDP stateUdp;
unsigned long lastPing = 0;
unsigned long lastArmedSignal = 0;
bool isArmed = false;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial monitor time to open

  Serial.println("\n--- Starting Radar Access Point Node ---");

  // Use WiFiManager so you don't have to hardcode passwords
  WiFiManager wm;
  
  // wm.resetSettings(); // Un-comment this to wipe saved Wi-Fi credentials
  
  Serial.println("Connecting to Wi-Fi...");
  bool res = wm.autoConnect("RadarNode-Setup", "12345678");

  if (!res) {
    Serial.println("Failed to connect to Wi-Fi! Rebooting...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("📡 CRITICAL: My MAC Address is: ");
  Serial.println(WiFi.macAddress());
  
  udp.begin(8888);
  stateUdp.begin(8889);
}

void loop() {
  int packetSize = stateUdp.parsePacket();
  if (packetSize) {
    char packetBuffer[255];
    int len = stateUdp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    String msg = String(packetBuffer);
    if (msg == "STATE:ARMED") {
      lastArmedSignal = millis();
      if (!isArmed) {
        Serial.println("System ARMED. Starting 20Hz radar PING...");
        isArmed = true;
      }
    } else if (msg == "STATE:DISARMED") {
      if (isArmed) {
        Serial.println("System DISARMED. Entering low-power standby mode...");
      }
      isArmed = false;
    }
  }

  // If we haven't heard from Brain in 5 seconds, assume it's offline/unplugged
  if (isArmed && (millis() - lastArmedSignal > 5000)) {
    Serial.println("Lost heartbeat from Brain. Entering low-power standby mode...");
    isArmed = false;
  }

  if (isArmed) {
    // Send a packet every 50ms (20Hz)
    if (millis() - lastPing > 50) {
      udp.beginPacket(brainIP, 8888);
      udp.write((const uint8_t*)"PING", 4);
      udp.endPacket();
      lastPing = millis();
    }
  } else {
    // Save CPU cycles and cool down the Wi-Fi modem while disarmed
    delay(100);
  }
}