#include "TelegramManager.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>

// Include your other managers to get their data!
#include "SecurityManager.h"
#include "RFIDManager.h"
#include "Config.h"
#include "Globals.h"

// --- Link to Main System Variables (ESP-NOW battery) ---
extern volatile float remoteBatteryVoltage;
extern int getBatteryPercentage(float voltage);
extern volatile bool triggerLowBatteryWarning;
extern volatile float lowBatteryVoltage;

volatile bool pendingAlarmNotification = false;
char alarmReason[128] = "";

// --- Internal Telegram Variables ---
String telegramToken = "";
String telegramChatID = "";
String callMeBotUser = "";
String targetSpeaker = "";

WiFiClientSecure secured_client;
UniversalTelegramBot* botPtr = nullptr;
#define bot (*botPtr)

// --- Implementations ---

void setupTelegram() {
    // CRITICAL: Ensure token is not empty before creating the bot!
    if (telegramToken == "") {
        Serial.println("❌ ERROR: telegramToken is empty! The bot will not work.");
    } else {
        Serial.println("Initializing Telegram with Token: " + telegramToken.substring(0, 5) + "...");
    }

    secured_client.setInsecure();
    botPtr = new UniversalTelegramBot(telegramToken, secured_client);
    Serial.println("Telegram Bot Initialized.");
}

void sendNotification(String message) {
    if (WiFi.status() == WL_CONNECTED && callMeBotUser != "") {
        message.replace(" ", "%20");
        String url = "https://api.callmebot.com/text.php?user=" + callMeBotUser + "&text=" + message;
        HTTPClient http;
        http.begin(url);
        int httpCode = http.GET();
        if (httpCode > 0) {
            Serial.println("CallMeBot sent: " + String(httpCode));
        } else {
            Serial.println("CallMeBot failed: " + http.errorToString(httpCode));
        }
        http.end();
    }
}

void sendTelegramMessage(String text, String parse_mode) {
    if (botPtr != nullptr && WiFi.status() == WL_CONNECTED && telegramChatID != "") {
        botPtr->sendMessage(telegramChatID, text, parse_mode);
    }
}

