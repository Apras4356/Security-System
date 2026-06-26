#pragma once

#include <Arduino.h>
#include <IRsend.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Globals.h"
#include "IRManager.h"
#include "RFIDManager.h"
#include "SecurityManager.h"
#include "TelegramManager.h"

extern bool devModeActive;
extern WebServer server;
extern Preferences preferences;
extern volatile bool isEnrollMode;
extern bool isGreek;
extern void saveCards();
extern volatile float remoteBatteryVoltage;
extern int getBatteryPercentage(float voltage);
extern volatile bool isAlarmActive;
// --- MISSING EXTERN DECLARATIONS ADDED HERE ---
extern int zoneCount;
extern AccessZone systemZones[MAX_ZONES];
extern void initZoneReader(int index);
extern int webSoundSensitivity;
extern String telegramToken;
extern String telegramChatID;
extern String callMeBotUser;
extern String targetSpeaker;
extern String activeLearningButton;
extern void sendIRCommand(String btn);
// ----------------------------------------------

inline String T(String en, String el) { return isGreek ? el : en; }

inline String getStyles() {
  String s = "<style>body{font-family: Arial; text-align: center; margin-top: "
             "50px; background-color: #222; color: white;} ";
  s += ".btn{padding: 15px 30px; font-size: 18px; margin: 10px; cursor: "
       "pointer; border-radius: 8px; border: none; font-weight: bold; "
       "text-decoration: none; display: inline-block;} ";
  s += ".btn-red{background-color: #e74c3c; color: white;} "
       ".btn-green{background-color: #2ecc71; color: white;} ";
  s += ".btn-blue{background-color: #3498db; color: white;} "
       ".btn-gray{background-color: #7f8c8d; color: white;} ";
  s += ".btn-orange{background-color: #e67e22; color: white;} ";
  s += "table {margin: 20px auto; border-collapse: collapse; width: 90%; "
       "max-width: 600px; background-color: #333;} ";
  s += "th, td {padding: 12px; border: 1px solid #555;} th {background-color: "
       "#444;} ";
  s += "input[type='text'], input[type='password'], input[type='number'] { "
       "width: 90%; max-width: 300px; padding: 12px; margin: 10px 0; "
       "border-radius: 6px; border: 1px solid #555; background-color: #444; "
       "color: white; font-size: 16px; } ";

  // --- CUSTOM PURPLE SLIDER CSS ---
  s += "input[type='range'] { -webkit-appearance: none; width: 80%; max-width: "
       "400px; background: transparent; margin: 15px 0; } ";
  s += "input[type='range']:focus { outline: none; } ";
  s += "input[type='range']::-webkit-slider-runnable-track { width: 100%; "
       "height: 10px; cursor: pointer; background: #f0f0f0; border-radius: "
       "5px; } ";
  s += "input[type='range']::-webkit-slider-thumb { height: 24px; width: 24px; "
       "border-radius: 50%; background: #9b59b6; cursor: pointer; "
       "-webkit-appearance: none; margin-top: -7px; box-shadow: 0 2px 5px "
       "rgba(0,0,0,0.5); } ";
  s += "input[type='range']::-moz-range-track { width: 100%; height: 10px; "
       "cursor: pointer; background: #f0f0f0; border-radius: 5px; } ";
  s += "input[type='range']::-moz-range-thumb { height: 24px; width: 24px; "
       "border-radius: 50%; background: #9b59b6; cursor: pointer; border: "
       "none; box-shadow: 0 2px 5px rgba(0,0,0,0.5); } ";
  // --------------------------------

  s += ".remote-box {background-color: #2c3e50; padding: 20px 10px; "
       "border-radius: 12px; border: 2px solid #34495e; max-width: 380px; "
       "margin: 30px auto; box-shadow: 0 4px 8px rgba(0,0,0,0.5);} ";
  s += ".btn-ir {width: 42%; margin: 6px; padding: 12px 0; font-size: 16px; "
       "border-radius: 8px; font-weight: bold; cursor: pointer; border: none; "
       "color: white;} ";
  s += ".btn-ir:active {opacity: 0.7; transform: scale(0.95);} ";
  s += "#learnOverlay { display:none; position:fixed; top:0; left:0; "
       "width:100%; height:100%; background:rgba(0,0,0,0.85); z-index:1000; "
       "color:#f39c12; padding-top:50%; font-size:24px; font-weight:bold; } ";
  s += "</style>";
  return s;
}

