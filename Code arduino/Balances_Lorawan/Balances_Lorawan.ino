//####################### Ratamuse #######################/
//################## ratamuse.pf@googlemail.com ##################/
//######################### MAY 2023 ######################/
//#################### SHELL V3 HELTEC  ####################/
//######################## Version 1.0.0 #######################/

/*
  LoRaWan Rain Gauge
  The tipping bucket rain gauge has a magnetic reed switch that closes momentarily each time the gauge measures 0.011" (0.2794 mm) of rain.
*/

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "CayenneLPP.h"


bool ENABLE_SERIAL = true;  // Enable serial debug output here if required


#define SDA_PIN 38
#define SCL_PIN 37
#define POWER_PIN 18
#define POWER_PIN_STATE HIGH
#define led 36
#define SD_SCK 41
#define SD_MISO 39
#define SD_MOSI 40
#define SD_CS 42
// The interrupt pin is attached to GPIO1
#define RAIN_GAUGE_PIN 17





/* OTAA para*/
uint8_t devEui[] = { 0x4F, 0xFA, 0x12, 0xF4, 0x00, 0x00, 0x5C, 0x83 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 };


/* ABP para*/
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda, 0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef, 0x67 };
uint32_t devAddr = (uint32_t)0x007e6ae1;

/*LoraWan channelsmask, default channels 0-7*/
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };


/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 600000;
unsigned long previousTime = 0;


/*OTAA or ABP*/
bool overTheAirActivation = true;

/*ADR enable*/
bool loraWanAdr = true;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = false;

/* Application port */
uint8_t appPort = 1;

/*!
   Number of trials to transmit the frame, if the LoRaMAC layer did not
   receive an acknowledgment.
*/
uint8_t confirmedNbTrials = 10;

/*********************SDcard*************************/

#include <SPI.h>
#include <Update.h>
#include <FS.h>
#include <SD.h>

#ifdef ALTERNATE_PINS
#define VSPI_MISO MISO
#define VSPI_MOSI MOSI
#define VSPI_SCLK SCK
#define VSPI_SS SS

#define HSPI_MISO 39
#define HSPI_MOSI 40
#define HSPI_SCLK 41
#define HSPI_SS 42
#else
#define VSPI_MISO MISO
#define VSPI_MOSI MOSI
#define VSPI_SCLK SCK
#define VSPI_SS SS

#define HSPI_MISO 39
#define HSPI_MOSI 40
#define HSPI_SCLK 41
#define HSPI_SS 42
#endif

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define VSPI FSPI
#endif

static const int spiClk = 1000000;  // 1 MHz

//uninitalised pointers to SPI objects
SPIClass *vspi = NULL;
SPIClass *hspi = NULL;

//#define SD_SPI_FREQ 4000000



#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"

// Définir le préfixe du SSID et le nom du dispositif
const char* ssidPrefix = "balance";
const char* deviceName = "G3";  // ou "G3" selon le microcontrôleur

// Concaténer le préfixe et le nom du dispositif pour former le SSID
String ssid = String(ssidPrefix) + String(deviceName);

// Déclarer le mot de passe
const char* password1 = "123456789";

AsyncWebServer server(80);
Preferences preferences;


int connectionFailures = 0;

bool setup1Executed = false;


/******************MAX17048******************/

#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>  // Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library
SFE_MAX1704X lipo(MAX1704X_MAX17048);                      // Create a MAX17048
float voltage = 0;                                         // Variable to keep track of LiPo voltage
float soc = 0;                                             // Variable to keep track of LiPo state-of-charge (SOC)
bool alert;                                                // Variable to keep track of whether alert has been triggered
double charge = 0;


/*****************************Wake up reason****************************/

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

/******************************Fonction de Deep Sleep********************************/
void rebootEspWithReason(String reason) {
  Serial.println(reason);
  delay(100);
}

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

void deepsleep() {
    int TimeToSleep = 0;
    int soc = lipo.getSOC();

    if (soc >= 75) {
        TimeToSleep = 1800; // 30 minutes
    } else if (soc >= 51) {
        TimeToSleep = 7200; // 2 heures
    } else if (soc >= 26) {
        TimeToSleep = 21600; // 6 heures
    } else if (soc >= 0) {
        TimeToSleep = 43200; // 12 heures
    } else {
        // Valeur par défaut au cas où la lecture de la batterie échoue
        TimeToSleep = 21600; // 6 heures
    }

    // Convertir le temps de sommeil en microsecondes et vérifier l'absence de débordement
    uint64_t sleepTimeMicroseconds = (uint64_t)TimeToSleep * uS_TO_S_FACTOR;

    // Activez le timer de réveil et entrez en deep sleep
    esp_sleep_enable_timer_wakeup(sleepTimeMicroseconds);
    esp_deep_sleep_start();
}


