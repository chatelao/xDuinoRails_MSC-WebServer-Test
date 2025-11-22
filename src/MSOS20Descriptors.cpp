#include "tusb.h"
#include "USBD_Net_Custom.h"

// Vendor Code for MS OS 2.0 Descriptor
#define VENDOR_REQUEST_MICROSOFT 0xEE

// Total length of MS OS 2.0 Descriptor Set
// Header(10) + ConfigSubset(8) + FunctionSubset(8) + CompatibleID(20) = 46 bytes
#define MS_OS_20_DESC_LEN  0x2E

// BOS Descriptor is required for MS OS 2.0
// It contains the Platform Capability Descriptor with MS OS 2.0 UUID
uint8_t const desc_bos[] = {
    // Total length, number of device caps
    TUD_BOS_DESCRIPTOR(TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN, 1),

    // Microsoft OS 2.0 descriptor
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};

uint8_t desc_ms_os_20[] = {
    // Set header: length, type, windows version, total length
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR),
    U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // Configuration subset header: length, type, configuration index, reserved, configuration total length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION),
    0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

    // Function Subset header: length, type, first interface, reserved, subset length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION),
    0 /*itf num*/, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08),

    // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID),
    'R', 'N', 'D', 'I', 'S', 0x00, 0x00, 0x00, // Compatible ID: RNDIS
    '5', '1', '6', '2', '0', '0', '1', 0x00,   // Sub-compatible ID: 5162001
};

extern "C" {

uint8_t const *tud_descriptor_bos_cb(void) {
  return desc_bos;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  // Nothing to do with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) {
    return true;
  }

  if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
    if (request->bRequest == VENDOR_REQUEST_MICROSOFT) {
      if (request->wIndex == 7) {
        // Get Microsoft OS 2.0 compatible descriptor

        // Update the Interface Number in the descriptor
        // Header(10) + ConfigSubset(8) + FunctionSubsetHeader(8)
        // FunctionSubsetHeader starts at offset 18.
        // bFirstInterface is at offset 4 of FunctionSubsetHeader.
        // So index is 18 + 4 = 22.

        uint8_t itf_num = usb_net.getInterfaceNumber();
        desc_ms_os_20[22] = itf_num;

        return tud_control_xfer(rhport, request, (void *)desc_ms_os_20, MS_OS_20_DESC_LEN);
      }
    }
  }

  return false;
}

} // extern "C"