inline void handleSetLang() {
  isGreek = !isGreek;
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

inline void handleRoot() {
  String soundStatus = (isSirenDetected)
                           ? T("<span style='color:red;'>YES</span>",
                               "<span style='color:red;'>ΝΑΙ</span>")
                           : T("<span style='color:green;'>NO</span>",
                               "<span style='color:green;'>ΟΧΙ</span>");
  String armedStatus =
      (systemArmed) ? T("<span style='color:green;'>ARMED</span>",
                        "<span style='color:green;'>ΟΠΛΙΣΜΕΝΟ</span>")
                    : T("<span style='color:orange;'>DISARMED</span>",
                        "<span style='color:orange;'>ΑΦΟΠΛΙΣΜΕΝΟ</span>");

  String battDisplay = T("Waiting for data...", "Αναμονή δεδομένων...");
  if (remoteBatteryVoltage > 0.0) {
    int pct = getBatteryPercentage(remoteBatteryVoltage);
    String color = (pct > 20) ? "#2ecc71" : "#e74c3c";
    battDisplay = "<span style='color:" + color + "; font-weight:bold;'>" +
                  String(remoteBatteryVoltage) + "V (" + String(pct) +
                  "%)</span>";
  }

  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>";
  html += getStyles() + "</head><body>";

  html += "<div style='text-align:right; padding:10px; max-width:600px; "
          "margin:0 auto;'>";
  html += "<a href='/setlang' style='color:#3498db; text-decoration:none; "
          "font-weight:bold; font-size:18px;'>" +
          T("Ελληνικά GR", "English EN") + "</a></div>";

  html += "<h1>" + T("Security System Panel", "Πίνακας Συστήματος") + "</h1>";
  html += "<h2>" + T("System Status: ", "Κατάσταση: ") +
          "<span id='status-armed'>" + armedStatus + "</span></h2>";
  html += "<h2>" + T("Alarm Blasting? ", "Χτυπάει; ") +
          "<span id='status-siren'>" + soundStatus + "</span></h2>";
  html += "<h2>" + T("Gate Battery: ", "Μπαταρία Πύλης: ") +
          "<span id='status-batt'>" + battDisplay + "</span></h2>";

  html += "<button onclick='toggleAlarm(\"start\")' class='btn btn-red'>" +
          T("START ALARM", "ΕΝΑΡΞΗ") + "</button><br>";

  html += "<div style='background:#333; padding:15px; border-radius:8px; "
          "display:inline-block; margin:10px;'>";
  html += "<input type='number' id='armTimer' value='10' style='width:60px; "
          "display:inline-block; margin-right:10px;'>";
  html += "<span style='font-weight:bold; margin-right:10px;'>" +
          T("sec", "δευτ") + "</span>";
  html += "<button onclick='armSystem()' class='btn btn-blue' style='padding: "
          "10px 20px; font-size:16px; margin:0;'>" +
          T("ARM", "ΟΠΛΙΣΜΟΣ") + "</button></div><br>";

  html += "<button onclick='toggleAlarm(\"stop\")' class='btn btn-green'>" +
          T("STOP ALARM", "ΔΙΑΚΟΠΗ") +
          "</button><br><hr style='border-color:#444; width:80%; margin: 20px "
          "auto;'>";
  html += "<a href='/cards' class='btn btn-gray'>" + T("CARDS", "ΚΑΡΤΕΣ") +
          "</a><br>";
  html += "<a href='/zones' class='btn btn-blue'>" + T("ZONES", "ΖΩΝΕΣ") +
          "</a><br>";
  html += "<a href='/radar' class='btn' style='background-color:#9b59b6; "
          "color:white;'>" +
          T("RADAR", "ΡΑΝΤΑΡ") + "</a><br>";
  html += "<a href='/settings' class='btn btn-orange'>" +
          T("SETUP", "ΡΥΘΜΙΣΕΙΣ") + "</a>";
  html +=
      "<a href='/help' class='btn btn-gray'>" + T("HELP", "ΒΟΗΘΕΙΑ") + "</a>";

  html += "<div id='learnOverlay'>📡 " +
          T("Waiting for remote...", "Αναμονή για τηλεχειριστήριο...") +
          "<br><span style='font-size:16px; color:#aaa;'>" +
          T("Point remote at ESP32 and press the button",
            "Στοχεύστε το ESP32 και πατήστε το κουμπί") +
          "</span></div>";

  // --- VIRTUAL REMOTE CONTROL UI ---
  html += "<div class='remote-box' id='remoteBox'>";
  html += "<h3 style='color:#ecf0f1; margin-top:0;'>🔊 " +
          T("Speaker Remote", "Τηλεχειριστήριο Ηχείου") + "</h3>";

  html += "<button class='btn-ir' style='background-color:#e74c3c;' "
          "onclick='sendIR(\"power\")'>🔌 " +
          T("Power", "On/Off") + "</button>";
  html += "<button class='btn-ir' style='background-color:#f39c12;' "
          "onclick='sendIR(\"mute\")'>🔇 " +
          T("Mute", "Σίγαση") + "</button><br>";

  html += "<button class='btn-ir' style='background-color:#3498db;' "
          "onclick='sendIR(\"voldown\")'>🔉 " +
          T("Vol -", "Ένταση -") + "</button>";
  html += "<button class='btn-ir' style='background-color:#3498db;' "
          "onclick='sendIR(\"volup\")'>🔊 " +
          T("Vol +", "Ένταση +") + "</button><br>";

  html += "<button class='btn-ir' style='background-color:#9b59b6;' "
          "onclick='sendIR(\"pcin\")'>💻 " +
          T("PC Input", "Είσοδος PC") + "</button>";
  html += "<button class='btn-ir' style='background-color:#9b59b6;' "
          "onclick='sendIR(\"btin\")'>📱 " +
          T("BT Input", "Είσοδος BT") + "</button><br>";

  html += "<button class='btn-ir' style='background-color:#2ecc71; width:88%; "
          "margin-top:10px;' onclick='sendIR(\"playpause\")'>⏯️ " +
          T("Play / Pause", "Αναπαραγωγή / Παύση") + "</button>";

  html += "<hr style='border-color:#555; margin: 15px auto; width: 90%;'>";
  html += "<button id='learnBtn' class='btn btn-orange' style='width:90%; "
          "font-size:14px;' onclick='toggleLearnMode()'>🎓 " +
          T("ENTER LEARN MODE", "ΛΕΙΤΟΥΡΓΙΑ ΕΚΜΑΘΗΣΗΣ") + "</button>";
  html += "</div>";

  // --- JAVASCRIPT ---
  html += "<script>";
  html += "var isGr = " + String(isGreek ? "true" : "false") + ";";
  html += "var isLearningMode = false;";

  html += "function armSystem() {";
  html += "  var t = document.getElementById('armTimer').value;";
  html += "  fetch('/arm_mesh?t=' + t).then(() => {";
  html += "    setTimeout(refreshStatus, (t * 1000) + 500);";
  html += "  });";
  html += "}";

  html += "function toggleAlarm(action) {";
  html += "  fetch(action === 'start' ? '/alarm' : '/stop').then(() => {";
  html += "    setTimeout(refreshStatus, 200);"; // Wait 200ms for hardware to
                                                 // trigger, then update text
  html += "  });";
  html += "}";

  html += "function refreshStatus() {";
  html += "  fetch('/status_data').then(r => r.json()).then(data => {";
  html += "    var armSpan = document.getElementById('status-armed');";
  html += "    var sirenSpan = document.getElementById('status-siren');";

  // If alarm is ringing, show red ALARM text. Otherwise, show normal
  // Armed/Disarmed status.
  html += "    if (data.alarmActive) {";
  html += "      armSpan.innerHTML = isGr ? '<span style=\"color:red; "
          "font-weight:bold;\">🔴 ΣΥΝΑΓΕΡΜΟΣ!</span>' : '<span "
          "style=\"color:red; font-weight:bold;\">🔴 ALARM TRIGGERED!</span>';";
  html += "    } else {";
  html += "      armSpan.innerHTML = data.armed ? (isGr ? '<span "
          "style=\"color:green;\">ΟΠΛΙΣΜΕΝΟ</span>' : '<span "
          "style=\"color:green;\">ARMED</span>') : (isGr ? '<span "
          "style=\"color:orange;\">ΑΦΟΠΛΙΣΜΕΝΟ</span>' : '<span "
          "style=\"color:orange;\">DISARMED</span>');";
  html += "    }";

  // Update the siren text
  html += "    sirenSpan.innerHTML = (data.siren) ? (isGr "
          "? '<span style=\"color:red;\">ΝΑΙ</span>' : '<span "
          "style=\"color:red;\">YES</span>') : (isGr ? '<span "
          "style=\"color:green;\">ΟΧΙ</span>' : '<span "
          "style=\"color:green;\">NO</span>');";
  html += "  });";
  html += "}";

  html += "function toggleLearnMode() {";
  html += "  isLearningMode = !isLearningMode;";
  html += "  var btn = document.getElementById('learnBtn');";
  html += "  var box = document.getElementById('remoteBox');";
  html += "  if (isLearningMode) {";
  html += "    btn.innerHTML = isGr ? '🛑 ΑΚΥΡΩΣΗ ΕΚΜΑΘΗΣΗΣ' : '🛑 CANCEL "
          "LEARNING';";
  html += "    btn.className = 'btn btn-red';";
  html += "    box.style.border = '4px solid #f39c12';";
  html += "  } else {";
  html += "    btn.innerHTML = isGr ? '🎓 ΛΕΙΤΟΥΡΓΙΑ ΕΚΜΑΘΗΣΗΣ' : '🎓 ENTER "
          "LEARN MODE';";
  html += "    btn.className = 'btn btn-orange';";
  html += "    box.style.border = '2px solid #34495e';";
  html += "  }";
  html += "}";

  html += "function sendIR(cmd) {";
  html += "  if (isLearningMode) {";
  html += "    fetch('/api/learn?btn=' + cmd).then(() => {";
  html +=
      "      document.getElementById('learnOverlay').style.display = 'block';";
  html += "      checkLearnStatus();";
  html += "    });";
  html += "  } else {";
  html += "    fetch('/api/ir?cmd=' + cmd).catch(e => console.error(e));";
  html += "  }";
  html += "}";

  html += "function checkLearnStatus() {";
  html += "  var interval = setInterval(() => {";
  html += "    fetch('/api/learn_status').then(r => r.text()).then(txt => {";
  html += "      if (txt === 'DONE') {";
  html += "        clearInterval(interval);";
  html +=
      "        document.getElementById('learnOverlay').style.display = 'none';";
  html += "        toggleLearnMode();"; // Turn off learn mode automatically
  html += "        alert(isGr ? 'Επιτυχία! Ο κωδικός αποθηκεύτηκε.' : "
          "'Success! IR Code saved permanently.');";
  html += "      }";
  html += "    });";
  html += "  }, 1000);"; // Check every 1 second
  html += "}";

  html += "</script>";

  html += "<hr style='border-color:#444; width:80%; margin: 20px auto;'>";
  html += "<h2>" + T("Mic Sensitivity", "Ευαισθησία Μικροφώνου") + "</h2>";
  html += "<p style='color:#aaa; font-size:12px;'>" +
          T("Lower = More Sensitive. Higher = Less Sensitive.",
            "Μικρότερο = Πιο Ευαίσθητο. Μεγαλύτερο = Λιγότερο Ευαίσθητο.") +
          "</p>";

  // 10 = Triggers on talking. 100 = Triggers on claps. 250 = Triggers on
  // screams.
  html += "<input type='range' min='10' max='250' value='" +
          String(webSoundSensitivity) + "' ";
  html += "onchange='fetch(\"/set_sens?val=\" + this.value)' style='width: "
          "80%; max-width: 400px;'>";
  html += "<br><hr style='border-color:#444; width:80%; margin: 20px auto;'>";

  if (devModeActive) {
    html += "<a href='/toggle_dev' class='btn btn-red'>" +
            T("DISABLE DEV MODE", "ΑΠΕΝΕΡΓ. DEV MODE") + "</a>";
  } else {
    html += "<a href='/toggle_dev' class='btn btn-gray'>" +
            T("ENABLE DEV MODE", "ΕΝΕΡΓΟΠ. DEV MODE") + "</a>";
  }

  html += "</body></html>";

  server.send(200, "text/html", html);
}

inline void handleResetWiFi() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>" +
                getStyles();
  html += "</head><body>";
  html +=
      "<h1 style='color:#e74c3c;'>" +
      T("WiFi Erased! Rebooting...", "Το WiFi διαγράφηκε! Επανεκκίνηση...") +
      "</h1>";
  html += "<h2>" +
          T("Look for 'SecuritySystem-Setup' on your phone.",
            "Ψάξτε το δίκτυο 'SecuritySystem-Setup' στο κινητό σας.") +
          "</h2>";
  html += "</body></html>";

  server.send(200, "text/html", html);
  delay(2000);
  WiFi.disconnect(true, true);
  delay(1000);
  ESP.restart();
}

