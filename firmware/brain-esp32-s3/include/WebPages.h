#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <IRsend.h>

#include "Globals.h"
#include "IRManager.h"
#include "RFIDManager.h"

extern bool devModeActive;
extern int webSoundSensitivity;
extern WebServer server;
extern Preferences preferences;
extern volatile bool systemArmed;
extern volatile bool isAlarmActive;
extern bool isSirenDetected;
extern volatile bool isEnrollMode;
extern bool isGreek;
extern String telegramToken;
extern String telegramChatID;
extern String callMeBotUser;
extern String targetSpeaker;
extern void saveCards(); 
extern volatile float remoteBatteryVoltage;
extern int getBatteryPercentage(float voltage);

// Note: urlDecode and getPostArg functions were removed! 
// WebServer handles URL decoding and POST arguments automatically.

inline String T(String en, String el) {
  return isGreek ? el : en;
}

inline String getStyles() {
  String s = "<style>body{font-family: Arial; text-align: center; margin-top: 50px; background-color: #222; color: white;} ";
  s += ".btn{padding: 15px 30px; font-size: 18px; margin: 10px; cursor: pointer; border-radius: 8px; border: none; font-weight: bold; text-decoration: none; display: inline-block;} ";
  s += ".btn-red{background-color: #e74c3c; color: white;} .btn-green{background-color: #2ecc71; color: white;} ";
  s += ".btn-blue{background-color: #3498db; color: white;} .btn-gray{background-color: #7f8c8d; color: white;} ";
  s += ".btn-orange{background-color: #e67e22; color: white;} ";
  s += "table {margin: 20px auto; border-collapse: collapse; width: 90%; max-width: 600px; background-color: #333;} ";
  s += "th, td {padding: 12px; border: 1px solid #555;} th {background-color: #444;} ";
  s += "input[type='text'], input[type='password'], input[type='number'] { width: 90%; max-width: 300px; padding: 12px; margin: 10px 0; border-radius: 6px; border: 1px solid #555; background-color: #444; color: white; font-size: 16px; }</style>";
  s += ".remote-box {background-color: #2c3e50; padding: 20px 10px; border-radius: 12px; border: 2px solid #34495e; max-width: 380px; margin: 30px auto; box-shadow: 0 4px 8px rgba(0,0,0,0.5);} ";
  s += ".btn-ir {width: 42%; margin: 6px; padding: 12px 0; font-size: 16px; border-radius: 8px; font-weight: bold; cursor: pointer; border: none; color: white;} ";
  s += ".btn-ir:active {opacity: 0.7; transform: scale(0.95);} ";
  s += "#learnOverlay { display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.85); z-index:1000; color:#f39c12; padding-top:50%; font-size:24px; font-weight:bold; } ";
  s += "</style>";
  return s;
}

