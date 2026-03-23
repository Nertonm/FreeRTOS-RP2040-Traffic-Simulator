/**
 * @file debug_log.h
 * @brief Macros centralizadas para logging estruturado, asserts defensivos
 *        e contadores de diagnóstico do firmware Clicker RP2040.
 *
 * ============================================================
 * NÍVEIS DE VERBOSIDADE (definir antes de incluir, ou em CMakeLists):
 *
 *   DEBUG_LEVEL_NONE    0  — Apenas panics/asserts fatais
 *   DEBUG_LEVEL_ERROR   1  — Erros detectados ([ERROR])
 *   DEBUG_LEVEL_NORMAL  2  — Transições de estado críticas (padrão)
 *   DEBUG_LEVEL_VERBOSE 3  — Operações relevantes por módulo
 *   DEBUG_LEVEL_TRACE   4  — Tudo, incluindo getters/setters e payloads
 *
 * Para ativar via compile flag:
 *   cmake -DDEBUG_LEVEL=3 ..
 *   ou em CMakeLists.txt:
 *   target_compile_definitions(firmware PRIVATE DEBUG_LEVEL=3)
 * ============================================================
 *
 * FORMATO DE SAÍDA DOS LOGS:
 *   [TAG] T+<ms> <mensagem>  campo1=val1 campo2=val2
 *
 * EXEMPLOS DE SAÍDA:
 *   [LAMPORT] T+1234 Tick: antes=5 depois=6
 *   [STATE]   T+2000 STATUS_ONLINE -> STATUS_OFFLINE razão=3_falhas_consecutivas
 *   [RPC]     T+3050 add_clicks: node=0 clicks=5 lamport=12 ok=true
 *   [ERROR]   T+4000 JSON parse falhou: campo=global_score raw="{...}"
 */

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"

/* ============================================================
 * Constantes de nível de debug
 * ============================================================ */
#define DEBUG_LEVEL_NONE 0
#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_NORMAL 2
#define DEBUG_LEVEL_VERBOSE 3
#define DEBUG_LEVEL_TRACE 4

/* Define o nível padrão se não foi definido em tempo de compilação */
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_NORMAL
#endif

/* ============================================================
 * Timestamp helper — milissegundos desde o boot
 * ============================================================ */
#define DBG_NOW_MS() ((unsigned long)to_ms_since_boot(get_absolute_time()))

/* ============================================================
 * Macros de logging por nível
 *
 * Uso:
 *   LOG_ERROR("[ERROR]", "socket falhou errno=%d", errno);
 *   LOG_NORMAL("[STATE]", "STATUS_ONLINE->STATUS_OFFLINE razao=%s", "timeout");
 *   LOG_VERBOSE("[RPC]", "parsed: global=%d local=%d", g, l);
 *   LOG_TRACE("[LAMPORT]", "get_current ts=%lu", (unsigned long)ts);
 * ============================================================ */

#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
#define LOG_ERROR(tag, fmt, ...) printf(tag " T+%lu " fmt "\n", DBG_NOW_MS(), ##__VA_ARGS__)
#else
#define LOG_ERROR(tag, fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_NORMAL
#define LOG_NORMAL(tag, fmt, ...) printf(tag " T+%lu " fmt "\n", DBG_NOW_MS(), ##__VA_ARGS__)
#else
#define LOG_NORMAL(tag, fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_VERBOSE
#define LOG_VERBOSE(tag, fmt, ...) printf(tag " T+%lu " fmt "\n", DBG_NOW_MS(), ##__VA_ARGS__)
#else
#define LOG_VERBOSE(tag, fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_TRACE
#define LOG_TRACE(tag, fmt, ...) printf(tag " T+%lu " fmt "\n", DBG_NOW_MS(), ##__VA_ARGS__)
#else
#define LOG_TRACE(tag, fmt, ...) ((void)0)
#endif

