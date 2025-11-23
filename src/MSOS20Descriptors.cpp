#include "tusb.h"
#include "USBD_Net_Custom.h"

#if !defined(CFG_TUD_VENDOR) || (CFG_TUD_VENDOR == 0)

// For CDC-NCM, we generally rely on standard descriptors and do not need
// Microsoft OS 2.0 Descriptors to force RNDIS driver loading.
// Therefore, we return NULL for BOS descriptor and handle no vendor requests.

extern "C" {

uint8_t const *tud_descriptor_bos_cb(void) {
  return NULL;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  (void)rhport;
  (void)stage;
  (void)request;
  return false;
}

} // extern "C"

#endif
