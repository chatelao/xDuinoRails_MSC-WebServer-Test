#ifndef RNDISINTERFACE_H
#define RNDISINTERFACE_H

#include <Adafruit_TinyUSB.h>

extern uint8_t tud_network_mac_address[6];

void rndis_setup();
void rndis_loop();

#endif // RNDISINTERFACE_H
