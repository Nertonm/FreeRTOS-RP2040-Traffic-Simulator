/**
 * @file firmware_config.h
 * @brief Parâmetros de configuração de comportamento do firmware.
 *
 * Contém definições de tempos de polling, tamanhos de fila e durações
 * de efeitos visuais/lógicos do jogo.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef FIRMWARE_CONFIG_H
#define FIRMWARE_CONFIG_H

/** @name Configurações de Tarefas */
/** @{ */
#define CLICK_QUEUE_LEN 64     /**< Profundidade máxima da fila de cliques. */
#define DISPLAY_PERIOD_MS 50   /**< Intervalo de atualização da interface visual (ms). */
#define MONITOR_PERIOD_MS 5000 /**< Intervalo de log de telemetria (ms). */
#define RPC_POLL_PERIOD_MS                                                                         \
  100                                     /**< Latência de resposta da tarefa RPC (ms).           \
                                           */
#define CYCLE_PERIOD_US 20000             /**< Período exato do ciclo online da task_rpc (µs). */
#define SCORES_REFRESH_MS 2000            /**< Intervalo de busca de scores globais (ms). */
#define HEARTBEAT_INTERVAL_US 30000000ULL /**< Intervalo de heartbeat ao servidor (30 s em µs). */

/** @brief Define se o backend TCP JSON-RPC deve ser usado (1) ou desabilitado (0). */
#define ENABLE_BACKEND_RPC 0

/** @brief Backoff exponencial para retry de registro (ms).
 *  Valores: 1s, 2s, 5s, 10s (máximo). */
#define RPC_REGISTER_BACKOFF_1 1000
#define RPC_REGISTER_BACKOFF_2 2000
#define RPC_REGISTER_BACKOFF_3 5000
#define RPC_REGISTER_BACKOFF_MAX 10000

/** @brief Intervalo de retry de conexão WiFi (ms). */
#define WIFI_RETRY_INTERVAL_MS 30000

/** @brief Porta UDP para troca direta de mensagens entre placas (simulador). */
#define INTERBOARD_UDP_PORT 5055
/** @} */

/** @name Regras de Jogo */
/** @{ */
#define TURBO_DURATION_MS 10000 /**< Duração do efeito Power-up (ms). */
#define MILESTONE_GLOW_TICKS                                                                       \
  10 /**< Duração do efeito visual de milestone (em ciclos de display). */
/** @} */

#endif /* FIRMWARE_CONFIG_H */