inline void handleSettings() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>" +
                getStyles() + "</head><body>";
  html += "<h1>" + T("System Configuration", "Ρυθμίσεις Συστήματος") + "</h1>";
  html += "<div style='background:#333; padding:20px; max-width: 400px; "
          "margin: 0 auto; border-radius: 8px;'>";
  String currentSSID = WiFi.SSID();
  if (currentSSID == "")
    currentSSID = T("Unknown", "Άγνωστο");
  html += "<h3 style='color:#2ecc71; margin-bottom: 5px;'>" +
          T("📡 Connected to: ", "📡 Συνδεδεμένο στο: ") + currentSSID +
          "</h3>";
  html += "<div style='background:#333; padding:20px; max-width: 400px; "
          "margin: 20px auto 0 auto; border-radius: 8px;'>";
  html += "<form action='/savesettings' method='POST'>";
  html += "<h3 style='color:#e67e22;'>" +
          T("Telegram Settings", "Ρυθμίσεις Telegram") + "</h3>";
  html += "<strong>" + T("Bot Token", "Bot Token") + "</strong><br>";
  html += "<input type='text' name='bot_token' value='" + telegramToken +
          "' required><br>";
  html += "<strong>" + T("Chat ID", "Chat ID") + "</strong><br>";
  html += "<input type='text' name='chat_id' value='" + telegramChatID +
          "' required><br><br>";
  html += "<strong>" + T("CallMeBot Username", "Όνομα Χρήστη CallMeBot") +
          "</strong><br>";
  html += "<input type='text' name='cmb_user' value='" + callMeBotUser +
          "' placeholder='@YourUsername' required><br><br>";
  html += "<strong>" + T("Arming Delay (seconds)", "Χρόνος Οπλισης (δευτ.)") +
          "</strong><br>";
  html += "<input type='number' name='arm_delay' value='" +
          String(armingDelaySeconds) + "' min='1' max='120' required><br><br>";
  html += "<hr style='border-color:#555; margin: 20px 0;'>";
  html += "<h3 style='color:#f1c40f;'>" +
          T("Hardware Toggles", "Διακόπτες Υλικού") + "</h3>";
  String laserChecked = laserEnabled ? "checked" : "";
  html +=
      "<label style='display: flex; align-items: center; cursor: pointer;'>";
  html += "<input type='checkbox' name='laser_en' value='1' style='width: "
          "25px; height: 25px; margin-right: 15px;' " +
          laserChecked + ">";
  html += "<strong>" +
          T("Enable Laser Tripwire", "Ενεργοποίηση Laser Ασφαλείας") +
          "</strong>";
  html += "</label><br><br>";

  String smokeChecked = smokeEnabled ? "checked" : "";
  html +=
      "<label style='display: flex; align-items: center; cursor: pointer;'>";
  html += "<input type='checkbox' name='smoke_en' value='1' style='width: "
          "25px; height: 25px; margin-right: 15px;' " +
          smokeChecked + ">";
  html += "<strong>" +
          T("Enable Smoke Detector", "Ενεργοποίηση Ανιχνευτή Καπνού") +
          "</strong>";
  html += "</label><br><br>";

  String wifiDetChecked = wifiDetectionEnabled ? "checked" : "";
  html +=
      "<label style='display: flex; align-items: center; cursor: pointer;'>";
  html += "<input type='checkbox' name='wifi_det' value='1' style='width: "
          "25px; height: 25px; margin-right: 15px;' " +
          wifiDetChecked + ">";
  html += "<strong>" +
          T("Enable WiFi Detection", "Ενεργοποίηση Ανίχνευσης WiFi") +
          "</strong>";
  html += "</label><br><br>";
  html += "<hr style='border-color:#555; margin: 20px 0;'>";

  html += "<h3 style='color:#9b59b6;'>" + T("Google Home / Sinric Pro", "Google Home / Sinric Pro") + "</h3>";
  html += "<strong>" + T("App Key", "App Key") + "</strong><br>";
  html += "<input type='text' name='sinric_key' value='" + sinricAppKey + "' placeholder='Sinric App Key'><br><br>";
  html += "<strong>" + T("App Secret", "App Secret") + "</strong><br>";
  html += "<input type='password' name='sinric_sec' value='" + sinricAppSecret + "' placeholder='Sinric App Secret'><br><br>";
  html += "<strong>" + T("Device ID (Switch)", "Device ID (Διακόπτης)") + "</strong><br>";
  html += "<input type='text' name='sinric_dev' value='" + sinricDeviceId + "' placeholder='Sinric Device ID'><br><br>";
  html += "<hr style='border-color:#555; margin: 20px 0;'>";
  html += "<h3 style='color:#9b59b6;'>" +
          T("Bluetooth Speaker", "Ηχείο Bluetooth") + "</h3>";
  html += "<strong>" + T("Exact Speaker Name", "Ακριβές Όνομα Ηχείου") +
          "</strong><br>";
  html += "<input type='text' name='bt_speaker' value='" + targetSpeaker +
          "' placeholder=''><br><br>";
  html += "<input type='submit' class='btn btn-green' value='" +
          T("SAVE & RESTART", "ΑΠΟΘΗΚΕΥΣΗ & ΕΠΑΝΕΚΚΙΝΗΣΗ") + "'>";
  html += "</form>";

  // --- RADAR NODE ADDITION FORM (OUTSIDE MAIN FORM) ---
  html += "<hr style='border-color:#555; margin: 30px 0 20px 0;'>";
  html += "<h3 style='color:#3498db;'>" + T("Radar Nodes (CSI)", "Κόμβοι Ραντάρ (CSI)") + "</h3>";
  
  html += "<ul style='list-style:none; padding:0;'>";
  for (int i = 1; i < activeAPCount; i++) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             sensingTable[i].mac[0], sensingTable[i].mac[1],
             sensingTable[i].mac[2], sensingTable[i].mac[3],
             sensingTable[i].mac[4], sensingTable[i].mac[5]);
    html += "<li style='margin-bottom:10px; font-family:monospace;'>" + String(macStr) + 
            " <a href='/del_mac?id=" + String(i) + "' style='background:#e74c3c; color:white; padding:4px 8px; text-decoration:none; border-radius:4px; font-weight:bold; margin-left:10px;' onclick=\"return confirm('Delete Node?');\">X</a></li>";
  }
  html += "</ul>";

  html += "<strong>" + T("Add Radar Node MAC", "Προσθήκη MAC Ραντάρ") + "</strong><br>";
  html += "<form action='/add_mac' method='POST' style='margin:0;'>";
  html += "<input type='text' name='new_mac' placeholder='XX:XX:XX:XX:XX:XX' required style='width: 60%; display:inline-block;'>";
  html += "<input type='submit' class='btn btn-blue' value='+' style='padding: 10px 15px; display:inline-block; margin-left:5px;'>";
  html += "</form>";
  // ---------------------------------------------------

  html += "<hr style='border-color:#e74c3c; margin: 30px 0 20px 0;'>";
  html += "<h3 style='color:#e74c3c;'>" + T("Danger Zone", "Επικίνδυνη Ζώνη") +
          "</h3>";
  String confirmMsg = T("Are you sure? This will disconnect the system from "
                        "WiFi and reboot into Setup Mode.",
                        "Είστε σίγουροι; Το σύστημα θα αποσυνδεθεί και θα μπει "
                        "σε λειτουργία ρύθμισης.");
  html +=
      "<a href='/resetwifi' class='btn btn-red' onclick=\"return confirm('" +
      confirmMsg + "');\">" + T("RESET WIFI", "ΕΠΑΝΑΦΟΡΑ WIFI") +
      "</a><br><br>";
  html += "</div><br><a href='/' class='btn btn-gray'>" + T("BACK", "ΠΙΣΩ") +
          "</a></body></html>";

  server.send(200, "text/html", html);
}

inline void handleSaveSettings() {
  String newToken = server.arg("bot_token");
  String newChatID = server.arg("chat_id");
  String newCMBUser = server.arg("cmb_user");
  String newSpeaker = server.arg("bt_speaker");
  String newSinricKey = server.arg("sinric_key");
  String newSinricSec = server.arg("sinric_sec");
  String newSinricDev = server.arg("sinric_dev");
  bool newLaserEnabled = server.hasArg("laser_en");
  bool newSmokeEnabled = server.hasArg("smoke_en");
  bool newWifiDetectionEnabled = server.hasArg("wifi_det");
  int newArmDelay =
      server.hasArg("arm_delay") ? server.arg("arm_delay").toInt() : 10;

  newToken.trim();
  newChatID.trim();
  newCMBUser.trim();
  newSpeaker.trim();
  newSinricKey.trim();
  newSinricSec.trim();
  newSinricDev.trim();

  preferences.begin("sys_prefs", false);
  preferences.putString("bot_token", newToken);
  preferences.putString("chat_id", newChatID);
  preferences.putString("cmb_user", newCMBUser);
  preferences.putString("bt_speaker", newSpeaker);
  preferences.putString("sinric_key", newSinricKey);
  preferences.putString("sinric_sec", newSinricSec);
  preferences.putString("sinric_dev", newSinricDev);
  preferences.putBool("laser_en", newLaserEnabled);
  preferences.putBool("smoke_en", newSmokeEnabled);
  preferences.putBool("wifi_det", newWifiDetectionEnabled);
  preferences.putInt("arm_delay", newArmDelay);
  preferences.end();

  // Update live state
  telegramToken = newToken;
  telegramChatID = newChatID;
  callMeBotUser = newCMBUser;
  targetSpeaker = newSpeaker;
  sinricAppKey = newSinricKey;
  sinricAppSecret = newSinricSec;
  sinricDeviceId = newSinricDev;
  laserEnabled = newLaserEnabled;
  smokeEnabled = newSmokeEnabled;
  wifiDetectionEnabled = newWifiDetectionEnabled;
  armingDelaySeconds = newArmDelay;

  // Tell the Mouth about the new speaker
  if (newSpeaker != "") {
    Serial2.println("N:" + newSpeaker);
  }

  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>"
                "<meta http-equiv='refresh' content='8;url=/' />" +
                getStyles() + "</head><body>";
  html += "<h1 style='color:#2ecc71;'>" +
          T("Settings Saved!", "Ρυθμίσεις Αποθηκεύτηκαν!") + "</h1>";
  html += "<h2>" + T("Restarting system...", "Επανεκκίνηση συστήματος...") +
          "</h2>";
  html += "</body></html>";
  server.send(200, "text/html", html);

  delay(2000);
  ESP.restart();
}

