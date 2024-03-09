#include "debug_config.h"
#include "startmodem.h"
#include "ttgopower.h"
// Your GPRS credentials, if any
const char apn[] = "";
const char gprsUser[] = "";
const char gprsPass[] = "";




void startmodem1(){  
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
  }