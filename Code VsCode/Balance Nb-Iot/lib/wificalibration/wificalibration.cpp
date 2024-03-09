#include "wificalibration.h"
#include "HX711.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"
#include <Arduino.h>
#include "ttgopower.h"
XPowersPMU PMU;
HX711 myScale1;
HX711 myScale2;

uint8_t dataPin1 = 13;
uint8_t clockPin1 = 14;
uint8_t dataPin2 = 11;
uint8_t clockPin2 = 12;

const char* ssid = "balance";
const char* password = "123456789";

AsyncWebServer server(80);
Preferences preferences;

void handleRoot(AsyncWebServerRequest *request) {
  String html = "<h1>Calibration des balances</h1>";
  html += "<p>Choisir la balance à calibrer</p>";
   html += "<p>Enlever tous poids sur les balances</p>";
  html += "<form action=\"/calibrate1\" method=\"get\">";
  html += "<input type=\"submit\" value=\"Calibration balance 1\">";
  html += "</form>";
  html += "<form action=\"/calibrate2\" method=\"get\">";
  html += "<input type=\"submit\" value=\"Calibration balance 2\">";
  html += "</form>";
  request->send(200, "text/html; charset=UTF-8", html);
}

void handleCalibration1(AsyncWebServerRequest *request) {
  myScale1.tare(20);  // average 20 measurements.
  uint32_t offset = myScale1.get_offset();
  String html = "<h1>Calibration balance 1</h1>";
  html += "<p>Offset: " + String(offset) + "</p>";
  html += "<p>Placer un poids sur la balance (en gramme)</p>";
  html += "<form action=\"/weight1\" method=\"get\">";
  html += "<input type=\"text\" name=\"weight\">";
  html += "<input type=\"submit\" value=\"Enter\">";
  html += "</form>";
  request->send(200, "text/html; charset=UTF-8", html);
}

void handleCalibration2(AsyncWebServerRequest *request) {
  myScale2.tare(20);  // average 20 measurements.
  uint32_t offset = myScale2.get_offset();
  String html = "<h1>Calibration balance 2</h1>";
  html += "<p>Offset: " + String(offset) + "</p>";
  html += "<p>Placer un poids sur la balance (en gramme)</p>";
  html += "<form action=\"/weight2\" method=\"get\">";
  html += "<input type=\"text\" name=\"weight\">";
  html += "<input type=\"submit\" value=\"Enter\">";
  html += "</form>";
  request->send(200, "text/html; charset=UTF-8", html);
}

void handleWeight1(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    myScale1.calibrate_scale(weight, 20);
    float scale = myScale1.get_scale();
    uint32_t offset = myScale1.get_offset();
    message = "<p>Calibration terminée. Factor 1: " + String(scale, 6) + ", Offset 1: " + String(offset) + "</p>";

    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor1", scale);
    preferences.end();
    // Restart ESP32
   // esp_restart();
  } else {
    message = "<p>Missing weight parameter</p>";
  }
  request->send(200, "text/html; charset=UTF-8", message);
}

void handleWeight2(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    myScale2.calibrate_scale(weight, 20);
    float scale = myScale2.get_scale();
    uint32_t offset = myScale2.get_offset();
    message = "<p>Calibration terminée. Factor 2 : " + String(scale, 6) + ", Offset 2: " + String(offset) + "</p>";

    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor2", scale);
    preferences.end();
    // Restart ESP32
   // esp_restart();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  request->send(200, "text/html; charset=UTF-8", message);
}

void initwifi(){
initPMU(); 

  myScale1.begin(dataPin1, clockPin1);
  myScale2.begin(dataPin2, clockPin2);

  // Créer un point d'accès WiFi au lieu de se connecter à un réseau WiFi
  WiFi.softAP(ssid, password);
  Serial.println("Point d'accès WiFi créé");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/calibrate1", HTTP_GET, handleCalibration1);
  server.on("/calibrate2", HTTP_GET, handleCalibration2);
  server.on("/weight1", HTTP_GET, handleWeight1);
  server.on("/weight2", HTTP_GET, handleWeight2);
  server.begin();
}


