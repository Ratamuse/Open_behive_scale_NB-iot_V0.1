#include "startmqtt.h"

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

 bool isConnect() {
  modem.sendAT("+SMSTATE?");
  if (modem.waitResponse("+SMSTATE: ")) {
    String res = modem.stream.readStringUntil('\r');
    return res.toInt();
  }
  return false;
}


void startmqtt1(){
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
      ESP.restart();
    }

  } while (ret != 1);

  Serial.println("Le client MQTT est connecté!");
}
