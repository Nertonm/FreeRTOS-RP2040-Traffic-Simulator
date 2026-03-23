/**
 * @file shared_state.h
 * @brief Gerenciamento de estado compartilhado entre núcleos (Core 0 e Core 1).
 *
 * Provê uma interface thread-safe para acesso a dados globais: status de
 * conexão, estado do simulador (grid, semáforos) e flags de eventos,
 * utilizando spinlocks de hardware do RP2040.
 *
 * @date 2026-03-01
 */

#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "middleware/rpc_types.h"
#include "simulator/traffic_types.h" /* GRID_ROWS, GRID_COLS, MAX_LIGHTS */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief true enquanto uma ambulância estiver ativa no nó local.
 * Escrita por task_ambulance, lida por task_display (sem lock).
 */
extern volatile bool g_ambulance_active;

/**
 * @brief Número máximo de nós suportados na rede.
 */
#define MAX_NODES 3

/**
 * @brief Estados possíveis da conexão do firmware com o servidor.
 */
typedef enum {
  STATUS_CONNECTING, /**< Tentando estabelecer conexão inicial. */
  STATUS_ONLINE,     /**< Conectado e sincronizado com o servidor. */
  STATUS_OFFLINE,    /**< Sem conexão ou erro de rede. */
  STATUS_SYNCING     /**< Em processo de sincronização de dados pendentes. */
} connection_status_t;

/**
 * @brief Inicializa o estado compartilhado e o spinlock de proteção.
 *
 * @note Deve ser chamado pelo Core 0 antes de iniciar o Core 1 ou qualquer
 * tarefa FreeRTOS.
 */
void shared_state_init(void);

/* --- Funções de Acesso (Thread-safe via Spinlock) --- */

/**
 * @brief Obtém a quantidade de cliques pendentes de envio.
 * @return uint32_t Número de cliques acumulados localmente.
 */
uint32_t shared_state_get_pending_clicks(void);

/**
 * @brief Incrementa atomicamente o contador de cliques pendentes.
 * Chamado normalmente em resposta a interrupções de hardware (botões).
 */
void shared_state_increment_pending_clicks(void);

/**
 * @brief Lê e zera atomicamente o contador de cliques pendentes.
 *
 * @return uint32_t Número de cliques consumidos para processamento.
 */
uint32_t shared_state_take_pending_clicks(void);

/**
 * @brief Define a quantidade de cliques em processo de sincronização.
 *
 * Usado para exibir no OLED durante STATUS_SYNCING. Este campo preserva
 * o snapshot de cliques pendentes enquanto a task de RPC realiza a chamada
 * sync_offline, permitindo que a task de display mostre o valor correto.
 *
 * @param[in] count Número de cliques sendo sincronizados.
 */
void shared_state_set_syncing_count(uint32_t count);

/**
 * @brief Obtém a quantidade de cliques em sincronização.
 * @return uint32_t Contador de cliques pendentes de sync.
 */
uint32_t shared_state_get_syncing_count(void);

/**
 * @brief Restaura uma quantidade de cliques ao contador pendente.
 *
 * Utilizado para restaurar cliques em caso de falha de envio para o servidor.
 * @param[in] n Quantidade de cliques a serem restaurados.
 */
void shared_state_restore_clicks(uint32_t n);

/**
 * @brief Acumula cliques no contador de modo offline.
 *
 * Usado para manter feedback visual quando offline. O contador é somado
 * ao local_score para exibição na matriz de LEDs.
 * @param[in] n Quantidade de cliques a acumular.
 */
void shared_state_add_offline_clicks(uint32_t n);

/**
 * @brief Obtém a quantidade de cliques acumulados offline.
 * @return uint32_t Total de cliques desde a última sincronização.
 */
uint32_t shared_state_get_offline_clicks(void);

/**
 * @brief Zera o contador de cliques offline.
 *
 * Chamado após sincronização bem-sucedida com o servidor.
 */
void shared_state_clear_offline_clicks(void);

/**
 * @brief Obtém a pontuação (score) local do dispositivo.
 * @return uint32_t Pontuação local.
 */
uint32_t shared_state_get_local_score(void);

/**
 * @brief Define a pontuação local.
 * @param[in] score Novo valor da pontuação.
 * @note Apenas Core 0 deve realizar a escrita.
 */
void shared_state_set_local_score(uint32_t score);

/**
 * @brief Obtém a pontuação global acumulada (todos os nós).
 * @return uint32_t Pontuação global.
 */