inline void handleHelp() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>" +
                getStyles() + "</head><body>";
  html += "<h1>" + T("Comprehensive Help Guide", "Πλήρης Οδηγός Συστήματος") +
          "</h1>";
  html += "<div style='background:#333; padding:20px; max-width:750px; "
          "margin:0 auto; text-align:left; border-radius:8px; line-height: "
          "1.6; font-size: 15px;'>";

  // 1. Dashboard & Status
  html += "<h2 style='color:#2ecc71; margin-bottom: 5px; border-bottom: 1px "
          "solid #555; padding-bottom: 5px;'>" +
          T("🎛️ Dashboard & Status", "🎛️ Πίνακας Ελέγχου & Κατάσταση") +
          "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li><b>" + T("System Status:", "Κατάσταση Συστήματος:") + "</b> " +
          T("Shows if the alarm is currently ARMED or DISARMED.",
            "Δείχνει αν ο συναγερμός είναι ΟΠΛΙΣΜΕΝΟΣ ή ΑΦΟΠΛΙΣΜΕΝΟΣ.") +
          "</li>";
  html += "<li><b>" + T("Gate Battery:", "Μπαταρία Πύλης:") + "</b> " +
          T("Displays the voltage and percentage of the remote ESP-NOW gate "
            "reader.",
            "Δείχνει την τάση και το ποσοστό της μπαταρίας του απομακρυσμένου "
            "αναγνώστη πύλης (ESP-NOW).") +
          "</li>";
  html += "<li><b>" + T("Start/Stop Alarm:", "Έναρξη/Διακοπή Συναγερμού:") +
          "</b> " +
          T("Manually trigger the siren or silence an active alarm.",
            "Χειροκίνητη ενεργοποίηση ή σίγαση της σειρήνας.") +
          "</li>";
  html +=
      "<li><b>" + T("Mic Sensitivity:", "Ευαισθησία Μικροφώνου:") + "</b> " +
      T("Slider to adjust the sound sensor. Lower values make it more "
        "sensitive to noise. Higher values require louder noises to trigger.",
        "Ρυθμιστικό για τον αισθητήρα ήχου. Μικρότερες τιμές τον κάνουν πιο "
        "ευαίσθητο στους θορύβους.") +
      "</li>";
  html += "</ul>";

  // 2. Access Management (Cards & Zones)
  html += "<h2 style='color:#3498db; margin-bottom: 5px; border-bottom: 1px "
          "solid #555; padding-bottom: 5px;'>" +
          T("🚪 Access Management (Cards & Zones)",
            "🚪 Διαχείριση Πρόσβασης (Κάρτες & Ζώνες)") +
          "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li><b>" + T("Zones Menu:", "Μενού Ζωνών:") + "</b> " +
          T("Define physical doors or readers. You must assign an SS/SDA Pin "
            "for the SPI RFID reader and a Relay Pin to unlock the door (-1 if "
            "no relay is connected).",
            "Ορίστε φυσικές πόρτες/αναγνώστες. Πρέπει να ορίσετε ένα SS/SDA "
            "Pin για τον αναγνώστη RFID και ένα Relay Pin για το ρελέ της "
            "πόρτας (-1 αν δεν υπάρχει).") +
          "</li>";
  html +=
      "<li><b>" + T("Cards Menu:", "Μενού Καρτών:") + "</b> " +
      T("Manage user access. Click <b>ADD NEW CARD</b>, tap the physical RFID "
        "tag on the reader, type the user's name, and check the boxes for "
        "which Zones they are allowed to open.",
        "Διαχείριση χρηστών. Πατήστε <b>ΠΡΟΣΘΗΚΗ</b>, σκανάρετε το RFID tag, "
        "δώστε όνομα και επιλέξτε ποιες Ζώνες επιτρέπεται να ανοίξει.") +
      "</li>";
  html += "<li><b>" + T("Logs Menu:", "Μενού Ιστορικού:") + "</b> " +
          T("View a history of scanned cards, access denied events, and alarm "
            "triggers.",
            "Δείτε το ιστορικό από σκαναρίσματα καρτών, αρνήσεις πρόσβασης και "
            "ενεργοποιήσεις συναγερμού.") +
          "</li>";
  html += "</ul>";

  // 3. Virtual Remote & IR Learning
  html += "<h2 style='color:#f39c12; margin-bottom: 5px; border-bottom: 1px "
          "solid #555; padding-bottom: 5px;'>" +
          T("📡 Virtual Remote & IR Learning",
            "📡 Εικονικό Τηλεχειριστήριο & Εκμάθηση IR") +
          "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li><b>" + T("Speaker Remote:", "Τηλεχειριστήριο Ηχείου:") +
          "</b> " +
          T("Use the on-screen buttons to control your connected audio system "
            "via Infrared (IR).",
            "Χρησιμοποιήστε τα κουμπιά της οθόνης για να ελέγξετε το "
            "ηχοσύστημά σας μέσω υπερύθρων (IR).") +
          "</li>";
  html +=
      "<li><b>" + T("How to Learn IR Codes:", "Πώς να μάθετε κωδικούς IR:") +
      "</b> " +
      T("To teach the ESP32 a new button:<br>1. Click <b>ENTER LEARN "
        "MODE</b>.<br>2. Click the on-screen button you want to program.<br>3. "
        "Point your real remote at the ESP32's IR receiver and press the "
        "physical button.<br>4. The screen will alert you when it saves "
        "successfully.",
        "Για να μάθει το ESP32 ένα νέο κουμπί:<br>1. Πατήστε <b>ΛΕΙΤΟΥΡΓΙΑ "
        "ΕΚΜΑΘΗΣΗΣ</b>.<br>2. Πατήστε το κουμπί στην οθόνη.<br>3. Στοχεύστε το "
        "πραγματικό χειριστήριο στον δέκτη IR του ESP32 και πατήστε το φυσικό "
        "κουμπί.<br>4. Η οθόνη θα σας ειδοποιήσει μόλις αποθηκευτεί.") +
      "</li>";
  html += "</ul>";

  // 4. Settings & Integrations
  html += "<h2 style='color:#9b59b6; margin-bottom: 5px; border-bottom: 1px "
          "solid #555; padding-bottom: 5px;'>" +
          T("⚙️ Settings & Integrations", "⚙️ Ρυθμίσεις & Ενσωματώσεις") +
          "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li><b>" + T("Telegram Bot:", "Telegram Bot:") + "</b> " +
          T("Search <b>@BotFather</b> on Telegram and send "
            "<code>/newbot</code> to get your Token. Search "
            "<b>@userinfobot</b> to find your personal Chat ID.",
            "Ψάξτε το <b>@BotFather</b> στο Telegram και στείλτε "
            "<code>/newbot</code> για το Token. Ψάξτε το <b>@userinfobot</b> "
            "για το Chat ID σας.") +
          "</li>";
  html +=
      "<li><b>" + T("CallMeBot:", "CallMeBot:") + "</b> " +
      T("For phone call alerts during an alarm, message <b>@CallMeBot_API</b> "
        "on Telegram to authorize it, then enter your @username in the "
        "settings.",
        "Για τηλεφωνικές κλήσεις, στείλτε μήνυμα στο <b>@CallMeBot_API</b> στο "
        "Telegram για έγκριση, και βάλτε το @username σας στις ρυθμίσεις.") +
      "</li>";
  html +=
      "<li><b>" + T("Bluetooth Speaker:", "Ηχείο Bluetooth:") + "</b> " +
      T("Enter the EXACT pairing name of your Bluetooth speaker for automatic "
        "audio connection.",
        "Βάλτε το ΑΚΡΙΒΕΣ όνομα του ηχείου σας για αυτόματη σύνδεση ήχου.") +
      "</li>";
  html += "</ul>";

  // 5. WiFi & Reset
  html += "<h2 style='color:#e74c3c; margin-bottom: 5px; border-bottom: 1px "
          "solid #555; padding-bottom: 5px;'>" +
          T("🌐 WiFi & Reset", "🌐 WiFi & Επαναφορά") + "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li>" +
          T("If you change your home router, go to Settings -> <b>Danger "
            "Zone</b> and click <b>Reset WiFi</b>.",
            "Αν αλλάξετε ρούτερ, πηγαίνετε στις Ρυθμίσεις -> <b>Επικίνδυνη "
            "Ζώνη</b> και πατήστε <b>Επαναφορά WiFi</b>.") +
          "</li>";
  html +=
      "<li>" +
      T("The system will erase the network and reboot. Look for a network "
        "called <code>SecuritySystem-Setup</code> (Password: "
        "<code>12345678</code>) on your phone to connect it to the new router.",
        "Το σύστημα θα διαγράψει το δίκτυο και θα κάνει επανεκκίνηση. Ψάξτε το "
        "δίκτυο <code>SecuritySystem-Setup</code> (Κωδικός: "
        "<code>12345678</code>) στο κινητό σας για να το συνδέσετε στο νέο "
        "ρούτερ.") +
      "</li>";
  html += "</ul>";

  html += "</div><br><a href='/' class='btn btn-gray'>" +
          T("BACK TO HOME", "ΕΠΙΣΤΡΟΦΗ") + "</a><br><br></body></html>";

  server.send(200, "text/html", html);
}

extern void triggerSeinagermos(String reason);
extern void disarmSystem();

