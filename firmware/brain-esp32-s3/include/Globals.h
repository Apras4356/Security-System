#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>

#include "Config.h"

extern bool devModeActive;
extern volatile bool isAlarmActive;
extern volatile bool systemArmed;
extern volatile bool isEnrollMode;
extern volatile int pendingArmTimer;
extern String pendingUID;
extern bool isGreek;
extern int armingDelaySeconds;
extern String sinricAppKey;
extern String sinricAppSecret;
extern String sinricDeviceId;

extern volatile bool pendingAlarmNotification;
extern char alarmReason[128];
extern bool laserEnabled;
extern bool smokeEnabled;

extern WebServer server;
extern Preferences preferences;

struct AccessZone {
  String name;
  int ssPin;
  int relayPin;
  bool isActive;
};

struct RFIDCard {
  String uid;
  String name;
  bool isActive;
  uint32_t allowedZones;
};

typedef struct {
  uint8_t mac[6];
  float lastScore;
  bool active;
  float static_background[CSI_CHANNELS];
} SensingAP;

extern SensingAP sensingTable[MAX_SENSING_APS];

extern RFIDCard enrolledCards[MAX_CARDS];
extern String rfidLogs[MAX_LOGS];

extern int cardCount;
extern int logIndex;

void resetSystemState();
void disarmSystem();
void triggerSeinagermos(String reason);
void saveCards();
void loadCards();