#include "USBD_Net_Custom.h"

USBD_Net_Custom::USBD_Net_Custom() {
  _itf_num = 0;
  _str_idx = 0;
  _ep_notif = 0;
  _ep_in = 0;
  _ep_out = 0;
}

bool USBD_Net_Custom::begin() {
  // RNDIS uses 2 interfaces: Control (Abstract Control Model) + Data
  _itf_num = USBDevice.allocInterface(2);
  _str_idx = USBDevice.addStringDescriptor("RNDIS");

  _ep_notif = USBDevice.allocEndpoint(true); // IN
  _ep_in = USBDevice.allocEndpoint(true);    // IN
  _ep_out = USBDevice.allocEndpoint(false);  // OUT

  return USBDevice.addInterface(*this);
}

uint16_t USBD_Net_Custom::getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf, uint16_t bufsize) {
  (void)itfnum; // We use the allocated interface number

  // TUD_RNDIS_DESCRIPTOR(_itfnum, _stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize)
  uint8_t desc[] = { TUD_RNDIS_DESCRIPTOR(_itf_num, _str_idx, _ep_notif, 8, _ep_out, _ep_in, 64) };

  uint16_t len = sizeof(desc);

  if (buf) {
    if (bufsize < len) return 0;
    memcpy(buf, desc, len);
  }

  return len;
}
