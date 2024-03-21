/**
 * @file      ModemMqttPulishExample.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"

XPowersPMU PMU;

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include "utilities.h"
#define MAX_CONNECT_ATTEMPTS 1

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(Serial1, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

const char *register_info[] = {
  "Not registered, MT is not currently searching an operator to register to.The GPRS service is disabled, the UE is allowed to attach for GPRS if requested by the user.",
  "Registered, home network.",
  "Not registered, but MT is currently trying to attach or searching an operator to register to. The GPRS service is enabled, but an allowable PLMN is currently not available. The UE will start a GPRS attach as soon as an allowable PLMN is available.",
  "Registration denied, The GPRS service is disabled, the UE is not allowed to attach for GPRS if it is requested by the user.",
  "Unknown.",
  "Registered, roaming.",
};

enum {
  MODEM_CATM = 1,
  MODEM_NB_IOT,
  MODEM_CATM_NBIOT,
};

#define randMax 35
#define randMin 18
int bat;

// Your GPRS credentials, if any
const char apn[] = "";
const char gprsUser[] = "";
const char gprsPass[] = "";

// cayenne server address and port
const char server[] = "ratamuse.hopto.org";
const int port = 1885;
char buffer[1024] = { 0 };

// To create a device : https://cayenne.mydevices.com/cayenne/dashboard
//  1. Add new...
//  2. Device/Widget
//  3. Bring Your Own Thing
//  4. Copy the <MQTT USERNAME> <MQTT PASSWORD> <CLIENT ID> field to the bottom for replacement
char username[] = "<>";
char password[] = "<>";
char clientID[] = "<>";

// To create a widget
//  1. Add new...
//  2. Device/Widget
//  3. Custom Widgets
//  4. Value
//  5. Fill in the name and select the newly created equipment
//  6. Channel is filled as 0
//  7.  Choose ICON
//  8. Add Widget
int data_channel = 0;

/*HX711*/
#include "HX711.h"
int scaleA;
int scaleB;
HX711 scale1;
HX711 scale2;
uint8_t dataPin1 = 35;
uint8_t clockPin1 = 36;
uint8_t dataPin2 = 13;
uint8_t clockPin2 = 14;


bool isConnect() {
  modem.sendAT("+SMSTATE?");
  if (modem.waitResponse("+SMSTATE: ")) {
    String res = modem.stream.readStringUntil('\r');
    return res.toInt();
  }
  return false;
}

int connectionFailures = 0;

void setup() {

  Serial.begin(115200);

  //Start while waiting for Serial monitoring
  while (!Serial)
    ;

  // delay(3000);

  Serial.println();

  /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
    ***********************************/
  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("Failed to initialize power.....");
    while (1) {
      delay(4000);
    }
  }
  //Set the working voltage of the modem, please do not modify the parameters
  PMU.setDC3Voltage(3000);  //SIM7080 Modem main power channel 2700~ 3400V
  PMU.enableDC3();

  PMU.setDC5Voltage(3300);  //SIM7080 Modem main power channel 2700~ 3400V
  PMU.enableDC5();
/*
  //Modem GPS Power channel
  PMU.setBLDO2Voltage(3300);
  PMU.enableBLDO2();  //The antenna power must be turned on to use the GPS function
*/
  PMU.enableBattVoltageMeasure();
  PMU.enableSystemVoltageMeasure();
  // TS Pin detection must be disable, otherwise it cannot be charged
  PMU.disableTSPinMeasure();

  /*********************************
     * step 2 : start modem
    ***********************************/

  Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

  pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
  pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
  pinMode(BOARD_MODEM_RI_PIN, INPUT);

  int retry = 0;
  while (!modem.testAT(1000)) {
    Serial.print(".");
    if (retry++ > 6) {
      // Pull down PWRKEY for more than 1 second according to manual requirements
      digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
      delay(100);
      digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
      delay(1000);
      digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
      retry = 0;
      Serial.println("Retry start modem .");
    }
  }
  Serial.println();
  Serial.print("Modem started!");

  /*********************************
     * step 3 : Check if the SIM card is inserted
    ***********************************/
  String result;


  if (modem.getSimStatus() != SIM_READY) {
    Serial.println("SIM Card is not insert!!!");
    return;
  }


  /*********************************
     * step 4 : Set the network mode to NB-IOT
    ***********************************/

  modem.setNetworkMode(2);  //use automatic

  modem.setPreferredMode(MODEM_NB_IOT);

  uint8_t pre = modem.getPreferredMode();

  uint8_t mode = modem.getNetworkMode();

  Serial.printf("getNetworkMode:%u getPreferredMode:%u\n", mode, pre);


  /*********************************
    * step 5 : Wait for the network registration to succeed
    ***********************************/
  RegStatus s;
  do {
    s = modem.getRegistrationStatus();
    if (s != REG_OK_HOME && s != REG_OK_ROAMING) {
      Serial.print(".");
      delay(1000);
    }

  } while (s != REG_OK_HOME && s != REG_OK_ROAMING);

  Serial.println();
  Serial.print("Network register info:");
  Serial.println(register_info[s]);

  // Activate network bearer, APN can not be configured by default,
  // if the SIM card is locked, please configure the correct APN and user password, use the gprsConnect() method

  bool res = modem.isGprsConnected();
  if (!res) {
    modem.sendAT("+CNACT=0,1");
    if (modem.waitResponse() != 1) {
      Serial.println("Activate network bearer Failed!");
      return;
    }
    // if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    //     return ;
    // }
  }
  modem.sendAT("+CGNSPWR=<0>");
  Serial.print("GPRS status:");
  Serial.println(res ? "connected" : "not connected");

  String ccid = modem.getSimCCID();
  Serial.print("CCID:");
  Serial.println(ccid);

  String imei = modem.getIMEI();
  Serial.print("IMEI:");
  Serial.println(imei);

  String imsi = modem.getIMSI();
  Serial.print("IMSI:");
  Serial.println(imsi);

  String cop = modem.getOperator();
  Serial.print("Operator:");
  Serial.println(cop);

  IPAddress local = modem.localIP();
  Serial.print("Local IP:");
  Serial.println(local);

  int csq = modem.getSignalQuality();
  Serial.print("Signal quality:");
  Serial.println(csq);




  /*********************************
    * step 6 : setup MQTT Client
    ***********************************/

  // If it is already connected, disconnect it first
  modem.sendAT("+SMDISC");
  modem.waitResponse();


  snprintf(buffer, 1024, "+SMCONF=\"URL\",\"%s\",%d", server, port);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }
  snprintf(buffer, 1024, "+SMCONF=\"USERNAME\",\"%s\"", username);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }

  snprintf(buffer, 1024, "+SMCONF=\"PASSWORD\",\"%s\"", password);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }

  snprintf(buffer, 1024, "+SMCONF=\"CLIENTID\",\"%s\"", clientID);
  modem.sendAT(buffer);
  if (modem.waitResponse() != 1) {
    return;
  }

  int8_t ret;
  int connectAttempts = 0;

  do {
    modem.sendAT("+SMCONN");
    ret = modem.waitResponse(10000);
    if (ret != 1) {
      Serial.println("La connexion a échoué, nouvelle tentative de connexion...");
      delay(1000);
      connectAttempts++;
    }

    // Si le nombre maximum de tentatives de connexion est atteint, redémarrer l'ESP32
    if (connectAttempts >= MAX_CONNECT_ATTEMPTS) {
      Serial.println("La connexion a échoué après plusieurs tentatives, redémarrage de l'ESP32...");
      modem.sendAT("+CPOWD=<1>");
      Serial.println("Modem stoppé");
      //delay(1000);
      //PMU.disableBLDO2();
      PMU.disableDC3();
      PMU.disableDC5();
      ESP.restart();
    }

  } while (ret != 1);

  Serial.println("Le client MQTT est connecté!");

  // Données de graine aléatoire
  randomSeed(esp_random());