inline void handleSetLang() {
  isGreek = !isGreek; 
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

inline void handleRoot() {
  String soundStatus = (isSirenDetected) ? T("<span style='color:red;'>YES</span>", "<span style='color:red;'>ΝΑΙ</span>") : T("<span style='color:green;'>NO</span>", "<span style='color:green;'>ΟΧΙ</span>");
  String armedStatus = (systemArmed) ? T("<span style='color:green;'>ARMED</span>", "<span style='color:green;'>ΟΠΛΙΣΜΕΝΟ</span>") : T("<span style='color:orange;'>DISARMED</span>", "<span style='color:orange;'>ΑΦΟΠΛΙΣΜΕΝΟ</span>");

  String battDisplay = T("Waiting for data...", "Αναμονή δεδομένων...");
  if (remoteBatteryVoltage > 0.0) {
    int pct = getBatteryPercentage(remoteBatteryVoltage);
    String color = (pct > 20) ? "#2ecc71" : "#e74c3c";
    battDisplay = "<span style='color:" + color + "; font-weight:bold;'>" + String(remoteBatteryVoltage) + "V (" + String(pct) + "%)</span>";
  }

  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += getStyles() + "</head><body>";
  
  html += "<div style='text-align:right; padding:10px; max-width:600px; margin:0 auto;'>";
  html += "<a href='/setlang' style='color:#3498db; text-decoration:none; font-weight:bold; font-size:18px;'>" + T("Ελληνικά GR", "English EN") + "</a></div>";
  
  html += "<h1>" + T("Security System Panel", "Πίνακας Συστήματος") + "</h1>";
  html += "<h2>" + T("System Status: ", "Κατάσταση: ") + "<span id='status-armed'>" + armedStatus + "</span></h2>";
  html += "<h2>" + T("Alarm Blasting? ", "Χτυπάει; ") + "<span id='status-siren'>" + soundStatus + "</span></h2>";
  html += "<h2>" + T("Gate Battery: ", "Μπαταρία Πύλης: ") + "<span id='status-batt'>" + battDisplay + "</span></h2>";
  
  html += "<a href='/alarm' class='btn btn-red'>" + T("START ALARM", "ΕΝΑΡΞΗ") + "</a><br>";
  html += "<a href='/stop' class='btn btn-green'>" + T("STOP ALARM", "ΔΙΑΚΟΠΗ") + "</a><br><hr style='border-color:#444; width:80%; margin: 20px auto;'>";
  html += "<a href='/logs' class='btn btn-blue'>" + T("LOGS", "ΙΣΤΟΡΙΚΟ") + "</a>";
  html += "<a href='/cards' class='btn btn-gray'>" + T("CARDS", "ΚΑΡΤΕΣ") + "</a><br>";
  html += "<a href='/zones' class='btn btn-blue'>" + T("ZONES", "ΖΩΝΕΣ") + "</a><br>";
  html += "<a href='/settings' class='btn btn-orange'>" + T("WIFI SETUP", "ΡΥΘΜΙΣΕΙΣ WIFI") + "</a>";
  html += "<a href='/help' class='btn btn-gray'>" + T("HELP", "ΒΟΗΘΕΙΑ") + "</a>";

html += "<div id='learnOverlay'>📡 " + T("Waiting for remote...", "Αναμονή για τηλεχειριστήριο...") + "<br><span style='font-size:16px; color:#aaa;'>" + T("Point remote at ESP32 and press the button", "Στοχεύστε το ESP32 και πατήστε το κουμπί") + "</span></div>";

  // --- VIRTUAL REMOTE CONTROL UI ---
  html += "<div class='remote-box' id='remoteBox'>";
  html += "<h3 style='color:#ecf0f1; margin-top:0;'>🔊 " + T("Speaker Remote", "Τηλεχειριστήριο Ηχείου") + "</h3>";
  
  html += "<button class='btn-ir' style='background-color:#e74c3c;' onclick='sendIR(\"power\")'>🔌 " + T("Power", "On/Off") + "</button>";
  html += "<button class='btn-ir' style='background-color:#f39c12;' onclick='sendIR(\"mute\")'>🔇 " + T("Mute", "Σίγαση") + "</button><br>";
  
  html += "<button class='btn-ir' style='background-color:#3498db;' onclick='sendIR(\"voldown\")'>🔉 " + T("Vol -", "Ένταση -") + "</button>";
  html += "<button class='btn-ir' style='background-color:#3498db;' onclick='sendIR(\"volup\")'>🔊 " + T("Vol +", "Ένταση +") + "</button><br>";
  
  html += "<button class='btn-ir' style='background-color:#9b59b6;' onclick='sendIR(\"pcin\")'>💻 " + T("PC Input", "Είσοδος PC") + "</button>";
  html += "<button class='btn-ir' style='background-color:#9b59b6;' onclick='sendIR(\"btin\")'>📱 " + T("BT Input", "Είσοδος BT") + "</button><br>";
  
  html += "<button class='btn-ir' style='background-color:#2ecc71; width:88%; margin-top:10px;' onclick='sendIR(\"playpause\")'>⏯️ " + T("Play / Pause", "Αναπαραγωγή / Παύση") + "</button>";
  
  html += "<hr style='border-color:#555; margin: 15px auto; width: 90%;'>";
  html += "<button id='learnBtn' class='btn btn-orange' style='width:90%; font-size:14px;' onclick='toggleLearnMode()'>🎓 " + T("ENTER LEARN MODE", "ΛΕΙΤΟΥΡΓΙΑ ΕΚΜΑΘΗΣΗΣ") + "</button>";
  html += "</div>";

  // --- JAVASCRIPT ---
  html += "<script>";
  html += "var isGr = " + String(isGreek ? "true" : "false") + ";";
  html += "var isLearningMode = false;";
  
  html += "function toggleLearnMode() {";
  html += "  isLearningMode = !isLearningMode;";
  html += "  var btn = document.getElementById('learnBtn');";
  html += "  var box = document.getElementById('remoteBox');";
  html += "  if (isLearningMode) {";
  html += "    btn.innerHTML = isGr ? '🛑 ΑΚΥΡΩΣΗ ΕΚΜΑΘΗΣΗΣ' : '🛑 CANCEL LEARNING';";
  html += "    btn.className = 'btn btn-red';";
  html += "    box.style.border = '4px solid #f39c12';";
  html += "  } else {";
  html += "    btn.innerHTML = isGr ? '🎓 ΛΕΙΤΟΥΡΓΙΑ ΕΚΜΑΘΗΣΗΣ' : '🎓 ENTER LEARN MODE';";
  html += "    btn.className = 'btn btn-orange';";
  html += "    box.style.border = '2px solid #34495e';";
  html += "  }";
  html += "}";

  html += "function sendIR(cmd) {";
  html += "  if (isLearningMode) {";
  html += "    fetch('/api/learn?btn=' + cmd).then(() => {";
  html += "      document.getElementById('learnOverlay').style.display = 'block';";
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
  html += "        document.getElementById('learnOverlay').style.display = 'none';";
  html += "        toggleLearnMode();"; // Turn off learn mode automatically
  html += "        alert(isGr ? 'Επιτυχία! Ο κωδικός αποθηκεύτηκε.' : 'Success! IR Code saved permanently.');";
  html += "      }";
  html += "    });";
  html += "  }, 1000);"; // Check every 1 second
  html += "}";

  html += "<hr style='border-color:#444; width:80%; margin: 20px auto;'>";
  html += "<h2>" + T("Mic Sensitivity", "Ευαισθησία Μικροφώνου") + "</h2>";
  html += "<p style='color:#aaa; font-size:12px;'>" + T("Lower = More Sensitive. Higher = Less Sensitive.", "Μικρότερο = Πιο Ευαίσθητο. Μεγαλύτερο = Λιγότερο Ευαίσθητο.") + "</p>";
  
  html += "<input type='range' min='20' max='150' value='" + String(webSoundSensitivity) + "' ";
  html += "onchange='fetch(\"/set_sens?val=\" + this.value)' style='width: 80%; max-width: 400px;'>";
  html += "<br><hr style='border-color:#444; width:80%; margin: 20px auto;'>";
  
  if (devModeActive) {
    html += "<a href='/toggle_dev' class='btn btn-red'>" + T("DISABLE DEV MODE", "ΑΠΕΝΕΡΓ. DEV MODE") + "</a>";
  } else {
    html += "<a href='/toggle_dev' class='btn btn-gray'>" + T("ENABLE DEV MODE", "ΕΝΕΡΓΟΠ. DEV MODE") + "</a>";
  }

  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

inline void handleResetWiFi() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyles();
  html += "</head><body>"; 
  html += "<h1 style='color:#e74c3c;'>" + T("WiFi Erased! Rebooting...", "Το WiFi διαγράφηκε! Επανεκκίνηση...") + "</h1>";
  html += "<h2>" + T("Look for 'SecuritySystem-Setup' on your phone.", "Ψάξτε το δίκτυο 'SecuritySystem-Setup' στο κινητό σας.") + "</h2>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  delay(2000);
  WiFi.disconnect(true, true); 
  delay(1000);
  ESP.restart(); 
}

inline void handleSettings() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyles() + "</head><body>";
  html += "<h1>" + T("System Configuration", "Ρυθμίσεις Συστήματος") + "</h1>";
  html += "<div style='background:#333; padding:20px; max-width: 400px; margin: 0 auto; border-radius: 8px;'>";
  String currentSSID = WiFi.SSID();
  if (currentSSID == "") currentSSID = T("Unknown", "Άγνωστο");
  html += "<h3 style='color:#2ecc71; margin-bottom: 5px;'>" + T("📡 Connected to: ", "📡 Συνδεδεμένο στο: ") + currentSSID + "</h3>";
  html += "<div style='background:#333; padding:20px; max-width: 400px; margin: 20px auto 0 auto; border-radius: 8px;'>";
  html += "<form action='/savesettings' method='POST'>";
  html += "<h3 style='color:#e67e22;'>" + T("Telegram Settings", "Ρυθμίσεις Telegram") + "</h3>";
  html += "<strong>" + T("Bot Token", "Bot Token") + "</strong><br>";
  html += "<input type='text' name='bot_token' value='" + telegramToken + "' required><br>";
  html += "<strong>" + T("Chat ID", "Chat ID") + "</strong><br>";
  html += "<input type='text' name='chat_id' value='" + telegramChatID + "' required><br><br>";
  html += "<strong>" + T("CallMeBot Username", "Όνομα Χρήστη CallMeBot") + "</strong><br>";
  html += "<input type='text' name='cmb_user' value='" + callMeBotUser + "' placeholder='@YourUsername' required><br><br>";
  html += "<hr style='border-color:#555; margin: 20px 0;'>";
  html += "<h3 style='color:#9b59b6;'>" + T("Bluetooth Speaker", "Ηχείο Bluetooth") + "</h3>";
  html += "<strong>" + T("Exact Speaker Name", "Ακριβές Όνομα Ηχείου") + "</strong><br>";
  html += "<input type='text' name='bt_speaker' value='" + targetSpeaker + "' placeholder=''><br><br>";
  html += "<input type='submit' class='btn btn-green' value='" + T("SAVE & RESTART", "ΑΠΟΘΗΚΕΥΣΗ & ΕΠΑΝΕΚΚΙΝΗΣΗ") + "'>";
  html += "</form>";
  html += "<hr style='border-color:#e74c3c; margin: 30px 0 20px 0;'>";
  html += "<h3 style='color:#e74c3c;'>" + T("Danger Zone", "Επικίνδυνη Ζώνη") + "</h3>";
  String confirmMsg = T("Are you sure? This will disconnect the system from WiFi and reboot into Setup Mode.", "Είστε σίγουροι; Το σύστημα θα αποσυνδεθεί και θα μπει σε λειτουργία ρύθμισης.");
  html += "<a href='/resetwifi' class='btn btn-red' onclick=\"return confirm('" + confirmMsg + "');\">" + T("RESET WIFI", "ΕΠΑΝΑΦΟΡΑ WIFI") + "</a><br><br>";
  html += "</div><br><a href='/' class='btn btn-gray'>" + T("BACK", "ΠΙΣΩ") + "</a></body></html>";
  
  server.send(200, "text/html", html);
}