uint32_t shared_state_get_global_score(void);

/**
 * @brief Define a pontuação global.
 * @param[in] score Novo valor da pontuação global.
 * @note Apenas Core 0 deve realizar a escrita.
 */
void shared_state_set_global_score(uint32_t score);

/**
 * @brief Obtém as pontuações individuais de todos os nós conhecidos.
 *
 * @param[out] out_scores Ponteiro para array que receberá os scores.
 * @param[in] count Quantidade máxima de elementos a ler.
 */
void shared_state_get_node_scores(uint32_t *out_scores, uint8_t count);

/**
 * @brief Atualiza os scores de múltiplos nós simultaneamente.
 *
 * @param[in] in_scores Ponteiro para array com os novos scores.
 * @param[in] count Quantidade de elementos no array.
 * @note Apenas Core 0 deve realizar a escrita.
 */
void shared_state_set_node_scores(const uint32_t *in_scores, uint8_t count);

/**
 * @brief Atualiza atomicamente todos os campos de estado derivados de um
 *        resultado de clique bem-sucedido.
 *
 * Consolida a atualização de local_score, global_score e milestone_triggered
 * sob um único acquire/release do spinlock, eliminando janelas de
 * inconsistência entre atualizações individuais.
 *
 * @param[in] result Ponteiro para o resultado RPC. Deve ser não-nulo e válido
 *                   (result->success == true garantido pelo chamador).
 */
void shared_state_set_scores(const RpcClickResult *result);

/**
 * @brief Snapshot para renderização do display do clicker (legado).
 *
 * Mantida para compatibilidade. Para o simulador, usar sim_snapshot_t.
 */
typedef struct {
  uint32_t local_score;
  uint32_t global_score;
  uint32_t pending_clicks;
  uint32_t syncing_count;
  uint32_t offline_clicks;
  connection_status_t status;
  bool turbo_active;
  uint32_t turbo_until_ms;
} display_snapshot_t;

void shared_state_get_display_snapshot(display_snapshot_t *snapshot);

/* ============================================================================
 * Snapshot do simulador de tráfego
 * ============================================================================ */

/**
 * @brief Snapshot atômico completo para renderização pelo task_display.
 *
 * Lido por task_display a cada ciclo de renderização. Escrito pelas tasks
 * de carro, semáforo, clock e RPC sob o spinlock do shared_state.
 */
typedef struct {
  /* Grade local — qual carro ocupa cada célula (0 = vazio, id+1 = carro N) */
  uint8_t car_grid[GRID_ROWS][GRID_COLS];

  /* Estado de cada semáforo: 0 = vermelho, 1 = verde */
  uint8_t semaphore_state[MAX_LIGHTS];

  /* Flags de estado global */
  bool ambulance_active; /**< true se ambulância está ativa no nó. */
  bool paused;           /**< true se a simulação está pausada. */
  bool tcp_active;       /**< true se há transferência TCP em curso. */

  /* Relógio e contadores */
  uint32_t tick_count; /**< Tick global atual do simulador. */
  uint8_t car_count;   /**< Número de carros ativos (0…MAX_CARS). */

  /* Identificação e rede */
  char wifi_status[8];  /**< "OK", "OFF", "CONN", "SYNC" (null-term). */
  char region_name[10]; /**< "Norte", "Centro", "Sul" (null-term). */
} sim_snapshot_t;

/**
 * @brief Lê atomicamente o snapshot do simulador para renderização.
 * @param[out] snap  Destino do snapshot.
 */
void shared_state_get_sim_snapshot(sim_snapshot_t *snap);

/* --------------------------------------------------------------------------
 * Setters das novas variáveis do simulador
 * Todos thread-safe (spinlock interno).
 * -------------------------------------------------------------------------- */

/**
 * @brief Registra qual carro ocupa a célula (y, x).
 * @param y, x   Coordenadas no grid local.
 * @param car_id  0 para limp a célula; car_id + 1 para marcar presença.
 */
void shared_state_set_car_cell(uint8_t y, uint8_t x, uint8_t car_id);

/** @brief Atualiza o estado de um semáforo (0=vermelho, 1=verde). */
void shared_state_set_semaphore_state(uint8_t light_id, uint8_t state_val);

/** @brief Informa tick atual do simulador. */
void shared_state_set_tick_count(uint32_t tick);

/** @brief Informa número de carros ativos. */
void shared_state_set_car_count(uint8_t count);

