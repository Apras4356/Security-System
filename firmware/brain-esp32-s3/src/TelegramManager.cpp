#include "TelegramManager.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>

// Include your other managers to get their data!
#include "SecurityManager.h"
#include "RFIDManager.h"
#include "Config.h"
#include "Globals.h"

// --- Link to Main System Variables (ESP-NOW battery) ---
extern volatile float remoteBatteryVoltage;
extern int getBatteryPercentage(float voltage);

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
    // Set the WiFi client to insecure so it can connect to Telegram's API
    secured_client.setInsecure();
    botPtr = new UniversalTelegramBot(telegramToken, secured_client);
    Serial.println("Telegram Bot Initialized.");
}

void sendNotification(String message) {
    if (WiFi.status() == WL_CONNECTED && callMeBotUser != "") {
        message.replace(" ", "%20");
        String url = "https://api.callmebot.com/text.php?user=" + callMeBotUser + "&text=" + message;
        // (Note: If you haven't added HTTPClient to actually send this URL request, 
        // you'll need to include <HTTPClient.h> and execute it here later!)
    }
}

void sendTelegramMessage(String text, String parse_mode) {
    // telegramChatID is already handled inside this manager!
    if (botPtr != nullptr && WiFi.status() == WL_CONNECTED && telegramChatID != "") {
        botPtr->sendMessage(telegramChatID, text, parse_mode);
    }
}

// --- The RTOS Task ---
void telegramLoop(void * pvParameters) {
    for(;;) {
        if (WiFi.status() == WL_CONNECTED && telegramToken != "") {
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            
            while(numNewMessages) {
                for (int i = 0; i < numNewMessages; i++) {
                    String text = bot.messages[i].text;
                    String id = bot.messages[i].chat_id;
                    
                    if (id != telegramChatID) continue;

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
                    else if (text == "/logs") {
                        String logMsg = "📋 *Recent System Logs:*\n\n";
                        bool hasLogs = false;
                        for (int j = 0; j < MAX_LOGS; j++) {
                            int idx = (logIndex - 1 - j + MAX_LOGS) % MAX_LOGS;
                            if (rfidLogs[idx] != "" && rfidLogs[idx].length() > 2) {
                                hasLogs = true;
                                logMsg += "• `" + rfidLogs[idx] + "`\n";
                            }
                        }
                        if (!hasLogs) logMsg += "_No logs recorded yet._";
                        bot.sendMessage(id, logMsg, "Markdown");
                    }
                    else if (text == "/cards") {
                        String msg = "💳 *Enrolled RFID Cards:*\n\n";
                        if (cardCount == 0) {
                            msg += "_No cards enrolled yet._";
                        } else {
                            for (int c = 0; c < cardCount; c++) {
                                msg += "👤 *" + enrolledCards[c].name + "*\n";
                                msg += "└ UID: `" + enrolledCards[c].uid + "`\n";
                                
                                String zNames = "";
                                bool first = true;
                                for(int z = 0; z < zoneCount; z++) {
                                    if(systemZones[z].isActive && (enrolledCards[c].allowedZones & (1 << z))) {
                                        if(!first) zNames += ", ";
                                        zNames += systemZones[z].name;
                                        first = false;
                                    }
                                }
                                if(zNames == "") zNames = "None";
                                msg += "└ Zones: " + zNames + "\n\n";
                            }
                        }
                        bot.sendMessage(id, msg, "Markdown");
                    }
                    else if (text == "/zones") {
                        String msg = "🚪 *Active Access Zones:*\n\n";
                        bool hasZones = false;
                        for (int z = 0; z < MAX_ZONES; z++) {
                            if (z < zoneCount && systemZones[z].isActive) {
                                hasZones = true;
                                msg += "📍 *Zone " + String(z) + ": " + systemZones[z].name + "*\n";
                                msg += "└ Reader SS Pin: `" + String(systemZones[z].ssPin) + "`\n";
                                msg += "└ Relay Pin: `" + String(systemZones[z].relayPin) + "`\n\n";
                            }
                        }
                        if (!hasZones) msg += "_No zones defined yet._";
                        bot.sendMessage(id, msg, "Markdown");
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
        }
        vTaskDelay(1500 / portTICK_PERIOD_MS); 
    }
}