/********************Sht4x*********************************/

#include "PWFusion_TCA9548A.h"
#include "SensirionI2cSht4x.h"
TCA9548A i2cMux;
SensirionI2cSht4x sht4x;
uint16_t error0;
uint16_t error1;
uint16_t error2;
uint16_t error3;
uint16_t error4;
float temperature0;
float humidity0;
float temperature1;
float humidity1;
float temperature2;
float humidity2;
float temperature3;
float humidity3;
float temperature4;
float humidity4;
char errorMessage[256];







/**********************HX711*******************************/

#include "HX711.h"

HX711 scale1;
HX711 scale2;
HX711 scale3;
HX711 scale4;
uint8_t dataPin1 = 48;
uint8_t clockPin1 = 47;
uint8_t dataPin2 = 14;
uint8_t clockPin2 = 13;
uint8_t dataPin3 = 48;
uint8_t clockPin3 = 47;
uint8_t dataPin4 = 14;
uint8_t clockPin4 = 13;

/********************serveur web***************************/

void sendCommonHTML(AsyncWebServerRequest *request, String content) {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #FFF176; /* Couleur jaune miel */ }";
  html += "h1 { color: #8D6E63; /* Marron foncé */ }"; // Couleur marron foncé pour les titres
  html += "input[type=submit] { background-color: #FFD54F; /* Jaune miel clair */ color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin: 10px; }";
  html += "input[type=checkbox] { margin-bottom: 20px; }"; // Espacement vertical pour les cases à cocher
  html += "</style></head><body>";
  html += "<h1>Calibration des balances</h1>";
  html += "<p>Choisir la balance à calibrer</p>";
  html += "<p>Enlever tous poids sur les balances</p>";
  html += content;
  html += "</body></html>";
  request->send(200, "text/html; charset=UTF-8", html);
}

void handleRoot(AsyncWebServerRequest *request) {
  String content = "<div style=\"display: inline-block;\">"; // Div pour aligner les boutons horizontalement
  content += "<form action=\"/calibrate1\" method=\"get\">";
  content += "<input type=\"submit\" value=\"Calibration balance 1\">";
  content += "</form>";
  content += "<form action=\"/calibrate2\" method=\"get\">";
  content += "<input type=\"submit\" value=\"Calibration balance 2\">";
  content += "</form>";
  content += "<form action=\"/calibrate3\" method=\"get\">";
  content += "<input type=\"submit\" value=\"Calibration balance 3\">";
  content += "</form>";
  content += "<form action=\"/calibrate4\" method=\"get\">";
  content += "<input type=\"submit\" value=\"Calibration balance 4\">";
  content += "</form>";
  // Ajout du bouton "Redémarrer l'ESP32"
  content += "<form action=\"/reboot\" method=\"post\">";
  content += "<button type=\"submit\">Redémarrer l'ESP32</button>";
  content += "</form>";
  content += "</div>";
  sendCommonHTML(request, content);
}

void handleCalibration1(AsyncWebServerRequest *request) {
  scale1.tare(20);  // average 20 measurements.
  uint32_t offset1 = scale1.get_offset();
  String content = "<h1>Calibration balance 1</h1>";
  content += "<p>Offset: " + String(offset1) + "</p>";
  content += "<p>Placer un poids sur la balance (en gramme)</p>";
  content += "<form action=\"/weight1\" method=\"get\">";
  content += "<input type=\"text\" name=\"weight\">";
  content += "<input type=\"submit\" value=\"Enter\">";
  content += "</form>";
  sendCommonHTML(request, content);
}

void handleCalibration2(AsyncWebServerRequest *request) {
  scale2.tare(20);  // average 20 measurements.
  uint32_t offset2 = scale2.get_offset();
  String content = "<h1>Calibration balance 2</h1>";
  content += "<p>Offset: " + String(offset2) + "</p>";
  content += "<p>Placer un poids sur la balance (en gramme)</p>";
  content += "<form action=\"/weight2\" method=\"get\">";
  content += "<input type=\"text\" name=\"weight\">";
  content += "<input type=\"submit\" value=\"Enter\">";
  content += "</form>";
  sendCommonHTML(request, content);
}

