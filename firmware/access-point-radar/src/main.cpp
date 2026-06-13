#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>

const char* brainIP = "255.255.255.255"; // Broadcast so the Brain can hear it anywhere
WiFiUDP udp;
unsigned long lastPing = 0;

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
}

void loop() {
  // Send a packet every 50ms (20Hz)
  if (millis() - lastPing > 50) {
    udp.beginPacket(brainIP, 8888);
    udp.write((const uint8_t*)"PING", 4);
    udp.endPacket();
    lastPing = millis();
  }
}