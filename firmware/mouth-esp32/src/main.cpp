#include <Adafruit_NeoPixel.h>
#include <BluetoothA2DPSource.h>
#include <Preferences.h>
#include "seinagermos.h"

#define RX2_PIN 16
#define TX2_PIN 17

volatile bool isAlarmTriggered = false;
uint32_t current_pcm_index = 0;
String targetSpeaker = "";

// Flags and timers for safely updating LEDs outside of the Bluetooth callback
volatile bool newlyConnected = false;
unsigned long ledTurnOffTime = 0;
bool isLedGreen = false;

Adafruit_NeoPixel LED_RGB(1, 22, NEO_GRBW + NEO_KHZ800);
BluetoothA2DPSource a2dp_source;
Preferences preferences;

int32_t get_audio_data(Frame *data, int32_t frameCount) {
  if (!isAlarmTriggered) {
    memset(data, 0, frameCount * sizeof(Frame));
    return frameCount; 
  }

  uint32_t total_frames = seinagermos_len / 4;

  for (int i = 0; i < frameCount; i++) {
    uint32_t byte_idx = current_pcm_index * 4;
    
    data[i].channel1 = (int16_t)(seinagermos[byte_idx] | (seinagermos[byte_idx + 1] << 8));
    data[i].channel2 = (int16_t)(seinagermos[byte_idx + 2] | (seinagermos[byte_idx + 3] << 8));
    
    current_pcm_index++;

    if (current_pcm_index >= total_frames) {
      current_pcm_index = 0;
    }
  }
  return frameCount;
}

// Callback is now lightning fast and non-blocking
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    newlyConnected = true; // Flag the main loop to handle the LED
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    Serial.println(">>> BLUETOOTH DISCONNECTED! Scanning... <<<");
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  
  preferences.begin("audio_prefs", false);
  targetSpeaker = preferences.getString("speaker", "");
  preferences.end();

  LED_RGB.begin(); 
  LED_RGB.clear();
  LED_RGB.show();

  Serial.println("\n--- MOUTH BOARD BOOTING ---");
  Serial.println("Saved Speaker Name: [" + targetSpeaker + "]");

  a2dp_source.set_on_connection_state_changed(connection_state_changed);

  if (targetSpeaker != "") {
    Serial.println("Starting Bluetooth stack...");
    a2dp_source.start(targetSpeaker.c_str(), get_audio_data);
  } else {
    Serial.println("No speaker name saved yet! Waiting for command from Brain...");
  }
}

void loop() {
  // Safely handle the successful connection LED animation here
  if (newlyConnected) {
    newlyConnected = false;
    Serial.println(">>> BLUETOOTH CONNECTED SUCCESSFULLY! <<<");
    LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0));
    LED_RGB.show();
    
    isLedGreen = true;
    ledTurnOffTime = millis() + 1000; // Set timer for 1 second from now
  }

  // Turn off the green LED after 1 second without using delay()
  if (isLedGreen && millis() > ledTurnOffTime) {
    isLedGreen = false;
    // Only clear if the alarm hasn't been triggered in the last second
    if (!isAlarmTriggered) { 
      LED_RGB.clear(); 
      LED_RGB.show();
    }
  }

  // Handle Serial Commands
  if (Serial2.available()) {
    String command = Serial2.readStringUntil('\n');
    command.trim(); 
  
    if (command == "A") {
      Serial.println("BRAIN SAID ALARM!");
      current_pcm_index = 0; 
      isAlarmTriggered = true;
      LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); 
      LED_RGB.show();
    }
    else if (command == "S") {
      Serial.println("BRAIN SAID STOP.");
      isAlarmTriggered = false; 
      LED_RGB.clear(); 
      LED_RGB.show();
    }
    else if (command.startsWith("N:")) {
      String newSpeaker = command.substring(2); 
      Serial.println("New Speaker Received: [" + newSpeaker + "]");
    
      preferences.begin("audio_prefs", false);
      preferences.putString("speaker", newSpeaker);
      preferences.end();
      
      Serial.println("Rebooting to connect to new speaker...");
      delay(1000);
      ESP.restart(); 
    }
  }
  
  delay(10);
}