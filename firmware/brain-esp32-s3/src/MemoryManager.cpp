#include "Globals.h"

void saveCards() {
  preferences.begin("rfid", false);
  preferences.putInt("count", cardCount);
  for(int i = 0; i < cardCount; i++) {
    preferences.putString(("uid"+String(i)).c_str(), enrolledCards[i].uid);
    preferences.putString(("name"+String(i)).c_str(), enrolledCards[i].name);
    preferences.putBool(("act"+String(i)).c_str(), enrolledCards[i].isActive);
    preferences.putUInt(("zones"+String(i)).c_str(), enrolledCards[i].allowedZones);
  }
  preferences.end();
}

void loadCards() {
  preferences.begin("rfid", true); 
  cardCount = preferences.getInt("count", 0);
  for(int i = 0; i < cardCount; i++) {
    enrolledCards[i].uid = preferences.getString(("uid"+String(i)).c_str(), "");
    enrolledCards[i].name = preferences.getString(("name"+String(i)).c_str(), "Unknown");
    enrolledCards[i].isActive = preferences.getBool(("act"+String(i)).c_str(), true);
    enrolledCards[i].allowedZones = preferences.getUInt(("zones"+String(i)).c_str(), 0xFFFFFFFF);
  }
  preferences.end();
}