inline void handleArmMesh() {
  if (server.hasArg("t")) {
    pendingArmTimer = server.arg("t").toInt();
    if (pendingArmTimer <= 0)
      pendingArmTimer = 1;
  } else {
    pendingArmTimer = 10;
  }
  server.send(200, "text/plain", "OK");
}

inline void handleWebStart() {
  triggerSeinagermos("Web Interface Button");
  server.send(200, "text/plain", "OK");
}

inline void handleWebStop() {
  disarmSystem();
  server.send(200, "text/plain", "OK");
}

inline void handleEnroll() {
  isEnrollMode = true;
  pendingUID = "";
  server.sendHeader("Location", "/cards");
  server.send(302, "text/plain", "");
}

inline void handleCancelEnroll() {
  isEnrollMode = false;
  pendingUID = "";
  server.sendHeader("Location", "/cards");
  server.send(302, "text/plain", "");
}

inline void handleSaveCard() {
  // WebServer automatically handles the POST body, no manual parsing needed!
  String uid = server.arg("uid");
  String name = server.arg("name");

  if (uid != "" && name != "" && cardCount < MAX_CARDS) {
    enrolledCards[cardCount].uid = uid;
    enrolledCards[cardCount].name = name;

    uint32_t mask = 0;
    for (int i = 0; i < zoneCount; i++) {
      if (server.arg("z" + String(i)) == "1") {
        mask |= (1 << i);
      }
    }

    enrolledCards[cardCount].allowedZones = mask;
    enrolledCards[cardCount].isActive = true;

    cardCount++;
    isEnrollMode = false;
    pendingUID = "";
    saveCards();

    Serial.println("Enrolled card: " + name);
  }

  server.sendHeader("Location", "/cards");
  server.send(302, "text/plain", "");
}

inline void handleLogs() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>" +
                getStyles() + "</head><body>";
  html += "<h1>" + T("System Logs", "Ιστορικό Συστήματος") + "</h1>";
  html += "<div style='background:#333; padding:20px; max-width:600px; "
          "margin:0 auto; border-radius:8px; text-align:left;'>";

  // Send in chunks to save RAM
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);

  bool hasLogs = false;
  for (int i = 0; i < MAX_LOGS; i++) {
    int idx = (logIndex - 1 - i + MAX_LOGS) % MAX_LOGS;
    if (rfidLogs[idx] != "" && rfidLogs[idx].length() > 2) {
      hasLogs = true;
      String logLine = "<div style='border-bottom: 1px solid #444; padding: "
                       "8px 0; font-family: monospace; font-size: 14px;'>";
      logLine += "<span style='color:#3498db;'>&bull;</span> " + rfidLogs[idx] +
                 "</div>";
      server.sendContent(logLine);
    }
  }

  if (!hasLogs) {
    server.sendContent(
        "<p style='text-align:center;'><i>" +
        T("No logs recorded yet.", "Δεν υπάρχει ιστορικό ακόμα.") + "</i></p>");
  }

  server.sendContent("</div><br><a href='/' class='btn btn-gray'>" +
                     T("BACK", "ΠΙΣΩ") + "</a></body></html>");
  server.sendContent(""); // End chunked transmission
}

inline void handleCards() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>" +
                getStyles() + "</head><body>";
  html += "<h1>" + T("Enrolled Cards", "Εγγεγραμμένες Κάρτες") + "</h1>";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);

  if (isEnrollMode) {
    String enrollBox =
        "<div style='background:#333; padding:20px; max-width:500px; margin:0 "
        "auto 20px auto; border-radius:8px; border: 2px solid #e67e22;'>";
    if (pendingUID == "") {
      enrollBox += "<h3 style='color:#e67e22;'>" +
                   T("WAITING FOR SCAN...", "ΑΝΑΜΟΝΗ ΓΙΑ ΚΑΡΤΑ...") + "</h3>";
      enrollBox += "<p>" +
                   T("Please tap your card on the reader now.",
                     "Παρακαλώ σκανάρετε την κάρτα σας στον αναγνώστη.") +
                   "</p>";
      enrollBox +=
          "<br><a href='/scanner' target='_blank' class='btn btn-blue'>" +
          T("OPEN QR SCANNER", "ΑΝΟΙΓΜΑ ΣΑΡΩΤΗ QR") + "</a><br>";
      enrollBox += "<script>";
      enrollBox += "setInterval(function() {";
      enrollBox += "  fetch('/checkuid').then(r => r.json()).then(data => {";
      enrollBox += "    if (data.uid !== '') { window.location.reload(); }";
      enrollBox += "  });";
      enrollBox += "}, 1000);";
      enrollBox += "</script>";
    } else {
      enrollBox += "<h3 style='color:#2ecc71;'>" +
                   T("CARD DETECTED!", "Η ΚΑΡΤΑ ΒΡΕΘΗΚΕ!") + "</h3>";
      enrollBox += "<form action='/savecard' method='POST'>";
      enrollBox +=
          "<input type='hidden' name='uid' value='" + pendingUID + "'>";
      enrollBox += "<strong>" + T("Enter Owner Name:", "Όνομα Κατόχου:") +
                   "</strong><br>";
      enrollBox += "<input type='text' name='name' autofocus required><br><br>";
      enrollBox +=
          "<strong>" +
          T("Allowed Access Zones:", "Επιτρεπόμενες Ζώνες Πρόσβασης:") +
          "</strong><br>";
      enrollBox += "<div style='text-align: left; padding-left: 20px;'>";

      if (zoneCount == 0) {
        enrollBox +=
            "<span style='color:#e74c3c;'>" +
            T("No zones created yet! Go to Zones menu first.",
              "Δεν έχουν δημιουργηθεί ζώνες! Πηγαίνετε στο μενού Ζώνες.") +
            "</span><br>";
      } else {
        for (int i = 0; i < zoneCount; i++) {
          if (systemZones[i].isActive) {
            enrollBox += "<input type='checkbox' name='z" + String(i) +
                         "' value='1' checked> " + systemZones[i].name + "<br>";
          }
        }
      }
      enrollBox += "</div><br>";
      enrollBox += "<input type='submit' class='btn btn-green' value='" +
                   T("SAVE CARD", "ΑΠΟΘΗΚΕΥΣΗ") + "'>";
      enrollBox += "</form>";
    }
    enrollBox += "<br><a href='/cancelenroll' style='color:#e74c3c; "
                 "text-decoration:none;'>" +
                 T("Cancel", "Ακύρωση") + "</a>";
    enrollBox += "</div>";
    server.sendContent(enrollBox);
  } else {
    server.sendContent("<a href='/enroll' class='btn btn-blue'>" +
                       T("ADD NEW CARD", "ΠΡΟΣΘΗΚΗ") +
                       "</a><br><hr style='border-color:#444; width:80%; "
                       "margin: 20px auto;'>");
  }

  server.sendContent("<table><tr style='background:#444;'><th>" +
                     T("Name", "Όνομα") + "</th><th>UID</th><th>" +
                     T("Zones", "Ζώνες") + "</th><th>" +
                     T("Action", "Ενέργεια") + "</th></tr>");

  for (int i = 0; i < cardCount; i++) {
    String row = "<tr>";
    row += "<td>" + enrolledCards[i].name + "</td>";
    row += "<td style='font-family:monospace; color:#aaa;'>" +
           enrolledCards[i].uid + "</td>";

    String allowedZonesStr = "";
    bool firstZone = true;
    for (int z = 0; z < zoneCount; z++) {
      if (systemZones[z].isActive &&
          (enrolledCards[i].allowedZones & (1 << z))) {
        if (!firstZone)
          allowedZonesStr += ", ";
        allowedZonesStr += systemZones[z].name;
        firstZone = false;
      }
    }
    if (allowedZonesStr == "")
      allowedZonesStr = "-";

    row += "<td style='font-size: 14px;'>" + allowedZonesStr + "</td>";
    row += "<td><a href='/deletecard?uid=" + enrolledCards[i].uid +
           "' style='color:#e74c3c; text-decoration:none; "
           "font-weight:bold;'>[X]</a></td></tr>";
    server.sendContent(row);
  }

  server.sendContent("</table><br><a href='/' class='btn btn-gray'>" +
                     T("BACK", "ΠΙΣΩ") + "</a></body></html>");
  server.sendContent("");
}

