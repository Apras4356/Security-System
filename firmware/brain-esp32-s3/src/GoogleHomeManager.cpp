#include "GoogleHomeManager.h"
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include "Globals.h"

extern String sinricAppKey;
extern String sinricAppSecret;
extern String sinricDeviceId;

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Device %s turned %s (via SinricPro/Google Home)\r\n", deviceId.c_str(), state ? "on" : "off");
  return true; // request handled properly
}

void setupGoogleHome() {
  if (sinricAppKey == "" || sinricAppSecret == "" || sinricDeviceId == "") {
    Serial.println("Sinric Pro credentials missing. Google Home integration disabled.");
    return;
  }

  // Setup SinricPro Switch
  SinricProSwitch& mySwitch = SinricPro[sinricDeviceId.c_str()];
  mySwitch.onPowerState(onPowerState);

  // Setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(sinricAppKey.c_str(), sinricAppSecret.c_str());
  
  Serial.println("Sinric Pro initialized!");
}

void loopGoogleHome() {
  if (sinricAppKey != "" && sinricAppSecret != "" && sinricDeviceId != "") {
    SinricPro.handle();
  }
}

void triggerGoogleHomeAlarm() {
  if (sinricDeviceId == "") return;
  SinricProSwitch& mySwitch = SinricPro[sinricDeviceId.c_str()];
  // Send state to SinricPro server (this triggers the Google Home routine)
  mySwitch.sendPowerStateEvent(true); 
  Serial.println("Sent ON to SinricPro (Alarm Triggered)");
}

void restoreGoogleHomeNormal() {
  if (sinricDeviceId == "") return;
  SinricProSwitch& mySwitch = SinricPro[sinricDeviceId.c_str()];
  // Send state to SinricPro server
  mySwitch.sendPowerStateEvent(false);
  Serial.println("Sent OFF to SinricPro (Alarm Disarmed)");
}
