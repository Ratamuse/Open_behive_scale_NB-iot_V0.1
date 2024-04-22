#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

#include <StreamDebugger.h>

#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#define DUMP_AT_COMMANDS

#define TINY_GSM_RX_BUFFER 1024

#ifdef DUMP_AT_COMMANDS
StreamDebugger debugger(Serial1, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#endif // DEBUG_CONFIG_H