inline void handleZones() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>" +
                getStyles() + "</head><body>";
  html += "<h1>" + T("Access Zones", "Ζώνες Πρόσβασης") + "</h1>";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);

  String addBox =
      "<div style='background:#333; padding:20px; max-width:500px; margin:0 "
      "auto 20px auto; border-radius:8px; border: 1px solid #3498db;'>";
  addBox += "<h3>" + T("Add New Door Zone", "Προσθήκη Νέας Ζώνης") + "</h3>";
  addBox += "<form action='/savezone' method='POST'>";
  addBox += "<strong>" + T("Zone Name:", "Όνομα Ζώνης:") + "</strong><br>";
  addBox += "<input type='text' name='zname' required><br>";
  addBox += "<strong>" + T("Reader SS/SDA Pin:", "Pin SS/SDA Αναγνώστη:") +
            "</strong><br>";
  addBox += "<input type='number' name='zss' value='21' required><br>";
  addBox +=
      "<strong>" +
      T("Door Relay Pin (-1 for none):", "Pin Ρελέ Πόρτας (-1 για κανένα):") +
      "</strong><br>";
  addBox += "<input type='number' name='zpin' value='-1' required><br><br>";
  addBox += "<input type='submit' class='btn btn-blue' value='" +
            T("ADD ZONE", "ΠΡΟΣΘΗΚΗ") + "'>";
  addBox += "</form></div>";
  server.sendContent(addBox);

  server.sendContent("<table><tr style='background:#444;'><th>ID</th><th>" +
                     T("Name", "Όνομα") +
                     "</th><th>SS Pin</th><th>Relay</th><th>" +
                     T("Action", "Ενέργεια") + "</th></tr>");

  bool hasZones = false;
  for (int i = 0; i < MAX_ZONES; i++) {
    if (i < zoneCount && systemZones[i].isActive) {
      hasZones = true;
      String row = "<tr>";
      row += "<td style='color:#aaa;'>" + String(i) + "</td>";
      row += "<td>" + systemZones[i].name + "</td>";
      row += "<td>" + String(systemZones[i].ssPin) + "</td>";
      row += "<td>" + String(systemZones[i].relayPin) + "</td>";
      row += "<td><a href='/deletezone?id=" + String(i) +
             "' style='color:#e74c3c; text-decoration:none; "
             "font-weight:bold;'>[X]</a></td></tr>";
      server.sendContent(row);
    }
  }

  if (!hasZones) {
    server.sendContent("<tr><td colspan='4'>" +
                       T("No zones defined.", "Δεν υπάρχουν ζώνες.") +
                       "</td></tr>");
  }

  server.sendContent("</table><br><a href='/' class='btn btn-gray'>" +
                     T("BACK", "ΠΙΣΩ") + "</a></body></html>");
  server.sendContent("");
}

inline void handleSaveZone() {
  String zname = server.arg("zname");
  String zssStr = server.arg("zss");
  String zpinStr = server.arg("zpin");

  if (zname != "" && zssStr != "" && zpinStr != "") {
    int slot = -1;
    for (int i = 0; i < MAX_ZONES; i++) {
      if (!systemZones[i].isActive) {
        slot = i;
        break;
      }
    }

    if (slot != -1) {
      systemZones[slot].name = zname;
      systemZones[slot].ssPin = zssStr.toInt();
      systemZones[slot].relayPin = zpinStr.toInt();
      systemZones[slot].isActive = true;

      if (systemZones[slot].ssPin >= 0) {
        initZoneReader(slot);
      }
      if (systemZones[slot].relayPin >= 0) {
        pinMode(systemZones[slot].relayPin, OUTPUT);
        digitalWrite(systemZones[slot].relayPin, LOW);
      }

      if (slot >= zoneCount)
        zoneCount = slot + 1;

      preferences.begin("sys_zones", false);
      String keyBase = "z" + String(slot);
      preferences.putString((keyBase + "n").c_str(), systemZones[slot].name);
      preferences.putInt((keyBase + "s").c_str(), systemZones[slot].ssPin);
      preferences.putInt((keyBase + "r").c_str(), systemZones[slot].relayPin);
      preferences.putBool((keyBase + "a").c_str(), true);
      preferences.putInt("zCount", zoneCount);
      preferences.end();

      Serial.println("Zone " + String(slot) + " saved securely.");
    }
  }

  server.sendHeader("Location", "/zones");
  server.send(302, "text/plain", "");
}

inline void handleDeleteZone() {
  String idStr = server.arg("id");
  if (idStr != "") {
    int id = idStr.toInt();
    if (id >= 0 && id < MAX_ZONES) {
      systemZones[id].isActive = false;
      preferences.begin("sys_zones", false);
      preferences.putBool(("z" + String(id) + "a").c_str(), false);
      preferences.end();
    }
  }
  server.sendHeader("Location", "/zones");
  server.send(302, "text/plain", "");
}

inline void handleDeleteCard() {
  String uidToDelete = server.arg("uid");
  if (uidToDelete != "") {
    int foundIndex = -1;
    for (int i = 0; i < cardCount; i++) {
      if (enrolledCards[i].uid == uidToDelete) {
        foundIndex = i;
        break;
      }
    }
    if (foundIndex != -1) {
      for (int i = foundIndex; i < cardCount - 1; i++) {
        enrolledCards[i] = enrolledCards[i + 1];
      }
      cardCount--;
      saveCards();
    }
  }
  server.sendHeader("Location", "/cards");
  server.send(302, "text/plain", "");
}

inline void handleCheckPendingUID() {
  server.send(200, "application/json", "{\"uid\":\"" + pendingUID + "\"}");
}

inline void handleStatusData() {
  String json = "{";
  json += "\"armed\":" + String(systemArmed ? "true" : "false") + ",";
  json += "\"siren\":" + String(isSirenDetected ? "true" : "false") + ",";
  json += "\"alarmActive\":" + String(isAlarmActive ? "true" : "false") + ",";
  json += "\"voltage\":" + String(remoteBatteryVoltage) + ",";
  json += "\"battPct\":" + String(getBatteryPercentage(remoteBatteryVoltage));
  json += "}";

  server.send(200, "application/json", json);
}

inline void handleSetSens() {
  if (server.hasArg("val")) {
    extern int webSoundSensitivity;
    webSoundSensitivity = server.arg("val").toInt();

    preferences.begin("sys_prefs", false);
    preferences.putInt("mic_sens", webSoundSensitivity);
    preferences.end();

    Serial.println("Mic Sensitivity updated to: " +
                   String(webSoundSensitivity));
  }
  server.send(200, "text/plain", "OK");
}

inline void handleToggleDev() {
  extern bool devModeActive;
  devModeActive = !devModeActive;

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

inline void handleIRLearnCmd() {
  if (server.hasArg("btn")) {
    activeLearningButton = server.arg("btn");
    Serial.println("Learning mode activated for: " + activeLearningButton);
    server.send(200, "text/plain", "LEARNING");
  } else {
    server.send(400, "text/plain", "Missing button");
  }
}

inline void handleIRLearnCheck() {
  if (activeLearningButton == "") {
    server.send(200, "text/plain", "DONE");
  } else {
    server.send(200, "text/plain", "WAITING");
  }
}

inline void handleIRCommand() {
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    Serial.println("Web Remote triggered: " + cmd);

    sendIRCommand(cmd);

    delay(100);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing command");
  }
}

// --- CSI Data JSON Endpoint ---
inline void handleCSIData() {
  String json = "{";
  json += "\"armed\":" + String(systemArmed ? "true" : "false") + ",";
  json += "\"gaitScore\":" + String(gaitDisturbanceScore, 3) + ",";

  float dynamicThreshold = 0.85 - ((activeAPCount - 1) * 0.08);
  if (dynamicThreshold < 0.55)
    dynamicThreshold = 0.55;
  json += "\"threshold\":" + String(dynamicThreshold, 3) + ",";
  json += "\"apCount\":" + String(activeAPCount) + ",";
  json += "\"aps\":[";

  bool first = true;
  for (int i = 0; i < MAX_SENSING_APS; i++) {
    if (sensingTable[i].active) {
      if (!first)
        json += ",";
      first = false;
      json += "{\"id\":" + String(i);

      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               sensingTable[i].mac[0], sensingTable[i].mac[1],
               sensingTable[i].mac[2], sensingTable[i].mac[3],
               sensingTable[i].mac[4], sensingTable[i].mac[5]);
      json += ",\"mac\":\"" + String(macStr) + "\"";
      json += ",\"score\":" + String(sensingTable[i].lastScore, 3);

      // Send 16 evenly-spaced channels out of 64 to keep payload small
      json += ",\"ch\":[";
      for (int c = 0; c < 64; c += 4) {
        if (c > 0)
          json += ",";
        json += String(csiChannelAmplitudes[i][c], 2);
      }
      json += "]";
      json += "}";
    }
  }

  json += "]}";
  server.send(200, "application/json", json);
}

