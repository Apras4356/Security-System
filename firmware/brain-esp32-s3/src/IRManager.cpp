#include "IRManager.h"
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Preferences.h>

// Hardware Pins
const uint16_t IR_LED_PIN = 2; // Changed from 35 to avoid PSRAM conflict
const uint16_t IR_RECV_PIN = 15;

IRsend irsend(IR_LED_PIN);
IRrecv irrecv(IR_RECV_PIN);
decode_results ir_results;

struct IRCommandData {
  uint64_t value;
  uint16_t type;
  uint16_t bits;
};

// Internal State
IRCommandData IR_POWER = {0, 0, 0};
IRCommandData IR_VOL_UP = {0, 0, 0};
IRCommandData IR_VOL_DOWN = {0, 0, 0};
IRCommandData IR_MUTE = {0, 0, 0};
IRCommandData IR_PC_IN = {0, 0, 0};
IRCommandData IR_BT_IN = {0, 0, 0};
IRCommandData IR_PLAY_PAUSE = {0, 0, 0};

String activeLearningButton = "";

// Import the buzzer function from your main file for the success beep
extern void passiveBuzzerBeep(int durationMs);

void setupIR() {
  irsend.begin();
  irrecv.enableIRIn();

  // Load saved codes on boot
  Preferences irPrefs;
  irPrefs.begin("ir_codes", true);
  
  auto loadIR = [&](const char* key, IRCommandData& data) {
    data.value = irPrefs.getULong64(key, 0);
    data.type = irPrefs.getUInt((String(key) + "_t").c_str(), 0);
    data.bits = irPrefs.getUInt((String(key) + "_b").c_str(), 0);
  };

  loadIR("power", IR_POWER);
  loadIR("volup", IR_VOL_UP);
  loadIR("voldown", IR_VOL_DOWN);
  loadIR("mute", IR_MUTE);
  loadIR("pcin", IR_PC_IN);
  loadIR("btin", IR_BT_IN);
  loadIR("playpause", IR_PLAY_PAUSE);
  
  irPrefs.end();
}

void doSend(const String& cmdName, IRCommandData& data) {
  if (data.value != 0 && data.type != 0 && data.bits != 0) {
    Serial.printf("Sending %s: Value=0x%llX, Type=%d, Bits=%d\n", cmdName.c_str(), data.value, data.type, data.bits);
    irsend.send((decode_type_t)data.type, data.value, data.bits);
  } else {
    Serial.printf("IR code for %s is empty (not learned yet or type is UNKNOWN).\n", cmdName.c_str());
  }
}

void sendIRCommand(String cmd) {
  Serial.print("sendIRCommand Request: "); Serial.println(cmd);
  if (cmd == "power") doSend("power", IR_POWER);
  else if (cmd == "volup") doSend("volup", IR_VOL_UP);
  else if (cmd == "voldown") doSend("voldown", IR_VOL_DOWN);
  else if (cmd == "mute") doSend("mute", IR_MUTE);
  else if (cmd == "pcin") doSend("pcin", IR_PC_IN);
  else if (cmd == "btin") doSend("btin", IR_BT_IN);
  else if (cmd == "playpause") doSend("playpause", IR_PLAY_PAUSE);
}

void handleIRLearning() {
  if (irrecv.decode(&ir_results)) {
    if (activeLearningButton != "") {
      
      // Exclude repeat codes from being learned as primary codes
      if (ir_results.repeat) {
         irrecv.resume();
         return;
      }
      
      uint64_t newCode = ir_results.value;
      if (newCode != 0xFFFFFFFFFFFFFFFF && newCode != 0xFFFFFFFF && newCode != 0) {
        Serial.printf("Captured new code: %llX, Protocol: %d, Bits: %d\n", newCode, ir_results.decode_type, ir_results.bits);

        if (ir_results.decode_type == UNKNOWN) {
          Serial.println("WARNING: Protocol is UNKNOWN. Saving raw timings is not supported yet! Try getting closer or avoiding sunlight.");
        }

        // Save it permanently
        Preferences irPrefs;
        irPrefs.begin("ir_codes", false);
        irPrefs.putULong64(activeLearningButton.c_str(), newCode);
        irPrefs.putUInt((activeLearningButton + "_t").c_str(), ir_results.decode_type);
        irPrefs.putUInt((activeLearningButton + "_b").c_str(), ir_results.bits);
        irPrefs.end();

        // Update live state
        IRCommandData newData = {newCode, (uint16_t)ir_results.decode_type, ir_results.bits};
        if (activeLearningButton == "power") IR_POWER = newData;
        else if (activeLearningButton == "volup") IR_VOL_UP = newData;
        else if (activeLearningButton == "voldown") IR_VOL_DOWN = newData;
        else if (activeLearningButton == "mute") IR_MUTE = newData;
        else if (activeLearningButton == "pcin") IR_PC_IN = newData;
        else if (activeLearningButton == "btin") IR_BT_IN = newData;
        else if (activeLearningButton == "playpause") IR_PLAY_PAUSE = newData;

        activeLearningButton = "";
        passiveBuzzerBeep(200);
      }
    }
    irrecv.resume();
  }
}