/*
  scale1.begin(dataPin1, clockPin1);
  scale1.set_scale(1009.484497);
*/
  scale2.begin(dataPin2, clockPin2);
  scale2.set_scale(1.601872);
  
}

void loop() {
  if (!isConnect()) {
    Serial.println("MQTT Client disconnect!");
    delay(100);
  }
  Serial.print("getBatteryPercent:");
  Serial.print(PMU.getBatteryPercent());
  Serial.println("%");
/*
  if (scale1.is_ready())
    delay(200);
  {
    scaleA = (scale1.get_units(1) + 15.7);
    Serial.println(scaleA);
  }

  delay(200);
*/
  if (scale2.is_ready())
    delay(200);
  {
    scaleB = (scale2.get_units(1) + 0);
    Serial.println(scaleB);
  }

  delay(200);
/*

  Serial.println();
  // Publish fake temperature data
  String payload1 = "";
  int temp1 = rand() % (randMax - randMin) + randMin;
  payload1.concat(temp1);
  payload1.concat("\r\n");
  
  String payload2 = "";
  payload2.concat(scaleA);
  payload2.concat("\r\n");
*/

String payload3 = "";
  payload3.concat(scaleB);
  payload3.concat("\r\n");


  bat = PMU.getBatteryPercent();
  String payload4 = "";
  payload4.concat(bat);
  payload4.concat("\r\n");

/*
  // AT+SMPUB=<topic>,<content length>,<qos>,<retain><CR>message is enteredQuit edit mode if messagelength equals to <contentlength>
  snprintf(buffer, 1024, "+SMPUB=\"v1/%s/Temp/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload1.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload1.c_str(), payload1.length());
    Serial.print("Try publish payload1: ");
    Serial.println(payload1);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet1 success!");
    } else {
      Serial.println("Send Packet1 failed!");
    }
  }
  delay(100);
  
  snprintf(buffer, 1024, "+SMPUB=\"v1/%s/scale1/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload2.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload2.c_str(), payload2.length());
    Serial.print("Try publish payload2: ");
    Serial.println(payload2);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet2 success!");
    } else {
      Serial.println("Send Packet2 failed!");
    }
  }
delay(100);
*/

  
  snprintf(buffer, 1024, "+SMPUB=\"v1/%s/scale2/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload3.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload3.c_str(), payload3.length());
    Serial.print("Try publish payload2: ");
    Serial.println(payload3);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet3 success!");
    } else {
      Serial.println("Send Packet3 failed!");
    }
  }
delay(100);



  snprintf(buffer, 1024, "+SMPUB=\"v1/%s/bat1/%s/data/%d\",%d,1,1", username, clientID, data_channel, payload4.length());
  modem.sendAT(buffer);
  if (modem.waitResponse(">") == 1) {
    modem.stream.write(payload4.c_str(), payload4.length());
    Serial.print("Try publish payload4: ");
    Serial.println(payload4);

    if (modem.waitResponse(3000)) {
      Serial.println("Send Packet4 success!");
    } else {
      Serial.println("Send Packet4 failed!");
    }
  }
  modem.sendAT("+SMDISC");

  // Entrer en mode de veille profonde pendant 30 secondes
  Serial.println("Entering deep sleep for 30 minutes...");
  esp_sleep_enable_timer_wakeup(1800 * 1000000);  // 30 secondes en microsecondes
  modem.sendAT("+CPOWD=<1>");
  Serial.println("Modem stoppé");
  delay(100);
  //PMU.disableBLDO2();
  PMU.disableDC3();
   PMU.disableDC5();
  esp_deep_sleep_start();
}