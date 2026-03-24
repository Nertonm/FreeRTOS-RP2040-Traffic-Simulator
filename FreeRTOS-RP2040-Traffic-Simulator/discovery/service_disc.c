#include "discovery/service_disc.h"

#include "lwip/udp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NODE_ID
#define NODE_ID 0
#endif

#define DISC_PORT 9999
#define RESP_PREFIX "COOKIE_SERVER:"
#define REQ_FORMAT "COOKIE_DISCOVER:NODE_ID:%d"

static volatile bool g_disc_success = false;
static ip_addr_t g_disc_ip;
static uint16_t g_disc_port = 0;

static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr,
                              u16_t port) {
  (void)arg;
  (void)pcb;
  (void)port;

  if (p == NULL) {
    return;
  }

  char buf[128];
  uint16_t copy_len = p->tot_len < (sizeof(buf) - 1u) ? (uint16_t)p->tot_len : (sizeof(buf) - 1u);

  pbuf_copy_partial(p, buf, copy_len, 0);
  buf[copy_len] = '\0';

  if (strncmp(buf, RESP_PREFIX, strlen(RESP_PREFIX)) == 0) {
    int parsed_port = atoi(buf + strlen(RESP_PREFIX));
    if (parsed_port > 0 && parsed_port <= 65535) {
      ip_addr_copy(g_disc_ip, *addr);
      g_disc_port = (uint16_t)parsed_port;
      g_disc_success = true;
    }
  }

  pbuf_free(p);
}

bool service_disc_discover(ip_addr_t *out_ip, uint16_t *out_port,
                           absolute_time_t timeout_deadline) {
  g_disc_success = false;
  g_disc_port = 0;

  struct udp_pcb *pcb = udp_new();
  if (pcb == NULL) {
    printf("[DISC] Falha ao alocar UDP PCB\n");
    return false;
  }

  err_t err = udp_bind(pcb, IP_ANY_TYPE, 0);
  if (err != ERR_OK) {
    printf("[DISC] Falha no bind UDP err=%d\n", err);
    udp_remove(pcb);
    return false;
  }

  udp_recv(pcb, udp_recv_callback, NULL);

  char req_buf[64];
  snprintf(req_buf, sizeof(req_buf), REQ_FORMAT, NODE_ID);

  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)strlen(req_buf), PBUF_RAM);
  if (p == NULL) {
    printf("[DISC] Falha ao alocar pbuf\n");
    udp_remove(pcb);
    return false;
  }
  memcpy(p->payload, req_buf, strlen(req_buf));

  err = udp_sendto(pcb, p, IP_ADDR_BROADCAST, DISC_PORT);
  pbuf_free(p);

  if (err != ERR_OK) {
    printf("[DISC] Falha ao enviar broadcast UDP err=%d\n", err);
    udp_remove(pcb);
    return false;
  }

  printf("[DISC] Broadcast enviado: %s\n", req_buf);

  while (absolute_time_diff_us(get_absolute_time(), timeout_deadline) > 0) {
    if (g_disc_success) {
      break;
    }
    sleep_ms(10);
  }

  udp_recv(pcb, NULL, NULL);
  udp_remove(pcb);

  if (g_disc_success) {
    ip_addr_copy(*out_ip, g_disc_ip);
    *out_port = g_disc_port;
    printf("[DISC] Sucesso: %s:%u\n", ip4addr_ntoa(ip_2_ip4(out_ip)), (unsigned)*out_port);
    return true;
  }

  printf("[DISC] Timeout\n");
  return false;
}