void handleCalibration3(AsyncWebServerRequest *request) {
  scale3.tare(20);  // average 20 measurements.
  uint32_t offset3 = scale3.get_offset();
  String content = "<h1>Calibration balance 3</h1>";
  content += "<p>Offset: " + String(offset3) + "</p>";
  content += "<p>Placer un poids sur la balance (en gramme)</p>";
  content += "<form action=\"/weight3\" method=\"get\">";
  content += "<input type=\"text\" name=\"weight\">";
  content += "<input type=\"submit\" value=\"Enter\">";
  content += "</form>";
  sendCommonHTML(request, content);
}

void handleCalibration4(AsyncWebServerRequest *request) {
  scale4.tare(20);  // average 20 measurements.
  uint32_t offset4 = scale4.get_offset();
  String content = "<h1>Calibration balance 4</h1>";
  content += "<p>Offset: " + String(offset4) + "</p>";
  content += "<p>Placer un poids sur la balance (en gramme)</p>";
  content += "<form action=\"/weight4\" method=\"get\">";
  content += "<input type=\"text\" name=\"weight\">";
  content += "<input type=\"submit\" value=\"Enter\">";
  content += "</form>";
  sendCommonHTML(request, content);
}

void handleWeight1(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    scale1.calibrate_scale(weight, 20);
    float scalea = scale1.get_scale();
    uint32_t offset1 = scale1.get_offset();
    message = "<p>Calibration terminée. Factor 1: " + String(scalea, 6) + ", Offset 1: " + String(offset1) + "</p>";

    // Button to trigger ESP32 reboot
    message += "<form action=\"/reboot\" method=\"post\">";
    message += "<button type=\"submit\">Redémarrer l'ESP32</button>";
    message += "</form>";
    
    delay(1000);

    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor1", scalea);
    preferences.putFloat("offset1", offset1);
    preferences.end();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  // Add a button to return to the home page
  message += "<form action=\"/\" method=\"get\">";
  message += "<input type=\"submit\" value=\"Retour à l'accueil\">";
  message += "</form>";
  sendCommonHTML(request, message);
}

void handleWeight2(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    scale2.calibrate_scale(weight, 20);
    float scaleb = scale2.get_scale();
    uint32_t offset2 = scale2.get_offset();
    message = "<p>Calibration terminée. Factor 2: " + String(scaleb, 6) + ", Offset 2: " + String(offset2) + "</p>";
    
    // Button to trigger ESP32 reboot
    message += "<form action=\"/reboot\" method=\"post\">";
    message += "<button type=\"submit\">Redémarrer l'ESP32</button>";
    message += "</form>";
    
    delay(1000);
    
    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor2", scaleb);
    preferences.putFloat("offset2", offset2);
    preferences.end();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  // Add a button to return to the home page
  message += "<form action=\"/\" method=\"get\">";
  message += "<input type=\"submit\" value=\"Retour à l'accueil\">";
  message += "</form>";
  sendCommonHTML(request, message);
}

void handleWeight3(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    scale3.calibrate_scale(weight, 20);
    float scalec = scale3.get_scale();
    uint32_t offset3 = scale3.get_offset();
    message = "<p>Calibration terminée. Factor 3: " + String(scalec, 6) + ", Offset 3: " + String(offset3) + "</p>";

    // Button to trigger ESP32 reboot
    message += "<form action=\"/reboot\" method=\"post\">";
    message += "<button type=\"submit\">Redémarrer l'ESP32</button>";
    message += "</form>";
    
    delay(1000);

    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor3", scalec);
    preferences.putFloat("offset3", offset3);
    preferences.end();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  // Add a button to return to the home page
  message += "<form action=\"/\" method=\"get\">";
  message += "<input type=\"submit\" value=\"Retour à l'accueil\">";
  message += "</form>";
  sendCommonHTML(request, message);
}

