#include <xDuinoRails_DccCvMscWeb.h>

#ifndef UNIT_TEST
void setup() {
  rndis_setup();
}

void loop() {
  rndis_loop();
}
#endif