inline void handleHelp() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyles() + "</head><body>";
  html += "<h1>" + T("Setup & Help Guide", "Οδηγός Εγκατάστασης & Βοήθεια") + "</h1>";
  html += "<div style='background:#333; padding:20px; max-width:600px; margin:0 auto; text-align:left; border-radius:8px; line-height: 1.6;'>";
  html += "<h2 style='color:#2ecc71; margin-bottom: 5px;'>" + T("🎛️ Dashboard Controls", "🎛️ Έλεγχος Συστήματος") + "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li><b>" + T("Start/Stop Alarm:", "Έναρξη/Διακοπή:") + "</b> " + T("Manually trigger or disarm the siren.", "Χειροκίνητη ενεργοποίηση ή αφοπλισμός της σειρήνας.") + "</li>";
  html += "<li><b>" + T("Logs:", "Ιστορικό:") + "</b> " + T("View past alarm triggers and RFID scans.", "Δείτε προηγούμενα συμβάντα και σκαναρίσματα καρτών.") + "</li>";
  html += "<li><b>" + T("Cards:", "Κάρτες:") + "</b> " + T("Manage authorized RFID keys.", "Διαχείριση εξουσιοδοτημένων καρτών RFID.") + "</li>";
  html += "</ul>";
  html += "<h2 style='color:#3498db; margin-bottom: 5px;'>" + T("🤖 Notifications Setup", "🤖 Ειδοποιήσεις") + "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li><b>Telegram Bot Token:</b> " + T("Search for <b>@BotFather</b> on Telegram and send <code>/newbot</code> to generate your token.", "Ψάξτε το <b>@BotFather</b> στο Telegram και στείλτε <code>/newbot</code> για να πάρετε το token.") + "</li>";
  html += "<li><b>Chat ID:</b> " + T("Search for <b>@userinfobot</b> on Telegram to find your personal ID number.", "Ψάξτε το <b>@userinfobot</b> στο Telegram για να βρείτε το προσωπικό σας ID.") + "</li>";
  html += "<li><b>CallMeBot Username:</b> " + T("To get phone calls during an alarm, message <b>@CallMeBot_API</b> on Telegram to activate, then enter your @username here.", "Για τηλεφωνικές κλήσεις, στείλτε μήνυμα στο <b>@CallMeBot_API</b> για έγκριση και βάλτε το @username σας εδώ.") + "</li>";
  html += "</ul>";
  html += "<h2 style='color:#9b59b6; margin-bottom: 5px;'>" + T("🔊 Bluetooth Speaker", "🔊 Ηχείο Bluetooth") + "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li>" + T("Enter the EXACT pairing name of your Bluetooth speaker so the audio system can connect automatically.", "Βάλτε το ΑΚΡΙΒΕΣ όνομα του ηχείου σας ώστε το σύστημα ήχου να συνδεθεί αυτόματα.") + "</li>";
  html += "</ul>";
  html += "<h2 style='color:#e74c3c; margin-bottom: 5px;'>" + T("🌐 WiFi & Reset", "🌐 WiFi & Επαναφορά") + "</h2>";
  html += "<ul style='margin-top: 5px;'>";
  html += "<li>" + T("If you change your home router, go to Settings -> <b>Danger Zone</b> and click <b>Reset WiFi</b>.", "Αν αλλάξετε ρούτερ, πηγαίνετε στις Ρυθμίσεις -> <b>Επικίνδυνη Ζώνη</b> και πατήστε <b>Επαναφορά WiFi</b>.") + "</li>";
  html += "<li>" + T("The system will reboot and broadcast a network called <code>SecuritySystem-Setup</code> (Password: <code>12345678</code>) so you can connect it to the new router.", "Το σύστημα θα κάνει επανεκκίνηση και θα δημιουργήσει το δίκτυο <code>SecuritySystem-Setup</code> (Κωδικός: <code>12345678</code>) για να βάλετε το νέο ρούτερ.") + "</li>";
  html += "</ul>";
  html += "</div><br><a href='/' class='btn btn-gray'>" + T("BACK TO HOME", "ΕΠΙΣΤΡΟΦΗ") + "</a></body></html>";
  
  server.send(200, "text/html", html);
}

