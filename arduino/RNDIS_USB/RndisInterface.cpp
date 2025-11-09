#include "RndisInterface.h"

uint8_t tud_network_mac_address[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

void rndis_setup() {
  Serial.begin(115200);
  Serial.println("TinyUSB RNDIS Test");
}

void rndis_loop() {
  tud_task();
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  return false;
}

void tud_network_init_cb(void) {
}
