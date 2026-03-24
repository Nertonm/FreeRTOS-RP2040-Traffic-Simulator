/**
 * @file task_rpc.c
 * @brief Implementação da tarefa de comunicação RPC e gerenciamento de rede.
 *
 * Esta tarefa orquestra a conexão WiFi, a descoberta do servidor e a
 * sincronização contínua de cliques e pontuações com o backend central.
 *
 * @author
 * @date 2026-03-01
 */

#include "FreeRTOS.h"
#include "task.h"

#include "config/firmware_config.h"
#include "config/net_config.h"
#include "debug_log.h"
#include "discovery/service_disc.h"
#include "hardware_config.h"
#include "middleware/app_queues.h"
#include "middleware/lamport.h"
#include "middleware/shared_state.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "rpc_client.h"
#include "task_rpc.h"
#include "tasks/task_car.h"
#include "tasks/task_clock.h"
#include "simulator/grid.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int g_interboard_sock = -1;

#define TRANSFER_RETRY_SLOTS 12u
#define TRANSFER_RETRY_ATTEMPTS 20u

typedef struct {
  bool in_use;
  CarState state;
  uint8_t src_node;
  uint8_t preferred_x;
  uint8_t attempts_left;
} TransferRetrySlot;

static TransferRetrySlot g_transfer_retry[TRANSFER_RETRY_SLOTS];

static void process_sim_rpc_message(const RpcMessage *msg);
static void process_transfer_retry_queue(void);

/**
 * @brief Aplica o efeito visual de Turbo localmente.
 *
 * @param[in] duration_ms Duração do efeito em milissegundos.
 */
static void apply_local_turbo(uint32_t duration_ms) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  shared_state_set_turbo_until_ms(now + duration_ms);
  shared_state_set_turbo_active(true);
}

/**
 * @brief Realiza a busca do servidor na rede e configura o cliente RPC.
 *
 * Tenta descobrir o servidor via UDP Broadcast. Caso falhe, utiliza
 * o endereço IP de fallback definido nas configurações.
 */
static void setup_network_target(void) {
  ip_addr_t discovered_ip;
  uint16_t discovered_port = 0;

  absolute_time_t timeout = make_timeout_time_ms(3000);
  if (service_disc_discover(&discovered_ip, &discovered_port, timeout)) {
    const char *ip_str = ip4addr_ntoa(ip_2_ip4(&discovered_ip));
    rpc_client_set_server(ip_str, discovered_port);
    shared_state_set_fallback_in_use(false);
    printf("[DISC] Servidor descoberto em %s:%u\n", ip_str, (unsigned)discovered_port);
  } else {
    rpc_client_set_server_fallback();
    shared_state_set_fallback_in_use(true);
    printf("[DISC] Discovery falhou, usando fallback\n");
  }
}

/**
 * @brief Inicializa e conecta ao ponto de acesso WiFi.
 *
 * @return bool Verdadeiro se a conexão foi estabelecida com sucesso.
 */
static bool setup_wifi(void) {
  cyw43_arch_enable_sta_mode();

  if (WIFI_SSID[0] == '\0' || WIFI_PASSWORD[0] == '\0') {
    printf("[WIFI] Credenciais ausentes; modo offline\n");
    shared_state_set_connection_status(STATUS_OFFLINE);
    return false;
  }

  printf("[WIFI] Conectando em %s...\n", WIFI_SSID);
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK,
                                         15000)) {
    printf("[WIFI] Falha de conexão\n");
    shared_state_set_connection_status(STATUS_OFFLINE);
    return false;
  }

  printf("[WIFI] Conectado\n");
  shared_state_set_connection_status(STATUS_CONNECTING);
  return true;
}

static void interboard_close_socket(void) {
  if (g_interboard_sock >= 0) {
    lwip_close(g_interboard_sock);
    g_interboard_sock = -1;
  }
}

