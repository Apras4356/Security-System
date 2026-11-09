#pragma once

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;
extern bool isGreek;
extern volatile bool isEnrollMode;
extern String pendingUID;
extern void processCardStatus(String scannedUID, bool isRemote, int zoneID);

inline String T_QR(String en, String el) {
  return isGreek ? el : en;
}

inline void handleScannerApp() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>";
  html += "<script src='https://unpkg.com/html5-qrcode'></script>";
  
  html += "<style>body{font-family: Arial, sans-serif; background-color: #111; color: white; text-align: center; margin: 0; padding: 20px;} ";
  html += "h1 { color: #3498db; margin-bottom: 5px; }";
  html += "p { color: #aaa; margin-bottom: 20px; }";
  html += "#reader { width: 100%; max-width: 400px; margin: 0 auto; border: 4px solid #333; border-radius: 12px; background: #000; overflow: hidden; } ";
  html += "</style></head><body>";
  
  html += "<h1>" + T_QR("Access Control", "Έλεγχος Πρόσβασης") + "</h1>";
  html += "<p>" + T_QR("Please present your QR code to the camera.", "Παρακαλώ δείξτε το QR code στην κάμερα.") + "</p>";
  
  html += "<div id='reader'></div>";

  html += "<script>";
  html += "function onScanSuccess(decodedText, decodedResult) {";
  html += "  html5QrcodeScanner.clear();";
  html += "  document.body.innerHTML = '<h1 style=\"color:#2ecc71; margin-top:50px;\">" + T_QR("SCANNED!", "ΕΠΙΤΥΧΙΑ!") + "</h1><p>" + T_QR("Processing...", "Επεξεργασία...") + "</p>';";
  html += "  window.location.href = '/qrprocess?uid=' + encodeURIComponent(decodedText);";
  html += "}";
  html += "let html5QrcodeScanner = new Html5QrcodeScanner('reader', { fps: 10, qrbox: {width: 250, height: 250} }, false);";
  html += "html5QrcodeScanner.render(onScanSuccess);";
  html += "</script>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}
    
inline void handleQRProcess() {
  if (server.hasArg("uid")) {
    String scannedQR = server.arg("uid");
    scannedQR.trim();
    scannedQR.toUpperCase();
    
    if (isEnrollMode) {
      pendingUID = scannedQR;
      server.sendHeader("Location", "/cards", true);
    } else {
      processCardStatus(scannedQR, false, 0); 
      server.sendHeader("Location", "/scanner", true); 
    }
    server.send(302, "text/plain", "");
  } else {
    server.sendHeader("Location", "/scanner", true);
    server.send(302, "text/plain", "");
  }
}

inline void setupQRHandlers() {
  server.on("/scanner", HTTP_GET, handleScannerApp);
  server.on("/qrprocess", HTTP_GET, handleQRProcess);
}