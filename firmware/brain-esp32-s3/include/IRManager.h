#pragma once
#include <Arduino.h>

// Expose this variable so the web server can tell us what button to learn
extern String activeLearningButton;

// The three core functions we will call from other files
void setupIR();
void sendIRCommand(String cmd);
void handleIRLearning();