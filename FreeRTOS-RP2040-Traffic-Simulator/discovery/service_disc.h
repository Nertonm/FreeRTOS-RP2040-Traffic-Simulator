#ifndef SERVICE_DISC_H
#define SERVICE_DISC_H

#include <stdbool.h>
#include <stdint.h>

#include "lwip/ip_addr.h"
#include "pico/stdlib.h"

/*
 * Descobre servidor via UDP broadcast e retorna IP/porta anunciados.
 * Retorna true quando uma resposta valida chega antes do timeout.
 */
bool service_disc_discover(ip_addr_t *out_ip, uint16_t *out_port, absolute_time_t timeout);

#endif /* SERVICE_DISC_H */
