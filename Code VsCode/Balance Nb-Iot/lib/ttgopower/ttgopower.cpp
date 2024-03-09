#include "ttgopower.h"


XPowersPMU PMU;
static int bat = PMU.getBatteryPercent();


void initPMU() {
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("Failed to initialize power.....");
    while (1) {
      delay(4000);
    }
  }
  //bat = PMU.getBatteryPercent();
  //Set the working voltage of the modem, please do not modify the parameters
  PMU.setDC5Voltage(3300);  //SIM7080 Modem main power channel 2700~ 3400V
  PMU.enableDC5();
   //Set the working voltage of the modem, please do not modify the parameters
  //PMU.setDC3Voltage(3000);  //SIM7080 Modem main power channel 2700~ 3400V
  //PMU.enableDC3();
/*
  //Modem GPS Power channel
  PMU.setBLDO2Voltage(3300);
  PMU.enableBLDO2();  //The antenna power must be turned on to use the GPS function
*/
 // PMU.enableBattVoltageMeasure();
  //PMU.enableSystemVoltageMeasure();
  // TS Pin detection must be disable, otherwise it cannot be charged
  //PMU.disableTSPinMeasure();
  

}
void modemoff(){
	PMU.disableDC5();
}

void scaleoff(){
	PMU.disableDC3();
}

void batpourcent(){
  Serial.print("getBatteryPercent:");
  Serial.print(PMU.getBatteryPercent());
  Serial.println("%");
  
  }