#include "RndisInterface.h"
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/etharp.h>
#include <lwip/apps/httpd.h>
#include <lwip/apps/fs.h>
#include <lwip/apps/mdns.h>

extern "C" void sys_check_timeouts(void);

// --- Mass Storage Class (MSC) Settings ---
#define MSC_RAM_BLOCK_SIZE 512
#define MSC_RAM_BLOCK_COUNT 128 // 64KB RAM Disk
uint8_t msc_disk[MSC_RAM_BLOCK_COUNT][MSC_RAM_BLOCK_SIZE];

Adafruit_USBD_MSC usb_msc;

int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  if (lba >= MSC_RAM_BLOCK_COUNT) return -1;
  uint32_t count = bufsize / MSC_RAM_BLOCK_SIZE;
  for (uint32_t i = 0; i < count; i++) {
    memcpy((uint8_t*)buffer + i * MSC_RAM_BLOCK_SIZE, msc_disk[lba + i], MSC_RAM_BLOCK_SIZE);
  }
  return bufsize;
}

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  if (lba >= MSC_RAM_BLOCK_COUNT) return -1;
  uint32_t count = bufsize / MSC_RAM_BLOCK_SIZE;
  for (uint32_t i = 0; i < count; i++) {
    memcpy(msc_disk[lba + i], (uint8_t*)buffer + i * MSC_RAM_BLOCK_SIZE, MSC_RAM_BLOCK_SIZE);
  }
  return bufsize;
}

void msc_flush_cb() { }

void format_msc_disk() {
  memset(msc_disk, 0, sizeof(msc_disk));

  // --- Boot Sector (Sector 0) ---
  uint8_t *bs = msc_disk[0];
  bs[0] = 0xEB; bs[1] = 0x3C; bs[2] = 0x90; // Jump to code
  memcpy(bs + 3, "MSDOS5.0", 8);            // OEM Name
  bs[11] = 0x00; bs[12] = 0x02;             // Bytes per sector: 512
  bs[13] = 0x01;                            // Sectors per cluster: 1
  bs[14] = 0x01; bs[15] = 0x00;             // Reserved sectors: 1
  bs[16] = 0x02;                            // Number of FATs: 2
  bs[17] = 0x10; bs[18] = 0x00;             // Root entries: 16
  bs[19] = 0x80; bs[20] = 0x00;             // Total sectors: 128
  bs[21] = 0xF8;                            // Media descriptor
  bs[22] = 0x01; bs[23] = 0x00;             // Sectors per FAT: 1
  bs[510] = 0x55; bs[511] = 0xAA;           // Signature

  // --- FAT1 (Sector 1) & FAT2 (Sector 2) ---
  // FAT12 entries: 0xF8, 0xFF, 0xFF (for entries 0 and 1)
  // Entry 2 (EOF) for README.TXT: 0xFF, 0x0F
  // Byte sequence: F8 FF FF FF 0F 00 ...
  uint8_t fat_data[] = { 0xF8, 0xFF, 0xFF, 0xFF, 0x0F, 0x00 };
  memcpy(msc_disk[1], fat_data, sizeof(fat_data));
  memcpy(msc_disk[2], fat_data, sizeof(fat_data));

  // --- Root Directory (Sector 3) ---
  uint8_t *rd = msc_disk[3];
  // Entry 0: README.TXT
  memcpy(rd, "README  TXT", 11);
  rd[11] = 0x20; // Attribute: Archive
  // Time/Date fields can be 0 or generic
  rd[26] = 0x02; rd[27] = 0x00; // Starting cluster: 2

  const char *readme_content = "RP2040 RNDIS Webserver\r\n\r\nConnect to http://192.168.7.1";
  uint32_t file_size = strlen(readme_content);
  rd[28] = file_size & 0xFF;
  rd[29] = (file_size >> 8) & 0xFF;
  rd[30] = (file_size >> 16) & 0xFF;
  rd[31] = (file_size >> 24) & 0xFF;

  // --- Data (Sector 4 / Cluster 2) ---
  memcpy(msc_disk[4], readme_content, file_size);
}

void msc_setup() {
  format_msc_disk(); // Initialize RAM disk with FAT12
  usb_msc.setID("Seeed", "XIAO RP2040", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  usb_msc.setCapacity(MSC_RAM_BLOCK_COUNT, MSC_RAM_BLOCK_SIZE);
  usb_msc.setUnitReady(true); // Make sure it's ready
  usb_msc.begin();
}

// --- HTTPD Custom FS ---
// Note: We assume LWIP_HTTPD_CUSTOM_FILES is enabled via build flags.
// If not, these won't be called and linker will fail on fs_open in httpd.c
// We wrap them in extern "C" because httpd.c is C.

extern "C" {

int fs_open_custom(struct fs_file *file, const char *name) {
  if (strcmp(name, "/index.html") == 0 || strcmp(name, "/") == 0) {
    static const char *html = "<html><body><h1>Hello from RP2040 RNDIS!</h1><p>Webserver over USB working.</p></body></html>";
    file->data = (const char *)html;
    file->len = strlen(html);
    file->index = file->len;
    file->flags = 0;
    return 1;
  }
  return 0;
}

void fs_close_custom(struct fs_file *file) {
  (void)file;
}

int fs_read_custom(struct fs_file *file, char *buffer, int count) {
  return 0;
}

int fs_bytes_left_custom(struct fs_file *file) {
  return file->len - file->index;
}

} // extern "C"

// --- RNDIS & LwIP Glue ---
uint8_t tud_network_mac_address[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
struct netif netif_data;
uint8_t transmit_buffer[1520];

extern "C" err_t linkoutput_fn(struct netif *netif, struct pbuf *p) {
  (void)netif;

  if (p->tot_len > sizeof(transmit_buffer)) {
    return ERR_VAL;
  }

  pbuf_copy_partial(p, transmit_buffer, p->tot_len, 0);

  if (tud_network_can_xmit(p->tot_len)) {
      tud_network_xmit(transmit_buffer, p->tot_len);
      return ERR_OK;
  }

  return ERR_MEM;
}

extern "C" err_t ip_init_fn(struct netif *netif) {
  netif->linkoutput = linkoutput_fn;
  netif->output = etharp_output;
  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP | NETIF_FLAG_IGMP;
  memcpy(netif->hwaddr, tud_network_mac_address, 6);
  netif->hwaddr_len = 6;
  netif->name[0] = 'r';
  netif->name[1] = 'n';
  return ERR_OK;
}

void rndis_setup() {
  msc_setup(); // Initialize MSC

  lwip_init();

  ip4_addr_t ipaddr, netmask, gw;
  IP4_ADDR(&ipaddr, 192, 168, 7, 1);
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 0, 0, 0, 0);

  netif_add(&netif_data, &ipaddr, &netmask, &gw, NULL, ip_init_fn, netif_input);
  netif_set_default(&netif_data);

  mdns_resp_init();
  mdns_resp_add_netif(&netif_data, "xdrtrain");
  mdns_resp_add_service(&netif_data, "xdrtrain", "_http", DNSSD_PROTO_TCP, 80, NULL, NULL);

  httpd_init(); // Initialize Webserver

  Serial.begin(115200);
}

void rndis_loop() {
  tud_task();
  sys_check_timeouts();
}

extern "C" {

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
  if (p) {
    pbuf_take(p, src, size);
    if (netif_data.input(p, &netif_data) != ERR_OK) {
      pbuf_free(p);
    }
    return true;
  }
  return false;
}

void tud_network_init_cb(void) {
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  // Placeholder
  (void)dst;
  (void)ref;
  (void)arg;
  return 0;
}

} // extern "C"
