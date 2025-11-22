#include "DHCPServer.h"
#include <lwip/udp.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <string.h>

// DHCP Constants
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_OP_REQUEST 1
#define DHCP_OP_REPLY   2

#define DHCP_HTYPE_ETH  1
#define DHCP_HLEN_ETH   6

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_OPTION_PAD         0
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER      3
#define DHCP_OPTION_DNS_SERVER  6
#define DHCP_OPTION_REQ_IP      50
#define DHCP_OPTION_MSG_TYPE    53
#define DHCP_OPTION_SERVER_ID   54
#define DHCP_OPTION_END         255

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_DECLINE  4
#define DHCP_ACK      5
#define DHCP_NAK      6
#define DHCP_RELEASE  7
#define DHCP_INFORM   8

// DHCP Header Structure
#pragma pack(push, 1)
typedef struct {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t siaddr;
  uint32_t giaddr;
  uint8_t chaddr[16];
  uint8_t sname[64];
  uint8_t file[128];
  uint32_t magic_cookie;
  uint8_t options[308]; // Adjustable size
} dhcp_packet_t;
#pragma pack(pop)

static struct udp_pcb *dhcp_pcb = NULL;

// Helper to append option
static uint8_t *add_option(uint8_t *opt_ptr, uint8_t code, uint8_t len, void *data) {
  *opt_ptr++ = code;
  *opt_ptr++ = len;
  memcpy(opt_ptr, data, len);
  return opt_ptr + len;
}

static uint8_t *add_option_byte(uint8_t *opt_ptr, uint8_t code, uint8_t val) {
  *opt_ptr++ = code;
  *opt_ptr++ = 1;
  *opt_ptr++ = val;
  return opt_ptr;
}

static void dhcp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  (void)arg;
  (void)pcb;

  if (p->len < sizeof(dhcp_packet_t) - 308) {
    pbuf_free(p);
    return;
  }

  dhcp_packet_t *req = (dhcp_packet_t *)p->payload;

  // Verify Magic Cookie
  if (ntohl(req->magic_cookie) != DHCP_MAGIC_COOKIE) {
    pbuf_free(p);
    return;
  }

  // Parse Options to find Message Type
  uint8_t *opt_ptr = req->options;
  uint8_t *end_ptr = (uint8_t *)req + p->len;
  uint8_t msg_type = 0;

  while (opt_ptr < end_ptr) {
    uint8_t code = *opt_ptr++;
    if (code == DHCP_OPTION_END) break;
    if (code == DHCP_OPTION_PAD) continue;

    if (opt_ptr >= end_ptr) break; // Malformed
    uint8_t len = *opt_ptr++;

    if (opt_ptr + len > end_ptr) break; // Malformed

    if (code == DHCP_OPTION_MSG_TYPE && len == 1) {
      msg_type = *opt_ptr;
    }
    opt_ptr += len;
  }

  if (msg_type != DHCP_DISCOVER && msg_type != DHCP_REQUEST) {
    pbuf_free(p);
    return;
  }

  // Prepare Reply
  struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(dhcp_packet_t), PBUF_RAM);
  if (!reply_pbuf) {
    pbuf_free(p);
    return;
  }

  dhcp_packet_t *reply = (dhcp_packet_t *)reply_pbuf->payload;
  memset(reply, 0, sizeof(dhcp_packet_t));

  reply->op = DHCP_OP_REPLY;
  reply->htype = DHCP_HTYPE_ETH;
  reply->hlen = DHCP_HLEN_ETH;
  reply->xid = req->xid;
  reply->flags = req->flags; // Copy flags (broadcast bit)
  reply->ciaddr = 0;

  // Assign IP: 192.168.7.2
  IP4_ADDR((ip4_addr_t *)&reply->yiaddr, 192, 168, 7, 2);

  // Server IP: 192.168.7.1
  IP4_ADDR((ip4_addr_t *)&reply->siaddr, 192, 168, 7, 1);

  memcpy(reply->chaddr, req->chaddr, 16);
  reply->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

  // Options
  uint8_t *reply_opts = reply->options;

  // Message Type
  uint8_t reply_type = (msg_type == DHCP_DISCOVER) ? DHCP_OFFER : DHCP_ACK;
  reply_opts = add_option_byte(reply_opts, DHCP_OPTION_MSG_TYPE, reply_type);

  // Server Identifier
  uint32_t server_ip_n;
  IP4_ADDR((ip4_addr_t *)&server_ip_n, 192, 168, 7, 1);
  reply_opts = add_option(reply_opts, DHCP_OPTION_SERVER_ID, 4, &server_ip_n);

  // Subnet Mask
  uint32_t subnet_mask_n;
  IP4_ADDR((ip4_addr_t *)&subnet_mask_n, 255, 255, 255, 0);
  reply_opts = add_option(reply_opts, DHCP_OPTION_SUBNET_MASK, 4, &subnet_mask_n);

  // Router
  reply_opts = add_option(reply_opts, DHCP_OPTION_ROUTER, 4, &server_ip_n);

  // DNS (same as server for simplicity, or 8.8.8.8)
  reply_opts = add_option(reply_opts, DHCP_OPTION_DNS_SERVER, 4, &server_ip_n);

  *reply_opts++ = DHCP_OPTION_END;

  // Send Reply
  // Destination: If broadcast bit is set or client has no IP, broadcast.
  // Ideally, send to 255.255.255.255 port 68.
  ip_addr_t broadcast_addr;
  IP_ADDR4(&broadcast_addr, 255, 255, 255, 255);

  udp_sendto(dhcp_pcb, reply_pbuf, &broadcast_addr, DHCP_CLIENT_PORT);

  pbuf_free(reply_pbuf);
  pbuf_free(p);
}

void dhcp_server_init(void) {
  dhcp_pcb = udp_new();
  if (dhcp_pcb) {
    udp_bind(dhcp_pcb, IP_ADDR_ANY, DHCP_SERVER_PORT);
    udp_recv(dhcp_pcb, dhcp_recv_callback, NULL);
  }
}
