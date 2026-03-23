#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "middleware/rpc_types.h"

#ifndef FALLBACK_SERVER_IP
#define FALLBACK_SERVER_IP "192.168.0.10"
#endif

#ifndef FALLBACK_SERVER_PORT
#define FALLBACK_SERVER_PORT 5000u
#endif

typedef struct {
  bool success;
  RpcError error_code;
} RpcSimpleResult;

typedef struct {
  bool success;
  RpcError error_code;
  int powerup_remaining_s;
} RpcPowerupResult;

typedef struct {
  bool success;
  RpcError error_code;
  int global_score;
  int node_scores[3];
} RpcScoreResult;

void rpc_client_set_server(const char *ip, uint16_t port);
void rpc_client_set_server_fallback(void);

RpcSimpleResult rpc_init(void);
RpcSimpleResult rpc_register_node(uint8_t node_id);
RpcClickResult rpc_add_clicks(int clicks, uint32_t lamport_ts);
RpcClickResult rpc_sync_offline(int clicks, uint32_t lamport_ts);
RpcPowerupResult rpc_activate_powerup(void);
RpcScoreResult rpc_get_scores(void);

void rpc_print_diagnostics(void);
bool rpc_is_connected(void);

#endif /* RPC_CLIENT_H */
