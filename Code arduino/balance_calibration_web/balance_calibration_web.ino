#include "HX711.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"

HX711 myScale;

uint8_t dataPin = 6;
uint8_t clockPin = 7;

const char* ssid = "balance";
const char* password = "123456789";

AsyncWebServer server(80);
Preferences preferences;

void setup() {
  Serial.begin(115200);
  myScale.begin(dataPin, clockPin);

  // Créer un point d'accès WiFi au lieu de se connecter à un réseau WiFi
  WiFi.softAP(ssid, password);
  Serial.println("Point d'accès WiFi créé");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/calibrate", HTTP_GET, handleCalibration);
  server.on("/weight", HTTP_GET, handleWeight);
  server.begin();
}

void loop() {
  // Pas besoin de gérer le client dans ESPAsyncWebServer
}

void handleRoot(AsyncWebServerRequest *request) {
  String html = "<h1>Calibration</h1>";
  html += "<p>Remove all weight from loadcell</p>";
  html += "<form action=\"/calibrate\" method=\"get\">";
  html += "<input type=\"submit\" value=\"Enter\">";
  html += "</form>";
  request->send(200, "text/html", html);
}

void handleCalibration(AsyncWebServerRequest *request) {
  myScale.tare(20);  // average 20 measurements.
  uint32_t offset = myScale.get_offset();
  String html = "<h1>Calibration</h1>";
  html += "<p>Offset: " + String(offset) + "</p>";
  html += "<p>Place a weight on the loadcell</p>";
  html += "<form action=\"/weight\" method=\"get\">";
  html += "<input type=\"text\" name=\"weight\">";
  html += "<input type=\"submit\" value=\"Enter\">";
  html += "</form>";
  request->send(200, "text/html", html);
}

void handleWeight(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    myScale.calibrate_scale(weight, 20);
    float scale = myScale.get_scale();
    uint32_t offset = myScale.get_offset();
    message = "<p>Calibration successful. Scale: " + String(scale, 6) + ", Offset: " + String(offset) + "</p>";

    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("scale", scale);
    preferences.end();
  } else {
    message = "<p>Missing weight parameter</p>";
  }
  request->send(200, "text/html", message);
}