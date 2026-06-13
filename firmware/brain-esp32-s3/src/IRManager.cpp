#include "IRManager.h"
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <Preferences.h>

// Hardware Pins
const uint16_t IR_LED_PIN = 2; // Changed from 35 to avoid PSRAM conflict
const uint16_t IR_RECV_PIN = 15;

IRsend irsend(IR_LED_PIN);
IRrecv irrecv(IR_RECV_PIN);
decode_results ir_results;

// Internal State
uint32_t IR_POWER = 0;
uint32_t IR_VOL_UP = 0;
uint32_t IR_VOL_DOWN = 0;
uint32_t IR_MUTE = 0;
uint32_t IR_PC_IN = 0;
uint32_t IR_BT_IN = 0;
uint32_t IR_PLAY_PAUSE = 0;

String activeLearningButton = "";

// Import the buzzer function from your main file for the success beep
extern void passiveBuzzerBeep(int durationMs);

void setupIR() {
    irsend.begin();
    irrecv.enableIRIn();
    
    // Load saved codes on boot
    Preferences irPrefs;
    irPrefs.begin("ir_codes", true);
    IR_POWER = irPrefs.getUInt("power", 0x0);
    IR_VOL_UP = irPrefs.getUInt("volup", 0x0);
    IR_VOL_DOWN = irPrefs.getUInt("voldown", 0x0);
    IR_MUTE = irPrefs.getUInt("mute", 0x0);
    IR_PC_IN = irPrefs.getUInt("pcin", 0x0);
    IR_BT_IN = irPrefs.getUInt("btin", 0x0);
    IR_PLAY_PAUSE = irPrefs.getUInt("playpause", 0x0);
    irPrefs.end();
}

void sendIRCommand(String cmd) {
    if (cmd == "power" && IR_POWER != 0) irsend.sendNEC(IR_POWER, 32);
    else if (cmd == "volup" && IR_VOL_UP != 0) irsend.sendNEC(IR_VOL_UP, 32);
    else if (cmd == "voldown" && IR_VOL_DOWN != 0) irsend.sendNEC(IR_VOL_DOWN, 32);
    else if (cmd == "mute" && IR_MUTE != 0) irsend.sendNEC(IR_MUTE, 32);
    else if (cmd == "pcin" && IR_PC_IN != 0) irsend.sendNEC(IR_PC_IN, 32);
    else if (cmd == "btin" && IR_BT_IN != 0) irsend.sendNEC(IR_BT_IN, 32);
    else if (cmd == "playpause" && IR_PLAY_PAUSE != 0) irsend.sendNEC(IR_PLAY_PAUSE, 32);
}

void handleIRLearning() {
    if (irrecv.decode(&ir_results)) {
        if (activeLearningButton != "") {
            uint32_t newCode = (uint32_t)ir_results.value;
            if (newCode != 0xFFFFFFFF && newCode != 0) {
                Serial.println("Captured new code: 0x" + String(newCode, HEX));
                
                // Save it permanently
                Preferences irPrefs;
                irPrefs.begin("ir_codes", false);
                irPrefs.putUInt(activeLearningButton.c_str(), newCode);
                irPrefs.end();
                
                // Update live state
                if (activeLearningButton == "power") IR_POWER = newCode;
                else if (activeLearningButton == "volup") IR_VOL_UP = newCode;
                else if (activeLearningButton == "voldown") IR_VOL_DOWN = newCode;
                else if (activeLearningButton == "mute") IR_MUTE = newCode;
                else if (activeLearningButton == "pcin") IR_PC_IN = newCode;
                else if (activeLearningButton == "btin") IR_BT_IN = newCode;
                else if (activeLearningButton == "playpause") IR_PLAY_PAUSE = newCode;
                
                activeLearningButton = ""; 
                passiveBuzzerBeep(200);
            }
        }
        irrecv.resume();
    }
}