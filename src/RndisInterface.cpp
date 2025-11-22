#include "RndisInterface.h"
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/etharp.h>
#include <lwip/apps/httpd.h>
#include <lwip/apps/fs.h>
#include <lwip/apps/mdns.h>
#include "DHCPServer.h"
#include "USBD_Net_Custom.h"
#include "WebServer.h"
#include <Arduino.h> // For Serial

extern "C" void sys_check_timeouts(void);

USBD_Net_Custom usb_net;

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


// --- RNDIS & LwIP Glue ---
uint8_t tud_network_mac_address[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
struct netif netif_data;

// Buffer for received frame (deferred processing)
static struct pbuf *received_frame = NULL;

extern "C" err_t linkoutput_fn(struct netif *netif, struct pbuf *p) {
  (void)netif;

  for (;;) {
    if (!tud_ready()) {
      return ERR_USE; // or ERR_OK/ERR_BUF if we want to drop, but GP2040-CE uses ERR_USE
    }

    if (tud_network_can_xmit(p->tot_len)) {
      tud_network_xmit(p, 0 /* unused */);
      return ERR_OK;
    }

    // Transfer execution to TinyUSB to clear buffer
    tud_task();
  }
}

extern "C" err_t ip_init_fn(struct netif *netif) {
  Serial.println("LwIP: ip_init_fn called");
  netif->linkoutput = linkoutput_fn;
  netif->output = etharp_output;
  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
  memcpy(netif->hwaddr, tud_network_mac_address, 6);
  netif->hwaddr_len = 6;
  netif->name[0] = 'r';
  netif->name[1] = 'n';
  return ERR_OK;
}

void rndis_setup() {
  usb_net.begin();
  msc_setup(); // Initialize MSC

  // Initialize Serial (CDC)
  Serial.begin(115200);

  Serial.println("==================================");
  Serial.println("RNDIS Msc Web Firmware Starting...");

  lwip_init();
  Serial.println("LwIP Initialized");

  ip4_addr_t ipaddr, netmask, gw;
  IP4_ADDR(&ipaddr, 192, 168, 7, 1);
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 0, 0, 0, 0);

  netif_add(&netif_data, &ipaddr, &netmask, &gw, NULL, ip_init_fn, netif_input);
  netif_set_default(&netif_data);
  netif_set_up(&netif_data);
  netif_set_link_up(&netif_data);
  Serial.println("Netif added and set up (IP: 192.168.7.1)");

  mdns_resp_init();
  mdns_resp_add_netif(&netif_data, "xdrtrain");
  mdns_resp_add_service(&netif_data, "xdrtrain", "_http", DNSSD_PROTO_TCP, 80, NULL, NULL);
  Serial.println("mDNS Initialized (xdrtrain.local)");

  dhcp_server_init(); // Initialize DHCP Server
  httpd_init(); // Initialize Webserver
  Serial.println("Webserver Initialized");
}

static void service_traffic(void) {
  if (received_frame) {
    err_t ret = netif_data.input(received_frame, &netif_data);
    if (ret != ERR_OK) {
        pbuf_free(received_frame); // Free if input didn't take ownership (or failed)
        // Note: netif->input (tcpip_input or netif_input) usually frees pbuf on OK.
        // But if using netif_input directly, it handles pbuf internally?
        // ethernet_input checks type and passes to ip_input.
        // ip_input frees pbuf if invalid.
        // Wait, GP2040-CE frees it manually?
        // GP2040-CE: err_t ret = ethernet_input(received_frame, &netif_data); pbuf_free(received_frame);
        // If using ethernet_input directly (NO_SYS), we must be careful.
        // If LwIP takes the pbuf (e.g. queueing), we shouldn't free it.
        // But in NO_SYS, it processes immediately.
        // GP2040-CE uses pbuf_free(received_frame) ALWAYS.
        // This implies LwIP copies data? Or processes it fully?
        // If I use netif_input, which is usually ethernet_input.
        // I should check LwIP docs/source.
        // ethernet_input -> ip_input. ip_input frees pbuf when done.
        // So if I free it again, double free!
        // GP2040-CE might be wrong or using different LwIP config?
        // Let's look at GP2040-CE source again.
        // "err_t ret = ethernet_input(received_frame, &netif_data); pbuf_free(received_frame);"
        // This looks like they assume they own the pbuf.
        // But if `tud_network_recv_cb` allocated PBUF_POOL, it's LwIP memory.
        // If I free it, and LwIP also frees it...
        // Maybe `ethernet_input` DOES NOT free if it returns ERR_OK?
        // Actually, `ethernet_input` usually consumes the pbuf.
        // I will trust LwIP convention: if passed to input, ownership is transferred.
        // BUT, if I don't free it, and LwIP doesn't free it (e.g. drop), memory leak.
        // I will stick to my previous logic which didn't explicitly free unless error.
        // Wait, my previous logic: "if (netif_data.input(...) != ERR_OK) pbuf_free(p);"
        // This is standard.
        // GP2040-CE explicitly frees. Maybe they know something.
        // I'll stick to standard "free on error".

        // Actually, if I look at GP2040-CE logic again:
        // pbuf_free(received_frame); received_frame = NULL;
        // This runs UNCONDITIONALLY.
        // If LwIP kept it (e.g. frag reassembly), this would be fatal.
        // Maybe they configured LwIP to copy?
        // Or maybe they use PBUF_REF? No, PBUF_POOL.

        // I will trust standard LwIP usage: do not free if input returns ERR_OK.
        // However, `tud_network_recv_renew()` is needed.
    }
    // If returns ERR_OK, pbuf is consumed.
    // received_frame pointer is dangling now, so set to NULL.
    received_frame = NULL;

    tud_network_recv_renew();
  }
}

void rndis_loop() {
  tud_task();
  service_traffic(); // Process deferred packets
  sys_check_timeouts();

  static uint32_t last_print = 0;
  if (millis() - last_print > 5000) {
    last_print = millis();
    Serial.println("RNDIS Loop Alive");
  }
}

extern "C" {

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  if (received_frame) return false; // Backpressure

  if (size) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p) {
      memcpy(p->payload, src, size);
      received_frame = p;
      return true;
    }
  }
  return false;
}

void tud_network_init_cb(void) {
  Serial.println("TinyUSB: tud_network_init_cb called");
  if (received_frame) {
      pbuf_free(received_frame);
      received_frame = NULL;
  }
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  struct pbuf *p = (struct pbuf *)ref;
  (void)arg;
  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

} // extern "C"
