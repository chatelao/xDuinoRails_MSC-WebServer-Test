#include "USBD_Net_Custom.h"
#include <stdio.h>

extern uint8_t tud_network_mac_address[6];

USBD_Net_Custom::USBD_Net_Custom() {
  _itf_num = 0;
  _str_idx = 0;
  _mac_str_idx = 0;
  _ep_notif = 0;
  _ep_in = 0;
  _ep_out = 0;
}

uint8_t USBD_Net_Custom::getInterfaceNumber() const {
  return _itf_num;
}

bool USBD_Net_Custom::begin() {
  // NCM uses 2 interfaces: Control (CDC) + Data
  _itf_num = USBDevice.allocInterface(2);
  _str_idx = USBDevice.addStringDescriptor("TinyUSB NCM");

  char mac_str[13];
  sprintf(mac_str, "%02X%02X%02X%02X%02X%02X",
    tud_network_mac_address[0], tud_network_mac_address[1], tud_network_mac_address[2],
    tud_network_mac_address[3], tud_network_mac_address[4], tud_network_mac_address[5]);
  _mac_str_idx = USBDevice.addStringDescriptor(mac_str);

  _ep_notif = USBDevice.allocEndpoint(true); // IN
  _ep_in = USBDevice.allocEndpoint(true);    // IN
  _ep_out = USBDevice.allocEndpoint(false);  // OUT

  return USBDevice.addInterface(*this);
}

uint16_t USBD_Net_Custom::getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf, uint16_t bufsize) {
  (void)itfnum; // We use the allocated interface number

  // TUD_CDC_NCM_DESCRIPTOR(_itfnum, _desc_stridx, _mac_stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize, _maxsegmentsize)
  // _ep_notif_size is usually 64. _epsize (bulk) is 64.
  // _maxsegmentsize: 1514 (Ethernet MTU + header)
  uint8_t desc[] = { TUD_CDC_NCM_DESCRIPTOR(_itf_num, _str_idx, _mac_str_idx, _ep_notif, 64, _ep_out, _ep_in, 64, 1514) };

  uint16_t len = sizeof(desc);

  if (buf) {
    if (bufsize < len) return 0;
    memcpy(buf, desc, len);
  }

  return len;
}