// --- 3D Radar Visualization Page ---
inline void handleRadar() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // --- HTML Head ---
  server.sendContent(
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>CSI Radar - Seinagermos</title>"
      "<style>"
      "*{margin:0;padding:0;box-sizing:border-box}"
      "body{background:#0a0a0f;overflow:hidden;font-family:'Segoe "
      "UI',Arial,sans-serif;color:#eee}"
      "#hud{position:fixed;top:12px;left:12px;z-index:10;pointer-events:none}"
      ".hud-card{background:rgba(15,15,25,0.85);border:1px solid "
      "rgba(155,89,182,0.4);"
      "border-radius:10px;padding:12px "
      "16px;margin-bottom:8px;backdrop-filter:blur(8px);"
      "min-width:220px;pointer-events:auto}"
      ".hud-title{font-size:11px;text-transform:uppercase;letter-spacing:2px;"
      "color:#9b59b6;margin-bottom:6px}"
      ".hud-val{font-size:28px;font-weight:700}"
      ".hud-sub{font-size:11px;color:#888;margin-top:2px}"
      ".bar-row{display:flex;align-items:flex-end;height:40px;gap:1px;margin-"
      "top:6px}"
      ".bar{flex:1;background:#9b59b6;min-width:2px;border-radius:1px 1px 0 "
      "0;transition:height .15s}"
      "#legend{position:fixed;bottom:12px;left:12px;z-index:10}"
      ".legend-card{background:rgba(15,15,25,0.85);border:1px solid "
      "rgba(155,89,182,0.3);"
      "border-radius:10px;padding:10px 14px;backdrop-filter:blur(8px)}"
      ".legend-item{display:flex;align-items:center;gap:8px;font-size:11px;"
      "color:#aaa;margin:3px 0}"
      ".legend-dot{width:10px;height:10px;border-radius:50%}"
      "#back-btn{position:fixed;top:12px;right:12px;z-index:10;background:rgba("
      "15,15,25,0.85);"
      "border:1px solid rgba(155,89,182,0.4);border-radius:8px;padding:8px "
      "16px;"
      "color:#9b59b6;text-decoration:none;font-size:13px;font-weight:600;"
      "cursor:pointer;"
      "backdrop-filter:blur(8px);transition:all .2s}"
      "#back-btn:hover{background:rgba(155,89,182,0.2);color:#fff}"
      "#status-pill{position:fixed;top:12px;right:100px;z-index:10;"
      "border-radius:20px;padding:6px 14px;font-size:11px;font-weight:700;"
      "letter-spacing:1px;backdrop-filter:blur(8px)}"
      ".armed{background:rgba(46,204,113,0.2);border:1px solid "
      "#2ecc71;color:#2ecc71}"
      ".disarmed{background:rgba(243,156,18,0.2);border:1px solid "
      "#f39c12;color:#f39c12}"
      ".alarm{background:rgba(231,76,60,0.3);border:1px solid "
      "#e74c3c;color:#e74c3c;animation:pulse-pill 0.8s infinite}"
      "@keyframes pulse-pill{0%,100%{opacity:1}50%{opacity:0.5}}"
      "</style></head><body>");

  // --- HTML Body ---
  server.sendContent(
      "<a href='/' id='back-btn'>← BACK</a>"
      "<div id='status-pill' class='disarmed'>DISARMED</div>"
      "<div id='hud'>"
      "<div class='hud-card'>"
      "<div class='hud-title'>Gait Disturbance</div>"
      "<div class='hud-val' id='score-val'>0.000</div>"
      "<div class='hud-sub'>Threshold: <span id='thresh-val'>0.85</span></div>"
      "</div>"
      "<div class='hud-card' id='ap-cards'></div>"
      "</div>"
      "<div id='legend'><div class='legend-card'>"
      "<div class='legend-item'><div class='legend-dot' "
      "style='background:#3498db'></div>AP Node (calm)</div>"
      "<div class='legend-item'><div class='legend-dot' "
      "style='background:#e74c3c'></div>AP Node (disturbed)</div>"
      "<div class='legend-item'><div class='legend-dot' "
      "style='background:#9b59b6'></div>Fresnel Zone</div>"
      "<div class='legend-item'><div class='legend-dot' "
      "style='background:rgba(231,76,60,0.5)'></div>Motion Cloud</div>"
      "</div></div>");

  // --- Three.js + OrbitControls from CDN ---
  server.sendContent("<script "
                     "src='https://cdnjs.cloudflare.com/ajax/libs/three.js/"
                     "r128/three.min.js'></script>"
                     "<script "
                     "src='https://cdn.jsdelivr.net/npm/three@0.128.0/examples/"
                     "js/controls/OrbitControls.min.js'></script>");

  // --- Main JavaScript (chunked for ESP32 RAM) ---
  // Part 1: Scene setup
  server.sendContent(
      "<script>"
      "const scene=new THREE.Scene();"
      "const camera=new "
      "THREE.PerspectiveCamera(60,innerWidth/innerHeight,0.1,100);"
      "camera.position.set(5,4,5);"
      "const renderer=new THREE.WebGLRenderer({antialias:true,alpha:true});"
      "renderer.setSize(innerWidth,innerHeight);"
      "renderer.setPixelRatio(Math.min(devicePixelRatio,2));"
      "renderer.setClearColor(0x0a0a0f);"
      "document.body.appendChild(renderer.domElement);"
      "const controls=new THREE.OrbitControls(camera,renderer.domElement);"
      "controls.enableDamping=true;controls.dampingFactor=0.08;"
      "controls.minDistance=2;controls.maxDistance=20;"
      "controls.target.set(0,1,0);"
      "window.addEventListener('resize',()=>{camera.aspect=innerWidth/"
      "innerHeight;"
      "camera.updateProjectionMatrix();renderer.setSize(innerWidth,innerHeight)"
      "});");

  // Part 2: Room + grid + lighting
  server.sendContent(
      "const roomW=6,roomD=5,roomH=3;"
      "const roomGeo=new THREE.BoxGeometry(roomW,roomH,roomD);"
      "const roomEdges=new THREE.EdgesGeometry(roomGeo);"
      "const roomLine=new THREE.LineSegments(roomEdges,"
      "new "
      "THREE.LineBasicMaterial({color:0x333355,transparent:true,opacity:0.4}));"
      "roomLine.position.y=roomH/2;scene.add(roomLine);"
      "const gridHelper=new THREE.GridHelper(8,16,0x222244,0x161630);"
      "scene.add(gridHelper);"
      "scene.add(new THREE.AmbientLight(0x334466,0.5));"
      "const pLight=new THREE.PointLight(0x9b59b6,1.5,15);"
      "pLight.position.set(0,4,0);scene.add(pLight);");

  // Part 3: ESP32 brain node at center
  server.sendContent(
      "const brainGeo=new THREE.OctahedronGeometry(0.15);"
      "const brainMat=new "
      "THREE.MeshPhongMaterial({color:0x2ecc71,emissive:0x2ecc71,"
      "emissiveIntensity:0.8});"
      "const brainNode=new THREE.Mesh(brainGeo,brainMat);"
      "brainNode.position.set(0,1,0);scene.add(brainNode);"
      // Brain glow ring
      "const brainRingGeo=new THREE.RingGeometry(0.2,0.25,32);"
      "const brainRingMat=new "
      "THREE.MeshBasicMaterial({color:0x2ecc71,transparent:true,opacity:0.3,"
      "side:THREE.DoubleSide});"
      "const brainRing=new THREE.Mesh(brainRingGeo,brainRingMat);"
      "brainRing.rotation.x=-Math.PI/"
      "2;brainRing.position.set(0,0.02,0);scene.add(brainRing);");

  // Part 4: AP node creation function + Fresnel zones
  server.sendContent(
      "const apNodes=[],apGlows=[],fresnelZones=[],apLabels=[];"
      "const apPositions=["
      "  new THREE.Vector3(2.5,1.5,2),"
      "  new THREE.Vector3(-2.5,2,1),"
      "  new THREE.Vector3(2,2.5,-2),"
      "  new THREE.Vector3(-2,1,-2),"
      "  new THREE.Vector3(0,2.8,0)"
      "];"
      "function createAPNode(idx){"
      "  const g=new THREE.SphereGeometry(0.12,16,16);"
      "  const m=new "
      "THREE.MeshPhongMaterial({color:0x3498db,emissive:0x3498db,"
      "emissiveIntensity:0.5});"
      "  const mesh=new THREE.Mesh(g,m);"
      "  mesh.position.copy(apPositions[idx%5]);"
      "  scene.add(mesh);apNodes.push(mesh);"
      // Glow halo
      "  const hg=new THREE.SphereGeometry(0.3,16,16);"
      "  const hm=new "
      "THREE.MeshBasicMaterial({color:0x3498db,transparent:true,opacity:0.15});"
      "  const halo=new THREE.Mesh(hg,hm);"
      "  halo.position.copy(apPositions[idx%5]);"
      "  scene.add(halo);apGlows.push(halo);"
      // Fresnel zone line to brain
      "  const pts=[apPositions[idx%5],new THREE.Vector3(0,1,0)];"
      "  const lineGeo=new THREE.BufferGeometry().setFromPoints(pts);"
      "  const lineMat=new "
      "THREE.LineBasicMaterial({color:0x9b59b6,transparent:true,opacity:0.2});"
      "  const line=new THREE.Line(lineGeo,lineMat);"
      "  scene.add(line);fresnelZones.push(line);"
      "}");

  // Part 5: Motion particle cloud
  server.sendContent(
      "const particleCount=200;"
      "const pGeo=new THREE.BufferGeometry();"
      "const pPositions=new Float32Array(particleCount*3);"
      "const pColors=new Float32Array(particleCount*3);"
      "for(let i=0;i<particleCount;i++){"
      "  pPositions[i*3]=(Math.random()-0.5)*2;"
      "  pPositions[i*3+1]=0.5+Math.random()*2;"
      "  pPositions[i*3+2]=(Math.random()-0.5)*2;"
      "  pColors[i*3]=0.9;pColors[i*3+1]=0.3;pColors[i*3+2]=0.2;"
      "}"
      "pGeo.setAttribute('position',new THREE.BufferAttribute(pPositions,3));"
      "pGeo.setAttribute('color',new THREE.BufferAttribute(pColors,3));"
      "const pMat=new "
      "THREE.PointsMaterial({size:0.06,transparent:true,opacity:0.0,"
      "vertexColors:true,blending:THREE.AdditiveBlending,depthWrite:false});"
      "const particles=new THREE.Points(pGeo,pMat);"
      "scene.add(particles);");

  // Part 6: Data fetching + update logic
  server.sendContent("let lastData=null,apCount=0,initialized=false;"
                     "async function fetchCSI(){"
                     "  try{"
                     "    const r=await fetch('/api/csi_data');"
                     "    lastData=await r.json();"
                     "    if(!initialized){initAPs(lastData);initialized=true;}"
                     "    updateScene(lastData);"
                     "  }catch(e){}"
                     "}"
                     "function initAPs(d){"
                     "  apCount=d.aps.length;"
                     "  for(let i=0;i<apCount;i++){createAPNode(i);}"
                     "  updateAPCards(d);"
                     "}"
                     "function lerpColor(a,b,t){"
                     "  const ar=(a>>16)&0xff,ag=(a>>8)&0xff,ab=a&0xff;"
                     "  const br=(b>>16)&0xff,bg=(b>>8)&0xff,bb=b&0xff;"
                     "  const rr=Math.round(ar+(br-ar)*t);"
                     "  const rg=Math.round(ag+(bg-ag)*t);"
                     "  const rb=Math.round(ab+(bb-ab)*t);"
                     "  return(rr<<16)|(rg<<8)|rb;"
                     "}");

  // Part 7: Scene update function
  server.sendContent(
      "function updateScene(d){"
      "  "
      "document.getElementById('score-val').textContent=d.gaitScore.toFixed(3);"
      "  "
      "document.getElementById('thresh-val').textContent=d.threshold.toFixed(3)"
      ";"
      "  const sv=document.getElementById('score-val');"
      "  const ratio=Math.min(d.gaitScore/d.threshold,1.5);"
      "  if(ratio>1)sv.style.color='#e74c3c';"
      "  else if(ratio>0.6)sv.style.color='#f39c12';"
      "  else sv.style.color='#2ecc71';"
      // Status pill
      "  const pill=document.getElementById('status-pill');"
      "  "
      "if(d.gaitScore>d.threshold&&d.armed){pill.className='alarm';pill."
      "textContent='ALARM';}"
      "  else if(d.armed){pill.className='armed';pill.textContent='ARMED';}"
      "  else{pill.className='disarmed';pill.textContent='DISARMED';}"
      // Update AP nodes
      "  let totalWeight=0;const center=new THREE.Vector3();"
      "  for(let i=0;i<d.aps.length&&i<apNodes.length;i++){"
      "    const s=d.aps[i].score;"
      "    const t=Math.min(s*3,1);"
      "    const col=lerpColor(0x3498db,0xe74c3c,t);"
      "    apNodes[i].material.color.setHex(col);"
      "    apNodes[i].material.emissive.setHex(col);"
      "    apNodes[i].material.emissiveIntensity=0.4+s*0.8;"
      "    const pulse=1+s*0.8;"
      "    apGlows[i].scale.setScalar(pulse);"
      "    apGlows[i].material.color.setHex(col);"
      "    apGlows[i].material.opacity=0.1+s*0.25;"
      "    fresnelZones[i].material.opacity=0.1+s*0.4;"
      "    "
      "fresnelZones[i].material.color.setHex(lerpColor(0x9b59b6,0xe74c3c,t));"
      "    const w=s*s;"
      "    center.add(apPositions[i%5].clone().multiplyScalar(w));"
      "    totalWeight+=w;"
      "  }"
      "  if(totalWeight>0.001)center.divideScalar(totalWeight);"
      "  else center.set(0,1,0);");

  // Part 8: Particle cloud update + AP cards
  server.sendContent(
      "  const maxScore=Math.max(...d.aps.map(a=>a.score),0.001);"
      "  particles.material.opacity=Math.min(maxScore*1.5,0.7);"
      "  const pos=particles.geometry.attributes.position.array;"
      "  const col=particles.geometry.attributes.color.array;"
      "  for(let i=0;i<particleCount;i++){"
      "    const ix=i*3;"
      "    pos[ix]+=(center.x-pos[ix])*0.03+(Math.random()-0.5)*0.08;"
      "    pos[ix+1]+=(center.y-pos[ix+1])*0.03+(Math.random()-0.5)*0.06;"
      "    pos[ix+2]+=(center.z-pos[ix+2])*0.03+(Math.random()-0.5)*0.08;"
      "    const intensity=maxScore;"
      "    col[ix]=0.2+intensity*0.7;"
      "    col[ix+1]=0.3*(1-intensity);"
      "    col[ix+2]=0.2*(1-intensity);"
      "  }"
      "  particles.geometry.attributes.position.needsUpdate=true;"
      "  particles.geometry.attributes.color.needsUpdate=true;"
      "  updateAPCards(d);"
      "}");

  // Part 9: AP info cards HTML
  server.sendContent(
      "function updateAPCards(d){"
      "  let h='<div class=\"hud-title\">AP Mesh ('+d.aps.length+')</div>';"
      "  d.aps.forEach((ap,i)=>{"
      "    const pct=Math.round(ap.score*100);"
      "    const "
      "barColor=ap.score>0.3?'#e74c3c':ap.score>0.1?'#f39c12':'#3498db';"
      "    h+='<div style=\"margin:6px 0;padding:4px 0;border-bottom:1px solid "
      "rgba(155,89,182,0.15)\">';"
      "    h+='<div "
      "style=\"display:flex;justify-content:space-between;font-size:11px\">';"
      "    h+='<span style=\"color:#9b59b6\">AP'+ap.id+'</span>';"
      "    h+='<span "
      "style=\"color:'+barColor+';font-weight:700\">'+pct+'%</span></div>';"
      "    if(ap.ch){"
      "      h+='<div class=\"bar-row\">';"
      "      const mx=Math.max(...ap.ch,0.1);"
      "      ap.ch.forEach(v=>{"
      "        const hPct=Math.round((v/mx)*100);"
      "        h+='<div class=\"bar\" "
      "style=\"height:'+hPct+'%;background:'+barColor+'\"></div>';"
      "      });"
      "      h+='</div>';"
      "    }"
      "    h+='</div>';"
      "  });"
      "  document.getElementById('ap-cards').innerHTML=h;"
      "}");

  // Part 10: Animation loop + start
  server.sendContent(
      "let t=0;"
      "function animate(){"
      "  requestAnimationFrame(animate);"
      "  t+=0.02;"
      "  brainNode.rotation.y=t;"
      "  brainRing.scale.setScalar(1+Math.sin(t*2)*0.15);"
      "  brainRing.material.opacity=0.2+Math.sin(t*3)*0.1;"
      "  if(lastData&&lastData.gaitScore>0.01){"
      "    pLight.intensity=1+lastData.gaitScore*2;"
      "    "
      "pLight.color.setHex(lerpColor(0x9b59b6,0xe74c3c,Math.min(lastData."
      "gaitScore*2,1)));"
      "  }"
      "  for(let i=0;i<apGlows.length;i++){"
      "    const s=apNodes[i]?1+Math.sin(t*3+i)*0.1:1;"
      "    apGlows[i].scale.setScalar(apGlows[i].scale.x*0.95+s*0.05);"
      "  }"
      "  controls.update();"
      "  renderer.render(scene,camera);"
      "}"
      "animate();"
      "setInterval(fetchCSI,200);"
      "fetchCSI();"
      "</script></body></html>");

  server.sendContent("");
}