static bool interboard_open_socket(void) {
  struct sockaddr_in bind_addr;
  int yes = 1;
  int nonblocking = 1;

  interboard_close_socket();

  g_interboard_sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
  if (g_interboard_sock < 0) {
    LOG_ERROR("[RPC]", "falha socket UDP interboard errno=%d", errno);
    return false;
  }

  (void)lwip_setsockopt(g_interboard_sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
  (void)lwip_setsockopt(g_interboard_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  (void)lwip_ioctl(g_interboard_sock, FIONBIO, &nonblocking);

  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(INTERBOARD_UDP_PORT);
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (lwip_bind(g_interboard_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
    LOG_ERROR("[RPC]", "falha bind UDP interboard errno=%d", errno);
    interboard_close_socket();
    return false;
  }

  LOG_NORMAL("[RPC]", "UDP interboard ativo porta=%u", (unsigned)INTERBOARD_UDP_PORT);
  return true;
}

static bool interboard_send_message(const RpcMessage *msg) {
  struct sockaddr_in dst;
  int sent;

  if (msg == NULL || g_interboard_sock < 0) {
    return false;
  }

  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port = htons(INTERBOARD_UDP_PORT);
  dst.sin_addr.s_addr = inet_addr("255.255.255.255");

  sent = lwip_sendto(g_interboard_sock, msg, sizeof(*msg), 0, (struct sockaddr *)&dst, sizeof(dst));
  return sent == (int)sizeof(*msg);
}

static void interboard_poll_incoming(void) {
  RpcMessage msg;
  struct sockaddr_in src;
  socklen_t src_len = sizeof(src);
  int n;

  if (g_interboard_sock < 0) {
    return;
  }

  while (1) {
    n = lwip_recvfrom(g_interboard_sock, &msg, sizeof(msg), 0, (struct sockaddr *)&src, &src_len);
    if (n < (int)sizeof(RpcMessage)) {
      break;
    }

    if (msg.src_node == (uint8_t)NODE_ID) {
      continue;
    }

    if (msg.dst_node == 0xFFu || msg.dst_node == (uint8_t)NODE_ID) {
      shared_state_set_tcp_active(true);
      process_sim_rpc_message(&msg);
      shared_state_set_tcp_active(false);
    }
  }
}

/**
 * @brief Tenta aplicar spawn de um carro recebido de outra placa.
 *
 * @param[in] state Estado recebido do carro.
 * @param[in] preferred_x Coluna de entrada esperada na placa destino.
 * @return true quando o spawn foi concluído.
 */
static bool try_spawn_transferred_car(const CarState *state, uint8_t preferred_x) {
  CarParams p;

  if (state == NULL) {
    return false;
  }

  p.id = 0;
  p.dir = state->dir;
  p.velocidade = state->velocidade;
  p.y = state->y;
  p.x = preferred_x;

  if (task_car_spawn(p)) {
    return true;
  }

  /* Mantem coerencia espacial: em transferencias entre placas, o carro deve
   * continuar entrando pelo mesmo lado (preferred_x), nunca pelo lado oposto.
   */
  for (uint8_t y = 0; y < GRID_ROWS; y++) {
    if (y == state->y) {
      continue;
    }

    if (!grid_can_enter(y, preferred_x, p.dir)) {
      continue;
    }

    p.y = y;
    p.x = preferred_x;
    if (task_car_spawn(p)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Enfileira uma transferência para retentativa quando há congestionamento na borda.
 *
 * @param[in] state Estado do carro transferido.
 * @param[in] src_node Nó de origem do carro transferido.
 * @param[in] preferred_x Coluna de entrada esperada no destino.
 */
static void queue_transfer_retry(const CarState *state, uint8_t src_node, uint8_t preferred_x) {
  uint8_t slot = TRANSFER_RETRY_SLOTS;

  if (state == NULL) {
    return;
  }

  for (uint8_t i = 0; i < TRANSFER_RETRY_SLOTS; i++) {
    if (!g_transfer_retry[i].in_use) {
      slot = i;
      break;
    }
  }

  if (slot == TRANSFER_RETRY_SLOTS) {
    /* Sobrescreve o slot com menor numero de tentativas remanescentes. */
    uint8_t worst = 0;
    uint8_t min_attempts = 0xFFu;
    for (uint8_t i = 0; i < TRANSFER_RETRY_SLOTS; i++) {
      if (g_transfer_retry[i].attempts_left <= min_attempts) {
        min_attempts = g_transfer_retry[i].attempts_left;
        worst = i;
      }
    }
    slot = worst;
  }

  g_transfer_retry[slot].in_use = true;
  g_transfer_retry[slot].state = *state;
  g_transfer_retry[slot].src_node = src_node;
  g_transfer_retry[slot].preferred_x = preferred_x;
  g_transfer_retry[slot].attempts_left = TRANSFER_RETRY_ATTEMPTS;

  LOG_NORMAL("[RPC]", "transfer %u->%u em fila de retry y=%u x_side=%u tentativas=%u",
             (unsigned)src_node, (unsigned)NODE_ID, (unsigned)state->y, (unsigned)preferred_x,
             (unsigned)TRANSFER_RETRY_ATTEMPTS);
}

/**
 * @brief Processa a fila local de retentativas de transferências.
 */
static void process_transfer_retry_queue(void) {
  for (uint8_t i = 0; i < TRANSFER_RETRY_SLOTS; i++) {
    TransferRetrySlot *slot = &g_transfer_retry[i];

    if (!slot->in_use) {
      continue;
    }

    if (try_spawn_transferred_car(&slot->state, slot->preferred_x)) {
      LOG_NORMAL("[RPC]", "transfer retried aplicado no slot=%u y=%u x_side=%u", (unsigned)i,
                 (unsigned)slot->state.y, (unsigned)slot->preferred_x);
      slot->in_use = false;
      continue;
    }

    if (slot->attempts_left > 0u) {
      slot->attempts_left--;
    }

    if (slot->attempts_left == 0u) {
      LOG_NORMAL("[RPC]", "transfer descartado apos retry slot=%u y=%u x_side=%u", (unsigned)i,
                 (unsigned)slot->state.y, (unsigned)slot->preferred_x);
      slot->in_use = false;
    }
  }
}

static void process_outgoing_rpc_queue(void) {
  QueueHandle_t q = app_queues_get_rpc_out();
  RpcMessage msg;

  if (q == NULL) {
    return;
  }

  while (xQueueReceive(q, &msg, 0) == pdTRUE) {
    shared_state_set_tcp_active(true);

    if (msg.dst_node != (uint8_t)NODE_ID) {
      if (!interboard_send_message(&msg)) {
        LOG_ERROR("[RPC]", "falha envio interboard tipo=0x%02X %u->%u", (unsigned)msg.type,
                  (unsigned)msg.src_node, (unsigned)msg.dst_node);
      }
    }

    if (msg.dst_node == (uint8_t)NODE_ID) {
      process_sim_rpc_message(&msg);
    }

    shared_state_set_tcp_active(false);
  }
}

/**
 * @brief Processa uma RpcMessage recebida da rede TCP.
 *
 * Implementa os handlers das mensagens do simulador de tráfego.
 *
 * @param[in] msg Mensagem recebida.
 */
static void process_sim_rpc_message(const RpcMessage *msg) {
  if (!msg)
    return;

  switch (msg->type) {
  case MSG_HEARTBEAT:
    /* Manter do clicker sem mudança - keep alive da rede */
    LOG_VERBOSE("[RPC]", "MSG_HEARTBEAT recebido do no %d", msg->src_node);
    break;

  case MSG_SYNC_TICK: {
    if (msg->payload_len >= (2 * sizeof(uint32_t))) {
      uint32_t tick_value;
      uint32_t lamport_value;
      memcpy(&tick_value, msg->payload, sizeof(uint32_t));
      memcpy(&lamport_value, msg->payload + sizeof(uint32_t), sizeof(uint32_t));

#if NODE_ID != 1
      /* O master é o nó 1. Nós 0 e 2 sincronizam o tick local. */
      task_clock_sync_remote(tick_value, lamport_value);
#endif
      LOG_VERBOSE("[RPC]", "MSG_SYNC_TICK recebido: tick=%lu lamport=%lu",
                  (unsigned long)tick_value, (unsigned long)lamport_value);
    }
    break;
  }

  case MSG_TRANSFER_CAR: {
    if (msg->payload_len >= sizeof(CarState)) {
      CarState state;
      memcpy(&state, msg->payload, sizeof(CarState));
      uint8_t preferred_x;

      /* Lógica de Transferência de Borda:
       * Carro saindo pelo lado LESTE de Norte (0) → entra pela borda OESTE de Centro (1)
       * Carro saindo pelo lado LESTE de Centro (1) → entra pela borda OESTE de Sul (2) */
      if ((msg->src_node == 0 && NODE_ID == 1) || (msg->src_node == 1 && NODE_ID == 2)) {
        preferred_x = 0; /* Entra pelo OESTE */
      }
      /* Caminho inverso:
       * Carro saindo pelo lado OESTE de Sul (2) → entra pela borda LESTE de Centro (1)
       * Carro saindo pelo lado OESTE de Centro (1) → entra pela borda LESTE de Norte (0) */
      else if ((msg->src_node == 2 && NODE_ID == 1) || (msg->src_node == 1 && NODE_ID == 0)) {
        preferred_x = GRID_COLS - 1; /* Entra pelo LESTE */
      } else {
        /* Fallback pelas direções se a topologia for contornada */
        preferred_x = (state.dir == DIR_EAST) ? 0 : (GRID_COLS - 1);
      }

      if (!try_spawn_transferred_car(&state, preferred_x)) {
        queue_transfer_retry(&state, msg->src_node, preferred_x);
      }
    }
    break;
  }

  case MSG_ACK:
    LOG_VERBOSE("[RPC]", "MSG_ACK recebido do no %d", msg->src_node);
    break;

  default:
    LOG_ERROR("[RPC]", "Mensagem TCP desconhecida: 0x%02X", msg->type);
    break;
  }
}

/**
 * @brief Loop principal da tarefa de rede RPC.
 */
void task_rpc(void *param) {
  (void)param;

  if (cyw43_arch_init()) {
    printf("[RPC] ERRO: falha ao inicializar CYW43 (task)\n");
    shared_state_set_connection_status(STATUS_OFFLINE);
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  if (!interboard_open_socket()) {
    LOG_ERROR("[RPC]", "sem socket interboard, roaming remoto degradado");
  }

  /* Tenta conectar ao WiFi com retry periódico */
  TickType_t last_wifi_attempt = 0;
wifi_connect:
  if (last_wifi_attempt == 0 ||
      (xTaskGetTickCount() - last_wifi_attempt) >= pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS)) {
    last_wifi_attempt = xTaskGetTickCount();

    if (setup_wifi()) {
      goto wifi_connected;
    }
    shared_state_set_connection_status(STATUS_OFFLINE);
    printf("[WIFI] Próxima tentativa em %d segundos\n", WIFI_RETRY_INTERVAL_MS / 1000);
  }

  /* Loop de modo offline com retry periódico */
  {
    uint32_t offline_loop_iter = 0;
    while (1) {
      offline_loop_iter++;

      if (shared_state_take_turbo_activation_requested()) {
        apply_local_turbo(TURBO_DURATION_MS);
        LOG_NORMAL("[TURBO]", "Ativado em modo offline duracao_ms=%u", TURBO_DURATION_MS);
      }

      process_outgoing_rpc_queue();
      interboard_poll_incoming();
      process_transfer_retry_queue();

      /* Cliques ficam em pending_clicks (shared_state); task_buttons não usa
       * mais queue_clicks, então não há nada para drenar aqui. O display
       * mostra pending_clicks via core1_display/task_display automaticamente.
       */

      /* Verifica se é hora de tentar reconectar WiFi */
      if ((xTaskGetTickCount() - last_wifi_attempt) >= pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS)) {
        printf("[WIFI] Tentando reconexão...\n");
        goto wifi_connect;
      }

      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

wifi_connected:; /* empty statement: label cannot precede declaration in C99 */

#if !ENABLE_BACKEND_RPC
  shared_state_set_connection_status(STATUS_ONLINE);
  LOG_NORMAL("[RPC]", "modo P2P ativo: backend TCP desabilitado");

  while (1) {
    if (shared_state_take_turbo_activation_requested()) {
      apply_local_turbo(TURBO_DURATION_MS);
      LOG_NORMAL("[TURBO]", "Ativado em modo P2P duracao_ms=%u", TURBO_DURATION_MS);
    }

    process_outgoing_rpc_queue();
    interboard_poll_incoming();
    process_transfer_retry_queue();
    vTaskDelay(pdMS_TO_TICKS(RPC_POLL_PERIOD_MS));
  }
#endif

  RpcSimpleResult init_result = rpc_init();

  setup_network_target();
  if (!init_result.success) {
    shared_state_set_connection_status(STATUS_OFFLINE);
  }

  bool registered = false;
  TickType_t last_scores_refresh = xTaskGetTickCount();
  TickType_t last_register_attempt = 0;
  uint16_t consecutive_rpc_failures = 0;
  uint8_t consecutive_register_failures = 0;
  uint64_t last_heartbeat_us = 0; /* Timestamp do último heartbeat bem-sucedido */

  /* Backoff exponencial para retry de registro (definido em firmware_config.h)
   */
  static const uint32_t register_backoff_ms[] = {RPC_REGISTER_BACKOFF_1, RPC_REGISTER_BACKOFF_2,
                                                 RPC_REGISTER_BACKOFF_3, RPC_REGISTER_BACKOFF_MAX};

  static uint32_t loop_count = 0;
/* Intervalo para dump de stats peridiocs (a cada 500 iterações) */
#define STATS_DUMP_INTERVAL 500u
  while (1) {
    /* Marca o início do ciclo de 20ms para compensação de tempo ao final */
    uint64_t cycle_start = time_us_64();

    loop_count++;
    if ((loop_count % 100) == 0) {
      LOG_VERBOSE("[RPC]", "loop #%lu", (unsigned long)loop_count);
    }
    /* Dump periódico de estatísticas a cada STATS_DUMP_INTERVAL iterações */
    if ((loop_count % STATS_DUMP_INTERVAL) == 0) {
      rpc_print_diagnostics();
      lamport_print_diagnostics();
    }

    process_outgoing_rpc_queue();
    interboard_poll_incoming();
    process_transfer_retry_queue();

    /* Processamento de Power-up (Turbo) */
    if (shared_state_take_turbo_activation_requested()) {
      uint32_t turbo_ms = TURBO_DURATION_MS;
      if (registered) {
        RpcPowerupResult powerup = rpc_activate_powerup();
        if (powerup.success && powerup.powerup_remaining_s > 0) {
          turbo_ms = (uint32_t)powerup.powerup_remaining_s * 1000u;
        }
      }
      apply_local_turbo(turbo_ms);
      printf("[TURBO] Ativado por %lu ms\n", (unsigned long)turbo_ms);
    }

    /* Lógica de Registro Inicial e Reconexão */
    if (!registered) {
      TickType_t now_ticks = xTaskGetTickCount();
      /* Calcula delay com backoff exponencial */
      uint8_t backoff_idx = consecutive_register_failures;
      if (backoff_idx > 3)
        backoff_idx = 3;
      uint32_t current_retry_ms = register_backoff_ms[backoff_idx];

      if ((last_register_attempt == 0) ||
          ((now_ticks - last_register_attempt) >= pdMS_TO_TICKS(current_retry_ms))) {
        last_register_attempt = now_ticks;
        shared_state_set_connection_status(STATUS_CONNECTING);
        RpcSimpleResult reg = rpc_register_node((uint8_t)NODE_ID);
        if (reg.success) {
          registered = true;
          consecutive_rpc_failures = 0;
          consecutive_register_failures = 0;
          last_heartbeat_us = time_us_64(); /* Inicia contador do heartbeat a
                                               partir do registro */

          /* --- Início da lógica de sincronização --- */

          /* Pega todos os cliques pendentes para sincronizar */
          uint32_t total_to_sync = shared_state_take_pending_clicks();

          if (total_to_sync > 0) {
            shared_state_set_syncing_count(total_to_sync);
            shared_state_set_connection_status(STATUS_SYNCING);
            printf("[RPC] Sincronizando %lu cliques pendentes...\n", (unsigned long)total_to_sync);

            uint32_t lamport_sent = lamport_tick();
            RpcClickResult sync_res = rpc_sync_offline((int)total_to_sync, lamport_sent);

            if (sync_res.success) {
              lamport_update((uint32_t)sync_res.lamport_ts);
              shared_state_set_scores(&sync_res);

              printf("[RPC] Sync completo: global=%d local=%d lamport=%lu\n", sync_res.global_score,
                     sync_res.local_score, (unsigned long)sync_res.lamport_ts);

              if (sync_res.milestone_triggered) {
                shared_state_set_milestone_triggered(true);
                shared_state_set_led_flash_requested(true);
                printf("[MILESTONE] Marco atingido durante sync: %d\n", sync_res.milestone_value);
              }
            } else {
              printf("[RPC] ERRO: Sync falhou (erro=%d), voltando para OFFLINE\n",
                     sync_res.error_code);
              shared_state_restore_clicks(total_to_sync);
              shared_state_set_syncing_count(0);
              shared_state_set_connection_status(STATUS_OFFLINE);
              registered = false;
              continue;
            }
          }
          /* --- Fim da lógica de sincronização --- */

          shared_state_set_syncing_count(0);
          shared_state_set_connection_status(STATUS_ONLINE);
          shared_state_set_server_error_active(false);
          printf("[RPC] Nó %d registrado e sincronizado\n", NODE_ID);

          /* Atualiza scores imediatamente para o display mostrar valores
           * corretos mesmo se o nó ficar idle (sem clicar). Sem isso,
           * local_score e global_score ficam em 0 até o primeiro rpc_add_clicks
           * bem-sucedido. */
          {
            RpcScoreResult boot_scores = rpc_get_scores();
            if (boot_scores.success) {
              uint32_t boot_node_scores[MAX_NODES] = {0};
              for (uint8_t i = 0; i < MAX_NODES; i++) {
                boot_node_scores[i] =
                    (boot_scores.node_scores[i] >= 0) ? (uint32_t)boot_scores.node_scores[i] : 0;
              }
              uint32_t boot_global =
                  (boot_scores.global_score >= 0) ? (uint32_t)boot_scores.global_score : 0;
              /* Aplica o mesmo guard do ciclo de refresh: não sobrescreve com 0
               * para evitar perder a contagem global quando o nó está idle na
               * reconexão e o parse falha ou o servidor retorna 0 transitório. */
              if (boot_global > 0) {
                shared_state_set_global_score(boot_global);
              }
              shared_state_set_node_scores(boot_node_scores, MAX_NODES);
              uint32_t boot_local = boot_node_scores[NODE_ID % MAX_NODES];
              if (boot_local > 0) {
                shared_state_set_local_score(boot_local);
              }
            }
          }
        } else {
          consecutive_register_failures++;
          shared_state_set_connection_status(STATUS_OFFLINE);
          printf("[RPC] Registro falhou, próximo retry em %lu ms\n",
                 (unsigned long)register_backoff_ms[consecutive_register_failures > 3
                                                        ? 3
                                                        : consecutive_register_failures]);
        }
      }

      /* Quando offline, task_buttons mantém cliques em pending_clicks,
       * então não precisa drenar a fila aqui */
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    /* Processamento de Cliques Pendentes — ciclo de 20ms (US-38)
     * Consome diretamente do shared_state; task_buttons não usa a fila
     * no modo online, eliminando a corrida entre os dois consumidores.
     * IMPORTANTE: só consome quando status=ONLINE. Caso contrário os cliques
     * ficam em pending_clicks e serão sincronizados via sync_offline no
     * próximo registro. */
    uint32_t pending_clicks = 0;
    connection_status_t current_status = shared_state_get_connection_status();
    if (current_status == STATUS_ONLINE) {
      pending_clicks = shared_state_take_pending_clicks();
    }

    if (pending_clicks > 0) {
      /* Envia cliques para o servidor */
      uint32_t lamport_sent = lamport_tick();
      RpcClickResult click_res = rpc_add_clicks((int)pending_clicks, lamport_sent);

      if (click_res.success) {
        LOG_VERBOSE("[LAMPORT]", "sent=%lu recv=%lu monotonic=%s", (unsigned long)lamport_sent,
                    (unsigned long)click_res.lamport_ts,
                    click_res.lamport_ts > lamport_sent ? "OK" : "VIOLATION");

        /* Reset do contador de falhas em caso de sucesso */
        consecutive_rpc_failures = 0;

        lamport_update((uint32_t)click_res.lamport_ts);
        shared_state_set_scores(&click_res);

        if (click_res.milestone_triggered) {
          shared_state_set_led_flash_requested(true);
          printf("[MILESTONE] Marco atingido: %d\n", click_res.milestone_value);
        }

        if (click_res.powerup_remaining_s > 0) {
          apply_local_turbo((uint32_t)click_res.powerup_remaining_s * 1000u);
          printf("[TURBO] Power-up do servidor: %d s restantes\n", click_res.powerup_remaining_s);
        }
      } else {
        /* Tratamento de Erros do Servidor */
        if (click_res.error_code == RPC_RATE_EXCEEDED) {
          /* Rate limit: servidor aceitou apenas parte dos cliques.
           * Atualiza scores com os cliques aceitos e restaura os rejeitados. */
          if (click_res.accepted_clicks > 0) {
            lamport_update((uint32_t)click_res.lamport_ts);
            shared_state_set_scores(&click_res);
          }
          uint32_t rejected = pending_clicks - (uint32_t)click_res.accepted_clicks;
          if (rejected > 0) {
            shared_state_restore_clicks(rejected);
          }
          consecutive_rpc_failures = 0; /* Rate limit não é falha de rede */
        } else {
          shared_state_restore_clicks(pending_clicks);
          if (click_res.error_code == RPC_LAMPORT_VIOLATION) {
            lamport_update((uint32_t)click_res.lamport_ts);
            /* Violação de Lamport não conta como falha de rede */
          } else {
            /* Falha de rede (timeout, desconexão, parse error, etc). */
            consecutive_rpc_failures++;
            LOG_NORMAL("[RPC]", "Falha RPC #%d/3", consecutive_rpc_failures);
          }
        }
      }
    }

    /* Heartbeat periódico: reenvia rpc_register_node a cada 30 s para manter
     * o nó ACTIVE no NodeRegistry. Verificado a cada ciclo de 20 ms via
     * time_us_64() — sem timer de hardware separado. */
    if (shared_state_get_connection_status() == STATUS_ONLINE) {
      uint64_t hb_now = time_us_64();
      if ((hb_now - last_heartbeat_us) >= HEARTBEAT_INTERVAL_US) {
        RpcSimpleResult hb_result = rpc_register_node((uint8_t)NODE_ID);
        if (hb_result.success) {
          last_heartbeat_us = time_us_64();
          LOG_NORMAL("[HEARTBEAT]", "HEARTBEAT SENT node_id=%d", NODE_ID);
        } else {
          consecutive_rpc_failures++;
          LOG_NORMAL("[HEARTBEAT]", "HEARTBEAT FAILED consecutivas=%d", consecutive_rpc_failures);
        }
      }
    }

    /* Avalia transição para OFFLINE independentemente de pending_clicks.
     * Garante que falhas de heartbeat (sem cliques) também transitam o nó
     * para OFFLINE após 3 falhas consecutivas. */
    if (consecutive_rpc_failures >= 3) {
      LOG_NORMAL("[RPC]",
                 "3 falhas consecutivas — transicao OFFLINE "
                 "pending_clicks=%lu",
                 (unsigned long)shared_state_get_pending_clicks());
      shared_state_set_connection_status(STATUS_OFFLINE);
      consecutive_rpc_failures = 0;
      registered = false;
    }

    /* Atualização Periódica do Placar Global.
     * Executado ANTES do cálculo de elapsed para que sua latência seja
     * contabilizada no orçamento de 20ms da compensação de tempo. */
    TickType_t now = xTaskGetTickCount();
    if ((now - last_scores_refresh) >= pdMS_TO_TICKS(SCORES_REFRESH_MS)) {
      last_scores_refresh = now;
      RpcScoreResult scores = rpc_get_scores();
      if (scores.success) {
        uint32_t node_scores[MAX_NODES] = {0};
        for (uint8_t i = 0; i < MAX_NODES; i++) {
          node_scores[i] = (scores.node_scores[i] >= 0) ? (uint32_t)scores.node_scores[i] : 0;
        }
        uint32_t global = (scores.global_score >= 0) ? (uint32_t)scores.global_score : 0;
        /* Só sobrescreve global_score se o servidor retornou valor > 0.
         * Evita zerar o display quando o nó está idle e o parse falha ou
         * o servidor retorna 0 transitoriamente. */
        if (global > 0) {
          shared_state_set_global_score(global);
        }
        shared_state_set_node_scores(node_scores, MAX_NODES);

        /* Só sobrescreve local_score se o servidor retornou valor > 0.
         * Evita zerar o display quando o nó ainda não aparece no backend. */
        uint32_t received_local = node_scores[NODE_ID % MAX_NODES];
        if (received_local > 0) {
          shared_state_set_local_score(received_local);
        }

        shared_state_set_connection_status(STATUS_ONLINE);
        shared_state_set_server_error_active(false);
      } else {
        printf("[RPC] Falha ao atualizar scores (ignorando)\n");
      }
    }

    /* Compensação de tempo: dorme apenas o restante do período de 20ms. */
    uint64_t elapsed = time_us_64() - cycle_start;
    LOG_VERBOSE("[RPC]", "ciclo_us=%llu pending=%lu", (unsigned long long)elapsed,
                (unsigned long)pending_clicks);
    if (elapsed < CYCLE_PERIOD_US) {
      uint32_t sleep_us = (uint32_t)(CYCLE_PERIOD_US - elapsed);
      TickType_t sleep_ticks = pdMS_TO_TICKS(sleep_us / 1000u);
      if (sleep_ticks > 0) {
        vTaskDelay(sleep_ticks);
      } else {
        taskYIELD();
      }
    } else {
      taskYIELD();
    }
  }
}
