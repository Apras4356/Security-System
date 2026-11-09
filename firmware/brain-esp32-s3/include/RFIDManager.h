#pragma once
#include <Arduino.h>
#include <MFRC522.h>
#include "Globals.h" // Assuming AccessZone and RFIDCard structs are defined here
#include "Config.h"  // Assuming MAX_ZONES, MAX_CARDS, etc. are defined here

// --- Expose these variables so WebPages.h can see them ---
extern AccessZone systemZones[];
extern int zoneCount;
extern RFIDCard enrolledCards[];
extern int cardCount;
extern String rfidLogs[];
extern int logIndex;
extern volatile bool isEnrollMode;
extern String pendingUID;

// --- Core RFID Functions ---
void setupRFID();
void handleRFID();
void manageRelays();
void initZoneReader(int slot);
void processCardStatus(String scannedUID, bool isRemote, int zoneID);
void logRFIDEvent(String nameOrUID, String status);
void openAccessPoint(int zoneID);
void loadZones();
void loadCards();
void saveCards();