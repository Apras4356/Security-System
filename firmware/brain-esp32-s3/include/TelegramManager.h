#pragma once
#include <Arduino.h>

// --- Expose these variables so WebPages.h can edit them ---
extern String telegramToken;
extern String telegramChatID;
extern String callMeBotUser;
extern String targetSpeaker;

// --- Core Telegram Functions ---
void setupTelegram();
void sendNotification(String message);
void sendTelegramMessage(String text, String parse_mode);

// The FreeRTOS Task Loop
void telegramLoop(void * pvParameters);