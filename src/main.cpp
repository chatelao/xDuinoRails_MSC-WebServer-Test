#include <Adafruit_TinyUSB.h>

// 8KB is the smallest size that windows allow to mount
#define DISK_BLOCK_NUM  16
#define DISK_BLOCK_SIZE 512

#define FILE_CONTENTS "hello.txt"

uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] = {
    //------------- Block 0: Boot Sector -------------//
    {
        0xEB, 0x3C, 0x90, 'M', 'S', 'D', 'O', 'S', '5', '.', '0', 0x00,
        0x02, 0x01, 0x01, 0x00, 0x01, 0x10, 0x00, 0x10, 0x00, 0xF8, 0x01, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00,
        'X', 'I', 'A', 'O', ' ', 'R', 'P', '2', '0', '4', '0', // Volume Label
        'F', 'A', 'T', '1', '2', ' ', ' ', ' ',
        // Rest of the boot sector is zeroed out
    },

    //------------- Block 1: FAT12 Table -------------//
    {
        0xF8, 0xFF, 0xFF, 0xFF, 0x0F, 0x00
    },

    //------------- Block 2: Root Directory -------------//
    {
        // First entry is volume label
        'X', 'I', 'A', 'O', ' ', 'R', 'P', '2', '0', '4', '0', 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Second entry is "info.txt"
        'I', 'N', 'F', 'O', ' ', ' ', ' ', ' ', 'T', 'X', 'T', 0x20, 0x00, 0xC6,
        0x52, 0x6D, 0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43,
        0x02, 0x00, sizeof(FILE_CONTENTS) - 1, 0x00, 0x00, 0x00
    },

    //------------- Block 3: File Content -------------//
    FILE_CONTENTS
};


// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

// Callbacks
int32_t msc_read_callback(uint32_t lba, void* buffer, uint32_t bufsize);
int32_t msc_write_callback(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
void msc_flush_callback(void);
bool msc_ready_callback(void);

void setup() {
  Serial.begin(115200);

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Seeed", "Xiao RP2040", "1.0");

  // Set disk size
  usb_msc.setCapacity(DISK_BLOCK_NUM, DISK_BLOCK_SIZE);

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_callback, msc_write_callback, msc_flush_callback);
  usb_msc.setReadyCallback(msc_ready_callback);

  // Set Lun ready (RAM disk is always ready)
  usb_msc.setUnitReady(true);
  usb_msc.begin();

  Serial.println("Adafruit TinyUSB Mass Storage RAM Disk example");
}

void loop() {
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_callback(uint32_t lba, void* buffer, uint32_t bufsize) {
  memcpy(buffer, msc_disk[lba], bufsize);
  return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and
// return number of written bytes (must be multiple of block size)
int32_t msc_write_callback(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  memcpy(msc_disk[lba], buffer, bufsize);
  return bufsize;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_callback(void) {
  // nothing to do
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool msc_ready_callback(void) {
  return true;
}