void handleWeight4(AsyncWebServerRequest *request) {
  String message;
  if (request->hasArg("weight")) {
    uint32_t weight = request->arg("weight").toInt();
    scale4.calibrate_scale(weight, 20);
    float scaled = scale4.get_scale();
    uint32_t offset4 = scale4.get_offset();
    message = "<p>Calibration terminée. Factor 4: " + String(scaled, 6) + ", Offset 4: " + String(offset4) + "</p>";
    
    // Button to trigger ESP32 reboot
    message += "<form action=\"/reboot\" method=\"post\">";
    message += "<button type=\"submit\">Redémarrer l'ESP32</button>";
    message += "</form>";
    
    delay(1000);
    
    // Save scale value to ESP32's EEPROM
    preferences.begin("my-app", false);
    preferences.putFloat("factor4", scaled);
    preferences.putFloat("offset4", offset4);
    preferences.end();
  } else {
    message = "<p>Manque paramètres de poids</p>";
  }
  // Add a button to return to the home page
  message += "<form action=\"/\" method=\"get\">";
  message += "<input type=\"submit\" value=\"Retour à l'accueil\">";
  message += "</form>";
  sendCommonHTML(request, message);
}

void handleReboot(AsyncWebServerRequest *request) {
  esp_restart();
}



/*********************MS5611*******************************/

#include <Wire.h>
#include "MS5611.h"
MS5611 MS5611(0x77);
uint32_t start, stop, count;



// perform the actual update from a given stream

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    } else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
      } else {
        Serial.println("Update not finished? Something went wrong!");
      }
    } else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }

  } else {
    Serial.println("Not enough space to begin OTA");
  }
}