inline void handleAddMac() {
  if (server.hasArg("new_mac")) {
    String macStr = server.arg("new_mac");
    macStr.trim();
    uint8_t mac[6];
    int values[6];
    if (6 == sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5])) {
      for (int i = 0; i < 6; ++i) {
        mac[i] = (uint8_t)values[i];
      }
      addSensingAP(mac);
    }
  }
  server.sendHeader("Location", "/settings");
  server.send(302, "text/plain", "");
}

inline void handleDeleteMac() {
  if (server.hasArg("id")) {
    extern void removeSensingAP(int index);
    int id = server.arg("id").toInt();
    removeSensingAP(id);
  }
  server.sendHeader("Location", "/settings");
  server.send(302, "text/plain", "");
}

inline void setupWebHandlers() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status_data", HTTP_GET, handleStatusData);
  server.on("/checkuid", HTTP_GET, handleCheckPendingUID);
  server.on("/resetwifi", HTTP_GET, handleResetWiFi);
  server.on("/setlang", HTTP_GET, handleSetLang);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/help", HTTP_GET, handleHelp);
  server.on("/alarm", HTTP_GET, handleWebStart);
  server.on("/stop", HTTP_GET, handleWebStop);
  server.on("/arm_mesh", HTTP_GET, handleArmMesh);
  server.on("/enroll", HTTP_GET, handleEnroll);
  server.on("/cancelenroll", HTTP_GET, handleCancelEnroll);
  server.on("/zones", HTTP_GET, handleZones);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/cards", HTTP_GET, handleCards);
  server.on("/deletezone", HTTP_GET, handleDeleteZone);
  server.on("/deletecard", HTTP_GET, handleDeleteCard);
  server.on("/savezone", HTTP_POST, handleSaveZone);
  server.on("/savecard", HTTP_POST, handleSaveCard);
  server.on("/savesettings", HTTP_POST, handleSaveSettings);
  server.on("/add_mac", HTTP_POST, handleAddMac);
  server.on("/del_mac", HTTP_GET, handleDeleteMac);
  server.on("/set_sens", HTTP_GET, handleSetSens);
  server.on("/toggle_dev", HTTP_GET, handleToggleDev);
  server.on("/api/ir", HTTP_GET, handleIRCommand);
  server.on("/api/learn", HTTP_GET, handleIRLearnCmd);
  server.on("/api/learn_status", HTTP_GET, handleIRLearnCheck);
  server.on("/api/csi_data", HTTP_GET, handleCSIData);
  server.on("/radar", HTTP_GET, handleRadar);
}