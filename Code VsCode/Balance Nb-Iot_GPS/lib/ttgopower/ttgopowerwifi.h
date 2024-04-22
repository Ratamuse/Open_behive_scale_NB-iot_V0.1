#ifndef TTGOPOWERWIFI_H
#define TTGOPOWERWIFI_H
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
extern XPowersPMU PMU;

void initPMU();
//void modemoff();
//void scaleoff();
//void batpourcent();
#define PWDN_GPIO_NUM               (-1)
#define RESET_GPIO_NUM              (18)
#define XCLK_GPIO_NUM               (8)
#define SIOD_GPIO_NUM               (2)
#define SIOC_GPIO_NUM               (1)
#define VSYNC_GPIO_NUM              (16)
#define HREF_GPIO_NUM               (17)
#define PCLK_GPIO_NUM               (12)


#define I2C_SDA                     (15)
#define I2C_SCL                     (7)

#define PMU_INPUT_PIN               (6)

#define BUTTON_CONUT                (1)
#define USER_BUTTON_PIN             (0)
#define BUTTON_ARRAY                {USER_BUTTON_PIN}

#define BOARD_MODEM_PWR_PIN         (41)
#define BOARD_MODEM_DTR_PIN         (42)
#define BOARD_MODEM_RI_PIN          (3)
#define BOARD_MODEM_RXD_PIN         (4)
#define BOARD_MODEM_TXD_PIN         (5)

#define USING_MODEM

#define SDMMC_CMD                   (39)
#define SDMMC_CLK                   (38)
#define SDMMC_DATA                  (40)


#endif