// --- The RTOS Task ---
void telegramLoop(void * pvParameters) {
    for(;;) {
        if (triggerLowBatteryWarning) {
            sendTelegramMessage("⚠️ *LOW BATTERY WARNING*\nGate Reader is at " + String(lowBatteryVoltage) + "V.\nPlease recharge soon.", "Markdown");
            triggerLowBatteryWarning = false; 
        }

        if (pendingAlarmNotification) {
            String reason(alarmReason);
            pendingAlarmNotification = false;
            sendNotification("SECURITY ALERT: " + reason);
            sendTelegramMessage("⚠️ ALARM TRIPPED: " + reason, "");
        }

        if (WiFi.status() == WL_CONNECTED && telegramToken != "") {
            esp_wifi_set_csi(false);
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            
            while(numNewMessages) {
                Serial.println("📥 Received " + String(numNewMessages) + " new Telegram messages!");
                
                for (int i = 0; i < numNewMessages; i++) {
                    String text = bot.messages[i].text;
                    String id = bot.messages[i].chat_id;
                    String senderName = bot.messages[i].from_name;
                    
                    // Debugging output so you know what is happening
                    Serial.println("Message: [" + text + "] from ID: [" + id + "] User: [" + senderName + "]");
                    Serial.println("Expected Chat ID is: [" + telegramChatID + "]");

                    // Security Check
                    if (id != telegramChatID) {
                        Serial.println("⛔ UNAUTHORIZED ID REJECTED: " + id);
                        bot.sendMessage(id, "Unauthorized user. Your ID is " + id, "");
                        continue; 
                    }

                    // Command Processing
                    if (text == "/status") {
                        String st = systemArmed ? "🟢 ARMED" : "🟠 DISARMED";
                        if (isAlarmActive) st = "🔴 ALARM TRIGGERED!";
                        
                        String battStr = "Waiting for ESP-NOW data...";
                        if (remoteBatteryVoltage > 0.0) {
                            int pct = getBatteryPercentage(remoteBatteryVoltage);
                            battStr = String(remoteBatteryVoltage) + "V (" + String(pct) + "%)";
                            if (pct < 15) battStr += " ⚠️ LOW BATT";
                        }
                        bot.sendMessage(id, "*System Status:* " + st + "\n*Gate Battery:* " + battStr, "Markdown");
                    } 
                    else if (text == "/arm") {
                        if (!systemArmed) {
                            resetSystemState(); 
                            bot.sendMessage(id, "🔒 *System Armed.* The mesh is now active.", "Markdown");
                        } else {
                            bot.sendMessage(id, "ℹ️ System is already armed.", "");
                        }
                    } 
                    else if (text == "/disarm") {
                        if (systemArmed || isAlarmActive) {
                            disarmSystem(); 
                            bot.sendMessage(id, "🔓 *System Disarmed.* Welcome home.", "Markdown");
                        } else {
                            bot.sendMessage(id, "ℹ️ System is already disarmed.", "");
                        }
                    } 
                    // ... [Keep your other commands (/logs, /cards, /zones) exactly as they were] ...
                    else if (text == "/logs") {
                        String logMsg = "📋 *Recent Events:*\n";
                        bool hasLogs = false;
                        for (int i = 0; i < MAX_LOGS; i++) {
                            int idx = (logIndex - 1 - i + MAX_LOGS) % MAX_LOGS;
                            if (rfidLogs[idx] != "" && rfidLogs[idx].length() > 2) {
                                hasLogs = true;
                                logMsg += "• " + rfidLogs[idx] + "\n";
                            }
                        }
                        if (!hasLogs) logMsg += "_No logs recorded yet._";
                        bot.sendMessage(id, logMsg, "Markdown");
                    }
                    else if (text == "/cards") {
                        String cardMsg = "🪪 *Enrolled Cards:*\n";
                        if (cardCount == 0) {
                            cardMsg += "_No cards enrolled._";
                        } else {
                            for (int i = 0; i < cardCount; i++) {
                                String status = enrolledCards[i].isActive ? "✅" : "❌";
                                cardMsg += status + " " + enrolledCards[i].name + " (`" + enrolledCards[i].uid + "`)\n";
                            }
                        }
                        bot.sendMessage(id, cardMsg, "Markdown");
                    }
                    else if (text == "/mesh") {
                        String meshMsg = "📡 *Sensing Mesh Nodes:*\n";
                        for (int i = 0; i < MAX_SENSING_APS; i++) {
                            if (sensingTable[i].active) {
                                char macStr[18];
                                snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                                         sensingTable[i].mac[0], sensingTable[i].mac[1],
                                         sensingTable[i].mac[2], sensingTable[i].mac[3],
                                         sensingTable[i].mac[4], sensingTable[i].mac[5]);
                                String label = (i == 0) ? " (Router)" : "";
                                meshMsg += "• AP" + String(i) + label + ": `" + String(macStr) + "`\n";
                            }
                        }
                        bot.sendMessage(id, meshMsg, "Markdown");
                    }
                    else if (text == "/zones") {
                        String zoneMsg = "🚪 *Access Zones:*\n";
                        bool hasZones = false;
                        for (int i = 0; i < zoneCount; i++) {
                            if (systemZones[i].isActive) {
                                hasZones = true;
                                zoneMsg += "• *" + systemZones[i].name + "* (SS:" + String(systemZones[i].ssPin) + " Relay:" + String(systemZones[i].relayPin) + ")\n";
                            }
                        }
                        if (!hasZones) zoneMsg += "_No zones defined._";
                        bot.sendMessage(id, zoneMsg, "Markdown");
                    }
                    else if (text == "/help" || text == "/cmds" || text == "/start") {
                        String helpMsg = "🤖 *Security Bot Commands:*\nTap a button below, or type a command:\n";
                        helpMsg += "• /status - Check system & battery\n";
                        helpMsg += "• /arm - Arm the system\n";
                        helpMsg += "• /disarm - Disarm the system\n";
                        helpMsg += "• /logs - View recent events\n";
                        helpMsg += "• /cards - List authorized users\n";
                        helpMsg += "• /zones - List hardware zones";
                        
                        String keyboardJson = "[[\"/status\", \"/logs\"], [\"/arm\", \"/disarm\"], [\"/cards\", \"/zones\"]]";
                        bot.sendMessageWithReplyKeyboard(id, helpMsg, "Markdown", keyboardJson, true);
                    }
                    else {
                        bot.sendMessage(id, "I didn't understand that. Type /help to see the menu.", "");
                    }
                }
                numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            }
            esp_wifi_set_csi(true);
        }
        vTaskDelay(1500 / portTICK_PERIOD_MS); 
    }
}