/** @brief Informa estado da ambulância (espelhado de g_ambulance_active). */
void shared_state_set_ambulance_active(bool active);

/** @brief Ativa/desativa o flag de pausa da simulação. */
void shared_state_set_paused(bool paused);

/** @brief Informa se há transferência TCP ativa (para LED roxo). */
void shared_state_set_tcp_active(bool active);

/**
 * @brief Atualiza a string de status WiFi (máx 7 chars + null).
 * @param status  String como "OK", "OFF", "CONN".
 */
void shared_state_set_wifi_status(const char *status);

/**
 * @brief Obtém o status atual da conexão.
 * @return connection_status_t Estado atual (Online, Offline, etc).
 */
connection_status_t shared_state_get_connection_status(void);

/**
 * @brief Define o status da conexão.
 * @param[in] status Novo estado de conexão.
 */
void shared_state_set_connection_status(connection_status_t status);

/**
 * @brief Adquire manualmente o spinlock do shared_state.
 *
 * Utilizado por módulos externos (ex: lamport.c) que necessitam compartilhar a
 * mesma trava de exclusão mútua.
 * @param[out] save Ponteiro para armazenar o estado das interrupções (IRQ)
 * antes da trava.
 */
void shared_state_lock_enter(uint32_t *save);

/**
 * @brief Libera manualmente o spinlock do shared_state.
 * @param[in] save Valor retornado por shared_state_lock_enter para restaurar o
 * estado de IRQ.
 */
void shared_state_lock_exit(uint32_t save);

/**
 * @brief Verifica se um marco de pontuação (milestone) foi atingido.
 * @return bool Verdadeiro se atingido.
 */
bool shared_state_get_milestone_triggered(void);

/**
 * @brief Define se um marco de pontuação foi atingido.
 * @param[in] triggered Estado do marco.
 */
void shared_state_set_milestone_triggered(bool triggered);

/**
 * @brief Lê e limpa atomicamente a flag de milestone.
 * @return bool Estado da flag antes de ser limpa.
 */
bool shared_state_take_milestone_triggered(void);

/**
 * @brief Verifica se houve pedido de flash do LED.
 * @return bool Verdadeiro se solicitado.
 */
bool shared_state_get_led_flash_requested(void);

/**
 * @brief Solicita um flash no LED.
 * @param[in] requested Verdadeiro para solicitar.
 */
void shared_state_set_led_flash_requested(bool requested);

/**
 * @brief Lê e limpa o pedido de flash do LED.
 * @return bool Verdadeiro se havia um pedido pendente.
 */
bool shared_state_take_led_flash_requested(void);

/**
 * @brief Verifica se o firmware está usando o IP de fallback.
 * @return bool Verdadeiro se em modo fallback.
 */
bool shared_state_get_fallback_in_use(void);

/**
 * @brief Define o uso do IP de fallback.
 * @param[in] in_use Estado do fallback.
 */
void shared_state_set_fallback_in_use(bool in_use);

/**
 * @brief Verifica se há erro ativo no servidor.
 * @return bool Verdadeiro se houver erro ativo.
 */
bool shared_state_get_server_error_active(void);

/**
 * @brief Define estado de erro no servidor.
 * @param[in] active Verdadeiro para indicar erro.
 */
void shared_state_set_server_error_active(bool active);

/**
 * @brief Solicita ativação do modo Turbo/Power-up.
 */
void shared_state_request_turbo_activation(void);

/**
 * @brief Lê e limpa o pedido de ativação de turbo.
 * @return bool Verdadeiro se solicitado.
 */
bool shared_state_take_turbo_activation_requested(void);

/**
 * @brief Verifica se o modo Turbo está ativo no momento.
 * @return bool Verdadeiro se ativo.
 */
bool shared_state_get_turbo_active(void);

/**
 * @brief Define se o modo Turbo está ativo.
 * @param[in] active Estado do turbo.
 */
void shared_state_set_turbo_active(bool active);

/**
 * @brief Obtém o tempo de expiração do modo Turbo (em ms).
 * @return uint32_t Timestamp (uptime_ms) de expiração.
 */
uint32_t shared_state_get_turbo_until_ms(void);

/**
 * @brief Define o tempo de expiração do modo Turbo.
 * @param[in] until_ms Timestamp (uptime_ms) de expiração.
 */
void shared_state_set_turbo_until_ms(uint32_t until_ms);

#endif // SHARED_STATE_H