extern void triggerSeinagermos(String reason);
extern void disarmSystem();

inline void handleWebStart() {
  triggerSeinagermos("Web Interface Button");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

inline void handleWebStop() {
  disarmSystem();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

inline void handleEnroll() {
  isEnrollMode = true; pendingUID = "";
  server.sendHeader("Location", "/cards");
  server.send(302, "text/plain", "");
}

inline void handleCancelEnroll() {
  isEnrollMode = false; pendingUID = "";
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
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyles() + "</head><body>";
  html += "<h1>" + T("System Logs", "Ιστορικό Συστήματος") + "</h1>";
  html += "<div style='background:#333; padding:20px; max-width:600px; margin:0 auto; border-radius:8px; text-align:left;'>";
  
  // Send in chunks to save RAM
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);

  bool hasLogs = false;
  for (int i = 0; i < MAX_LOGS; i++) {
    int idx = (logIndex - 1 - i + MAX_LOGS) % MAX_LOGS;
    if (rfidLogs[idx] != "" && rfidLogs[idx].length() > 2) {
      hasLogs = true;
      String logLine = "<div style='border-bottom: 1px solid #444; padding: 8px 0; font-family: monospace; font-size: 14px;'>";
      logLine += "<span style='color:#3498db;'>&bull;</span> " + rfidLogs[idx] + "</div>";
      server.sendContent(logLine);
    }
  }

  if (!hasLogs) {
    server.sendContent("<p style='text-align:center;'><i>" + T("No logs recorded yet.", "Δεν υπάρχει ιστορικό ακόμα.") + "</i></p>");
  }

  server.sendContent("</div><br><a href='/' class='btn btn-gray'>" + T("BACK", "ΠΙΣΩ") + "</a></body></html>");
  server.sendContent(""); // End chunked transmission
}

inline void handleCards() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyles() + "</head><body>";
  html += "<h1>" + T("Enrolled Cards", "Εγγεγραμμένες Κάρτες") + "</h1>";
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);

  if (isEnrollMode) {
    String enrollBox = "<div style='background:#333; padding:20px; max-width:500px; margin:0 auto 20px auto; border-radius:8px; border: 2px solid #e67e22;'>";
    if (pendingUID == "") {
      enrollBox += "<h3 style='color:#e67e22;'>" + T("WAITING FOR SCAN...", "ΑΝΑΜΟΝΗ ΓΙΑ ΚΑΡΤΑ...") + "</h3>";
      enrollBox += "<p>" + T("Please tap your card on the reader now.", "Παρακαλώ σκανάρετε την κάρτα σας στον αναγνώστη.") + "</p>";
      enrollBox += "<br><a href='/scanner' target='_blank' class='btn btn-blue'>" + T("OPEN QR SCANNER", "ΑΝΟΙΓΜΑ ΣΑΡΩΤΗ QR") + "</a><br>";
      enrollBox += "<script>";
      enrollBox += "setInterval(function() {";
      enrollBox += "  fetch('/checkuid').then(r => r.json()).then(data => {";
      enrollBox += "    if (data.uid !== '') { window.location.reload(); }";
      enrollBox += "  });";
      enrollBox += "}, 1000);"; 
      enrollBox += "</script>";
    } else {
      enrollBox += "<h3 style='color:#2ecc71;'>" + T("CARD DETECTED!", "Η ΚΑΡΤΑ ΒΡΕΘΗΚΕ!") + "</h3>";
      enrollBox += "<form action='/savecard' method='POST'>";
      enrollBox += "<input type='hidden' name='uid' value='" + pendingUID + "'>";
      enrollBox += "<strong>" + T("Enter Owner Name:", "Όνομα Κατόχου:") + "</strong><br>";
      enrollBox += "<input type='text' name='name' autofocus required><br><br>";
      enrollBox += "<strong>" + T("Allowed Access Zones:", "Επιτρεπόμενες Ζώνες Πρόσβασης:") + "</strong><br>";
      enrollBox += "<div style='text-align: left; padding-left: 20px;'>";
      
      if (zoneCount == 0) {
         enrollBox += "<span style='color:#e74c3c;'>" + T("No zones created yet! Go to Zones menu first.", "Δεν έχουν δημιουργηθεί ζώνες! Πηγαίνετε στο μενού Ζώνες.") + "</span><br>";
      } else {
        for (int i = 0; i < zoneCount; i++) {
          if (systemZones[i].isActive) {
             enrollBox += "<input type='checkbox' name='z" + String(i) + "' value='1' checked> " + systemZones[i].name + "<br>";
          }
        }
      }
      enrollBox += "</div><br>";
      enrollBox += "<input type='submit' class='btn btn-green' value='" + T("SAVE CARD", "ΑΠΟΘΗΚΕΥΣΗ") + "'>";
      enrollBox += "</form>";
    }
    enrollBox += "<br><a href='/cancelenroll' style='color:#e74c3c; text-decoration:none;'>" + T("Cancel", "Ακύρωση") + "</a>";
    enrollBox += "</div>";
    server.sendContent(enrollBox);
  } else {
    server.sendContent("<a href='/enroll' class='btn btn-blue'>" + T("ADD NEW CARD", "ΠΡΟΣΘΗΚΗ") + "</a><br><hr style='border-color:#444; width:80%; margin: 20px auto;'>");
  }

  server.sendContent("<table><tr style='background:#444;'><th>" + T("Name", "Όνομα") + "</th><th>UID</th><th>" + T("Zones", "Ζώνες") + "</th><th>" + T("Action", "Ενέργεια") + "</th></tr>");

  for (int i = 0; i < cardCount; i++) {
    String row = "<tr>";
    row += "<td>" + enrolledCards[i].name + "</td>";
    row += "<td style='font-family:monospace; color:#aaa;'>" + enrolledCards[i].uid + "</td>";
    
    String allowedZonesStr = "";
    bool firstZone = true;
    for(int z = 0; z < zoneCount; z++) {
        if(systemZones[z].isActive && (enrolledCards[i].allowedZones & (1 << z))) {
            if(!firstZone) allowedZonesStr += ", ";
            allowedZonesStr += systemZones[z].name;
            firstZone = false;
        }
    }
    if(allowedZonesStr == "") allowedZonesStr = "-";
    
    row += "<td style='font-size: 14px;'>" + allowedZonesStr + "</td>";
    row += "<td><a href='/deletecard?uid=" + enrolledCards[i].uid + "' style='color:#e74c3c; text-decoration:none; font-weight:bold;'>[X]</a></td></tr>";
    server.sendContent(row);
  }

  server.sendContent("</table><br><a href='/' class='btn btn-gray'>" + T("BACK", "ΠΙΣΩ") + "</a></body></html>");
  server.sendContent("");
}

