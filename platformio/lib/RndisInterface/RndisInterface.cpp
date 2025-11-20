#include "RndisInterface.h"
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/etharp.h>
#include <lwip/apps/httpd.h>
#include <lwip/apps/fs.h>

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

void msc_setup() {
  usb_msc.setID("Seeed", "XIAO RP2040", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  usb_msc.setCapacity(MSC_RAM_BLOCK_COUNT, MSC_RAM_BLOCK_SIZE);
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
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
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
