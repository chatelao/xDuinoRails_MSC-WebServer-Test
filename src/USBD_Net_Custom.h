#ifndef USBD_NET_CUSTOM_H
#define USBD_NET_CUSTOM_H

#include "Adafruit_USBD_Device.h"

class USBD_Net_Custom : public Adafruit_USBD_Interface {
public:
  USBD_Net_Custom();
  bool begin();
  uint8_t getInterfaceNumber() const;
  virtual uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf, uint16_t bufsize);

private:
  uint8_t _itf_num;
  uint8_t _str_idx;
  uint8_t _ep_notif;
  uint8_t _ep_in;
  uint8_t _ep_out;
};

extern USBD_Net_Custom usb_net;

#endif