inline void handleZones() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" + getStyles() + "</head><body>";
  html += "<h1>" + T("Access Zones", "Ζώνες Πρόσβασης") + "</h1>";
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);

  String addBox = "<div style='background:#333; padding:20px; max-width:500px; margin:0 auto 20px auto; border-radius:8px; border: 1px solid #3498db;'>";
  addBox += "<h3>" + T("Add New Door Zone", "Προσθήκη Νέας Ζώνης") + "</h3>";
  addBox += "<form action='/savezone' method='POST'>";
  addBox += "<strong>" + T("Zone Name:", "Όνομα Ζώνης:") + "</strong><br>";
  addBox += "<input type='text' name='zname' required><br>";
  addBox += "<strong>" + T("Reader SS/SDA Pin:", "Pin SS/SDA Αναγνώστη:") + "</strong><br>";
  addBox += "<input type='number' name='zss' value='21' required><br>";
  addBox += "<strong>" + T("Door Relay Pin (-1 for none):", "Pin Ρελέ Πόρτας (-1 για κανένα):") + "</strong><br>";
  addBox += "<input type='number' name='zpin' value='-1' required><br><br>";
  addBox += "<input type='submit' class='btn btn-blue' value='" + T("ADD ZONE", "ΠΡΟΣΘΗΚΗ") + "'>";
  addBox += "</form></div>";
  server.sendContent(addBox);

  server.sendContent("<table><tr style='background:#444;'><th>ID</th><th>" + T("Name", "Όνομα") + "</th><th>SS Pin</th><th>Relay</th><th>" + T("Action", "Ενέργεια") + "</th></tr>");

  bool hasZones = false;
  for (int i = 0; i < MAX_ZONES; i++) {
    if (i < zoneCount && systemZones[i].isActive) {
      hasZones = true;
      String row = "<tr>";
      row += "<td style='color:#aaa;'>" + String(i) + "</td>";
      row += "<td>" + systemZones[i].name + "</td>";
      row += "<td>" + String(systemZones[i].ssPin) + "</td>";
      row += "<td>" + String(systemZones[i].relayPin) + "</td>";
      row += "<td><a href='/deletezone?id=" + String(i) + "' style='color:#e74c3c; text-decoration:none; font-weight:bold;'>[X]</a></td></tr>";
      server.sendContent(row);
    }
  }
  
  if (!hasZones) {
    server.sendContent("<tr><td colspan='4'>" + T("No zones defined.", "Δεν υπάρχουν ζώνες.") + "</td></tr>");
  }

  server.sendContent("</table><br><a href='/' class='btn btn-gray'>" + T("BACK", "ΠΙΣΩ") + "</a></body></html>");
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
        mfrc522[slot].PCD_Init(systemZones[slot].ssPin, LOCAL_RST_PIN);
      }
      if (systemZones[slot].relayPin >= 0) {
        pinMode(systemZones[slot].relayPin, OUTPUT);
        digitalWrite(systemZones[slot].relayPin, LOW);
      }

      if (slot >= zoneCount) zoneCount = slot + 1;

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
      if (enrolledCards[i].uid == uidToDelete) { foundIndex = i; break; }
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
    
    Serial.println("Mic Sensitivity updated to: " + String(webSoundSensitivity));
  }
  server.send(200, "text/plain", "OK");
}

inline void handleToggleDev() {
  extern bool devModeActive;
  devModeActive = !devModeActive; // Flip the state
  
  // Refresh the page so the button changes color
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
  // If the main loop cleared the variable, learning is done!
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
    
    // We just tell the manager to handle it!
    sendIRCommand(cmd); 
    
    delay(100); 
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing command");
  }
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
  server.on("/enroll", HTTP_GET, handleEnroll);
  server.on("/cancelenroll", HTTP_GET, handleCancelEnroll);
  server.on("/zones", HTTP_GET, handleZones);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/cards", HTTP_GET, handleCards);
  server.on("/deletezone", HTTP_GET, handleDeleteZone);
  server.on("/deletecard", HTTP_GET, handleDeleteCard);
  server.on("/savezone", HTTP_POST, handleSaveZone);
  server.on("/savecard", HTTP_POST, handleSaveCard);
  server.on("/set_sens", HTTP_GET, handleSetSens);
  server.on("/toggle_dev", HTTP_GET, handleToggleDev);
  server.on("/api/ir", HTTP_GET, handleIRCommand);
  server.on("/api/learn", HTTP_GET, handleIRLearnCmd);
  server.on("/api/learn_status", HTTP_GET, handleIRLearnCheck);
}