/* ============================================================
 * Assert defensivo com log antes de travar
 *
 * Uso:
 *   DIAG_ASSERT(node_id < MAX_NODES, "[ERROR]", "node_id out of range node_id=%d", node_id);
 *
 * Comportamento:
 *   - Sempre loga o erro (independente de DEBUG_LEVEL)
 *   - Chama configASSERT(0) que aciona o panic handler do FreeRTOS
 * ============================================================ */
#define DIAG_ASSERT(cond, tag, fmt, ...)                                                           \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      printf(tag " T+%lu ASSERT FAIL: " fmt "\n", DBG_NOW_MS(), ##__VA_ARGS__);                    \
      configASSERT(0);                                                                             \
    }                                                                                              \
  } while (0)

/* ============================================================
 * Verificação de invariante (não fatal) — apenas log de warning
 *
 * Uso:
 *   DIAG_CHECK(score <= MAX_SCORE, "[ERROR]", "score inválido score=%lu", score);
 * ============================================================ */
#define DIAG_CHECK(cond, tag, fmt, ...)                                                            \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      printf(tag " T+%lu CHECK FAIL (non-fatal): " fmt "\n", DBG_NOW_MS(), ##__VA_ARGS__);         \
    }                                                                                              \
  } while (0)

/* ============================================================
 * Contadores de diagnóstico — simples variáveis globais atômicas
 *
 * Para usar: declare extern diag_counters_t g_diag em seu módulo,
 * ou use a estrutura global definida em debug_log.c (se existir).
 *
 * Exemplo simples — declare localmente no módulo:
 *   static uint32_t diag_rpc_total = 0, diag_rpc_ok = 0, diag_rpc_fail = 0;
 *   DIAG_CNT_INC(diag_rpc_total);
 *   DIAG_CNT_INC(diag_rpc_ok);
 * ============================================================ */
#define DIAG_CNT_INC(counter)                                                                      \
  do {                                                                                             \
    (counter)++;                                                                                   \
  } while (0)
#define DIAG_CNT_GET(counter) ((uint32_t)(counter))

/* ============================================================
 * Macro utilitária: loga transição de estado
 *
 * Uso:
 *   LOG_STATE_TRANSITION("[STATE]", "connection", "ONLINE", "OFFLINE", "3_falhas");
 * ============================================================ */
#define LOG_STATE_TRANSITION(tag, ctx, from, to, reason)                                           \
  LOG_NORMAL(tag, "TRANSIÇÃO %s: %s -> %s razao=%s", ctx, from, to, reason)

/* ============================================================
 * Helper: nome legível para connection_status_t
 * Coloque em um .c ou inline conforme necessidade
 * ============================================================ */
static inline const char *conn_status_name(int status) {
  switch (status) {
  case 0:
    return "CONNECTING";
  case 1:
    return "ONLINE";
  case 2:
    return "OFFLINE";
  case 3:
    return "SYNCING";
  default:
    return "UNKNOWN";
  }
}

/*
 * Flags opcionais de subsistema (podem ser desligadas individualmente
 * mesmo com DEBUG_LEVEL alto, para reduzir ruído em módulos específicos):
 *
 *   -DDEBUG_SHARED_STATE=0  desliga logs de shared_state
 *   -DDEBUG_LAMPORT=0       desliga logs de lamport
 *   -DDEBUG_RPC=0           desliga logs de rpc_client
 *   -DDEBUG_BTN=0           desliga logs de button_handler
 *
 * Por padrão todos estão habilitados quando DEBUG_LEVEL >= NORMAL.
 */
#ifndef DEBUG_SHARED_STATE
#define DEBUG_SHARED_STATE 1
#endif
#ifndef DEBUG_LAMPORT
#define DEBUG_LAMPORT 1
#endif
#ifndef DEBUG_RPC
#define DEBUG_RPC 1
#endif
#ifndef DEBUG_BTN
#define DEBUG_BTN 1
#endif

#endif /* DEBUG_LOG_H */