// check given FS for valid update.bin and perform update if available
void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/update.bin");
  if (updateBin) {
    if (updateBin.isDirectory()) {
      Serial.println("Error, update.bin is not a file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      Serial.println("Try to start update");
      performUpdate(updateBin, updateSize);
    } else {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    // whe finished remove the binary from sd card to indicate end of the process
    fs.remove("/update.bin");
  } else {
    Serial.println("Could not load update.bin from sd root");
  }
}

/*****************************Void HX711*************************************************/

void hx711ABCD(){

    
  // Récupérer la valeur de "factor1"
  float factor1 = preferences.getFloat("factor1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
  
  // Récupérer la valeur de "factor2"
  float factor2 = preferences.getFloat("factor2", 0.0); // 0.0 est la valeur par défaut si "factor2" n'est pas trouvé

  // Récupérer la valeur de "factor3"
  float factor3 = preferences.getFloat("factor3", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
  
  // Récupérer la valeur de "factor4"
  float factor4 = preferences.getFloat("factor4", 0.0); // 0.0 est la valeur par défaut si "factor2" n'est pas trouvé

 // Récupérer la valeur de "offset1"
float offset1 = preferences.getFloat("offset1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé

// Récupérer la valeur de "offset2"
float offset2 = preferences.getFloat("offset2", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé

// Récupérer la valeur de "offset3"
float offset3 = preferences.getFloat("offset3", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé

// Récupérer la valeur de "offset4"
float offset4 = preferences.getFloat("offset4", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé    
  // Terminer l'utilisation des préférences
  preferences.end();

int scaleA = 0;
  if (scale1.is_ready())
    delay(200);
  {
    scaleA = static_cast<int>(((scale1.read_average(10)-offset1)/factor1));
    
   Serial.print("ScaleA:");
    Serial.println(scaleA);
  }

  delay(200);


int scaleB = 0;

  if (scale2.is_ready());
    delay(200);
  {
   scaleB = static_cast<int>(((scale2.read_average(10)-offset2)/factor2));
    Serial.print("ScaleB:");
    Serial.println(scaleB);

    //Serial.println(scale2.get_units(1));
  }
  delay(200);

int scaleC = 0;
  if (scale3.is_ready())
    delay(200);
  {
    scaleC = static_cast<int>(((scale3.read_average(10)-offset3)/factor3));
    
   Serial.print("ScaleC:");
    Serial.println(scaleC);
  }

  delay(200);

int scaleD = 0;
  if (scale4.is_ready())
    delay(200);
  {
    scaleD = static_cast<int>(((scale4.read_average(10)-offset4)/factor4));
    
   Serial.print("ScaleD:");
    Serial.println(scaleD);
  }

  delay(200);
}

/***************************** Prepares the payload of the frame ************************/

static void prepareTxFrame1(uint8_t port) {


  Serial.print("on commence ici de preparer les frames");


 // Récupérer la valeur de "factor1"
  float factor1 = preferences.getFloat("factor1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
// Récupérer la valeur de "offset1"
float offset1 = preferences.getFloat("offset1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
int scaleA = 0;
  if (scale1.is_ready())
    delay(200);
  {
    scaleA = static_cast<int>(((scale1.read_average(10)-offset1)/factor1));
    
   Serial.print("ScaleA:");
    Serial.println(scaleA);
  }

  delay(200);


  CayenneLPP lpp(51); //déterminer le max payload
  lpp.reset();
  
  lpp.addAnalogInput(17, scaleA);
  
  appDataSize = lpp.getSize();
  memcpy(appData, lpp.getBuffer(), lpp.getSize());

    
    }
  static void prepareTxFrame2(uint8_t port) {

  Serial.print("on commence ici de preparer les frames");
  
 // Récupérer la valeur de "factor2"
  float factor2 = preferences.getFloat("factor2", 0.0); // 0.0 est la valeur par défaut si "factor2" n'est pas trouvé
// Récupérer la valeur de "offset2"
float offset2 = preferences.getFloat("offset2", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
int scaleB = 0;

  if (scale2.is_ready());
    delay(200);
  {
   scaleB = static_cast<int>(((scale2.read_average(10)-offset2)/factor2));
    Serial.print("ScaleB:");
    Serial.println(scaleB);

    //Serial.println(scale2.get_units(1));
  }
  delay(200);

  CayenneLPP lpp(51); //déterminer le max payload
  lpp.reset();
  lpp.addAnalogInput(18, scaleB);
 
  appDataSize = lpp.getSize();
  memcpy(appData, lpp.getBuffer(), lpp.getSize());

    
    }

int gpioPin = 11;

#define MONITOR_INTERVAL_SECONDS 300

// Définir une variable globale pour stocker le dernier temps où le premier cœur a été vu en cours d'exécution
unsigned long lastSeenRunning = 0;

// Déclaration de la fonction pour la tâche de surveillance du premier cœur
void monitorFirstCoreTask(void * parameter);
// Fonction pour la tâche de surveillance du premier cœur
void monitorFirstCoreTask(void * parameter) {
  // Boucle infinie pour la surveillance
  while (true) {
    // Vérifier si le premier cœur a été vu en cours d'exécution récemment
    if (millis() - lastSeenRunning > MONITOR_INTERVAL_SECONDS * 1000) {
      // Si le premier cœur est bloqué depuis plus de MONITOR_INTERVAL_SECONDS secondes, redémarrer l'ESP32-S3
      Serial.println("Le premier cœur est bloqué ! Redémarrage en cours...");
      esp_restart();  // Redémarrer l'ESP32-S3
    }
    // Attendre pendant un certain temps avant de vérifier à nouveau (par exemple, toutes les 10 secondes)
    vTaskDelay(10000 / portTICK_PERIOD_MS);  // Attendre 10 secondes
  }
}

    void setup1(){
      Serial.begin(115200);
       //Start while waiting for Serial monitoring
  while (!Serial);
        // Si la broche est basse (LOW), exécutez la fonction "calibration"
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, POWER_PIN_STATE);
  delay(50);

pinMode(led, OUTPUT);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
        scale1.begin(dataPin1, clockPin1);
        scale2.begin(dataPin2, clockPin2);
        scale3.begin(dataPin3, clockPin3);
        scale4.begin(dataPin4, clockPin4);
  // Créer un point d'accès WiFi au lieu de se connecter à un réseau WiFi
 WiFi.softAP(ssid.c_str(), password1);
  Serial.println("Point d'accès WiFi créé");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/calibrate1", HTTP_GET, handleCalibration1);
  server.on("/calibrate2", HTTP_GET, handleCalibration2);
  server.on("/calibrate3", HTTP_GET, handleCalibration3);
  server.on("/calibrate4", HTTP_GET, handleCalibration4);
  server.on("/weight1", HTTP_GET, handleWeight1);
  server.on("/weight2", HTTP_GET, handleWeight2);
  server.on("/weight3", HTTP_GET, handleWeight3);
  server.on("/weight4", HTTP_GET, handleWeight4);
  server.on("/reboot", HTTP_POST, handleReboot);
server.begin();
  setup1Executed = true;
  
        
    }



void setup() {

  pinMode(gpioPin, INPUT_PULLUP);

    // Lecture de l'état de la broche GPIO 2
    

    if (digitalRead(gpioPin) == LOW) {
setup1();
}
else{
         

  uint8_t cardType;
  if (ENABLE_SERIAL) {
    Serial.begin(115200);
  }

  

  previousTime = millis();


  delay(10);
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, POWER_PIN_STATE);
  
delay(50);


  Wire.begin(SDA_PIN, SCL_PIN);


 Serial.println();
xTaskCreatePinnedToCore(
    monitorFirstCoreTask,       // Fonction de la tâche
    "FirstCoreMonitorTask",     // Nom de la tâche
    4096,                       // Taille de la pile
    NULL,                       // Paramètre de la tâche
    1,                          // Priorité de la tâche
    NULL,                       // Handle de la tâche (inutilisé)
    1                           // Core sur lequel la tâche doit être exécutée (core 1)
  );

pinMode(led, OUTPUT);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);
delay(50);
digitalWrite(led, HIGH);
delay(50);
digitalWrite(led, LOW);


/*******************HX711***********************************/

// Initialiser les préférences
  preferences.begin("my-app", false);
 // Récupérer les valeurs des factors
float factor1 = preferences.getFloat("factor1", 0.0); // 0.0 est la valeur par défaut si "factor1" n'est pas trouvé
float factor2 = preferences.getFloat("factor2", 0.0); // 0.0 est la valeur par défaut si "factor2" n'est pas trouvé
float factor3 = preferences.getFloat("factor3", 0.0); // 0.0 est la valeur par défaut si "factor3" n'est pas trouvé
float factor4 = preferences.getFloat("factor4", 0.0); // 0.0 est la valeur par défaut si "factor4" n'est pas trouvé

// Récupérer les valeurs des offsets
float offset1 = preferences.getFloat("offset1", 0.0); // 0.0 est la valeur par défaut si "offset1" n'est pas trouvé
float offset2 = preferences.getFloat("offset2", 0.0); // 0.0 est la valeur par défaut si "offset2" n'est pas trouvé
float offset3 = preferences.getFloat("offset3", 0.0); // 0.0 est la valeur par défaut si "offset3" n'est pas trouvé
float offset4 = preferences.getFloat("offset4", 0.0); // 0.0 est la valeur par défaut si "offset4" n'est pas trouvé

    
  // Terminer l'utilisation des préférences
  preferences.end();
    
  // Utiliser factor1, factor2, factor3 et factor4 dans votre code
Serial.println("Factor 1: " + String(factor1, 6));
Serial.println("Factor 2: " + String(factor2, 6));
Serial.println("Factor 3: " + String(factor3, 6));
Serial.println("Factor 4: " + String(factor4, 6));

// Utiliser offset1, offset2, offset3 et offset4 dans votre code
Serial.println("Offset 1: " + String(offset1, 6));
Serial.println("Offset 2: " + String(offset2, 6));
Serial.println("Offset 3: " + String(offset3, 6));
Serial.println("Offset 4: " + String(offset4, 6));

 // Initialiser les échelles et configurer les facteurs
  scale1.begin(dataPin1, clockPin1);
  scale1.set_scale(factor1);

  scale2.begin(dataPin2, clockPin2);
  scale2.set_scale(factor2);

  scale3.begin(dataPin3, clockPin3);
  scale3.set_scale(factor3);

  scale4.begin(dataPin4, clockPin4);
  scale4.set_scale(factor4);

  /*********************MS5611*******************************/
  // Initialize MS5611
  Serial.println();
  Serial.println(__FILE__);
  Serial.print("MS5611_LIB_VERSION: ");
  Serial.println(MS5611_LIB_VERSION);



  if (MS5611.begin() == true) {
    Serial.println("MS5611 found.");
  } else {
    Serial.println("MS5611 not found. halt.");
    while (1)
      ;
  }

  
  /*****************SD update*********************************/
  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  vspi = new SPIClass(VSPI);
  hspi = new SPIClass(HSPI);

  //clock miso mosi ss

#ifndef ALTERNATE_PINS
  //initialise vspi with default pins
  //SCLK = 18, MISO = 19, MOSI = 23, SS = 5
  vspi->begin();
#else
  //alternatively route through GPIO pins of your choice
  vspi->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS);  //SCLK, MISO, MOSI, SS
#endif

#ifndef ALTERNATE_PINS
  //initialise hspi with default pins
  //SCLK = 14, MISO = 12, MOSI = 13, SS = 15
  hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_SS);
#else
  //alternatively route through GPIO pins
  hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_SS);  //SCLK, MISO, MOSI, SS
#endif

  //set up slave select pins as outputs as the Arduino API
  //doesn't handle automatically pulling SS low
  pinMode(vspi->pinSS(), OUTPUT);  //VSPI SS
  pinMode(hspi->pinSS(), OUTPUT);  //HSPI SS




  Serial.println("Welcome to the SD-Update example!");

  // You can uncomment this and build again
  // Serial.println("Update successfull");

  //first init and check SD card
  if (!SD.begin(42, *hspi, spiClk)) {
    rebootEspWithReason("Card Mount Failed");
  }

  cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    rebootEspWithReason("No SD_MMC card attached");
  } else {
    updateFromFS(SD);
  }



  /*********************Sht4x*******************************/

  // Initialize I2C multiplexor
  i2cMux.begin();
  sht4x.begin(Wire, SHT40_I2C_ADDR_44);

  /*****************MAX17048******************************************/

  lipo.enableDebugging();  // Uncomment this line to enable helpful debug messages on Serial

  // Set up the MAX17043 LiPo fuel gauge:
  if (lipo.begin() == false)  // Connect to the MAX17043 using the default wire port
  {
    Serial.println(F("MAX17048 not detected. Please check wiring. Freezing."));
    while (1)
      ;
  }
  // Quick start restarts the MAX17043 in hopes of getting a more accurate
  // guess for the SOC.
  lipo.quickStart();

  // We can set an interrupt to alert when the battery SoC gets too low.
  // We can alert at anywhere between 1% - 32%:
  lipo.setThreshold(20);  // Set alert threshold to 20%.
  
  deviceState = DEVICE_STATE_INIT;
}

}


void loop() {


 
    switch (deviceState) {
      case DEVICE_STATE_INIT:
        {
#if (LORAWAN_DEVEUI_AUTO)
          LoRaWAN.generateDeveuiByChipID();
#endif
          LoRaWAN.init(loraWanClass, loraWanRegion);
          break;
        }
      case DEVICE_STATE_JOIN:
        {
          LoRaWAN.join();
          break;
        }
      case DEVICE_STATE_SEND:
        {
           hx711ABCD();
          prepareTxFrame1(appPort);
          prepareTxFrame2(appPort);
          LoRaWAN.send();

          deviceState = DEVICE_STATE_CYCLE;
          break;
        }
      case DEVICE_STATE_CYCLE:
        {
          // Schedule next packet transmission
          txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
          LoRaWAN.cycle(txDutyCycleTime);
          deviceState = DEVICE_STATE_SLEEP;
          break;
        }
      case DEVICE_STATE_SLEEP:
        {         
          LoRaWAN.sleep(loraWanClass);
          break;
        }
      default:
        {
          deviceState = DEVICE_STATE_INIT;
          break;
        }
    }
  }


/*****************************Calcul de la pression barométrique ramené au niveau de la mer****************************/

void baropressure() {

 delay(1000);

  start = micros();
  int result = MS5611.read();   // uses default OSR_ULTRA_LOW  (fastest)
  stop = micros();

  if (count % 20 == 0)
  {
    Serial.println();
    Serial.println("CNT\tDUR\tRES\tTEMP\tPRES");
  }

  Serial.print(count);
  count++;
  Serial.print("\t");
  Serial.print(stop - start);
  Serial.print("\t");
  Serial.print(result);
  Serial.print("\t");
  Serial.print(MS5611.getTemperature(), 2);
  Serial.print("\t");
  Serial.print(MS5611.getPressure(), 2);
  Serial.println();
}

/*****************************Valeur de la batterie en volt et %****************************/

void battery() {
  voltage = lipo.getVoltage();
  soc = lipo.getSOC();
}

/*****************************Valeur des 2 capteurs SHT41****************************/

void sht() {


 // i2cMux.setChannel(CHAN4);
  
  //error4 = sht4x.measureHighPrecision(temperature4, humidity4);

  i2cMux.setChannel(CHAN0);
  float temperature0;
  float humidity0;
  error0 = sht4x.measureHighPrecision(temperature0, humidity0);
}







