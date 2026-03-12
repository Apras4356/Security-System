#pragma once
#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

// --- Expose these states to WebPages.h and Telegram ---
extern volatile bool systemArmed;
extern volatile bool isAlarmActive;
extern bool isSirenDetected;
extern int webSoundSensitivity;
extern int activeAPCount;
extern volatile float gaitDisturbanceScore;

// --- Core Security Functions ---
void setupSecurity();
void resetSystemState();
void disarmSystem();
void triggerSeinagermos(String reason);
void addSensingAP(uint8_t* new_mac);

// The FreeRTOS Task Loop
void securityLoop(void * pvParameters);