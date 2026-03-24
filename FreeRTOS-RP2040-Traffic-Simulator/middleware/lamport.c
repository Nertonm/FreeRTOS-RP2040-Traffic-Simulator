#include "lamport.h"
#include "middleware/shared_state.h"
#include "debug_log.h"
#include <stdint.h>
#include <stdio.h>

/* --- Contadores de diagnóstico do relógio de Lamport --- */
static uint32_t diag_tick_count = 0;
static uint32_t diag_update_count = 0;
static uint32_t diag_jump_alerts = 0; /* Saltos > 1000 unidades detectados */
static uint32_t diag_resets = 0;      /* Resets por overflow */

#define LAMPORT_JUMP_THRESHOLD 1000u /* Salto considerado anormal */

static uint32_t lamport_L = 0;

/* Limite de segurança para evitar overflow (reserva margem para operações) */
#define LAMPORT_MAX_SAFE (UINT32_MAX - 1000)

void lamport_init(void) {
  /* shared_state_init() deve ter sido chamado antes */
  uint32_t save;
  shared_state_lock_enter(&save);
  lamport_L = 0;
  shared_state_lock_exit(save);
  diag_tick_count = 0;
  diag_update_count = 0;
  diag_jump_alerts = 0;
  diag_resets = 0;
  LOG_NORMAL("[LAMPORT]", "Inicializado ts=0");
}

uint32_t lamport_tick(void) {
  uint32_t save;
  shared_state_lock_enter(&save);
  uint32_t before = lamport_L;

  /* Verificação de overflow com saturação */
  if (lamport_L >= LAMPORT_MAX_SAFE) {
    lamport_L = 1;
    uint32_t current = lamport_L;
    shared_state_lock_exit(save);
    DIAG_CNT_INC(diag_resets);
    LOG_ERROR("[LAMPORT]", "OVERFLOW RESET no tick antes=%lu resets=%lu", (unsigned long)before,
              (unsigned long)diag_resets);
    DIAG_CNT_INC(diag_tick_count);
    return current;
  } else {
    lamport_L += 1u;
  }
  uint32_t current = lamport_L;
  shared_state_lock_exit(save);

  /* Invariante: relógio deve ser estritamente crescente após tick */
  DIAG_ASSERT(current > before, "[LAMPORT]", "Monotonicidade violada no tick: antes=%lu depois=%lu",
              (unsigned long)before, (unsigned long)current);

  DIAG_CNT_INC(diag_tick_count);
  LOG_VERBOSE("[LAMPORT]", "Tick: antes=%lu depois=%lu (ticks=%lu)", (unsigned long)before,
              (unsigned long)current, (unsigned long)diag_tick_count);
  return current;
}

void lamport_update(uint32_t received_ts) {
  uint32_t save;
  shared_state_lock_enter(&save);
  uint32_t before = lamport_L;

  /* Detecção de salto anormal (remoto muito maior que local) */
  bool big_jump = (received_ts > before) && (received_ts - before > LAMPORT_JUMP_THRESHOLD);

  /* Atualiza para o maior valor, com verificação de overflow */
  if (received_ts > lamport_L && received_ts < LAMPORT_MAX_SAFE) {
    lamport_L = received_ts;
  }

  bool reset = false;
  if (lamport_L >= LAMPORT_MAX_SAFE) {
    lamport_L = 1;
    reset = true;
  } else {
    lamport_L += 1u;
  }
  uint32_t current = lamport_L;
  shared_state_lock_exit(save);

  DIAG_CNT_INC(diag_update_count);

  if (reset) {
    DIAG_CNT_INC(diag_resets);
    LOG_ERROR("[LAMPORT]", "OVERFLOW RESET no update antes=%lu resets=%lu", (unsigned long)before,
              (unsigned long)diag_resets);
  } else if (big_jump) {
    DIAG_CNT_INC(diag_jump_alerts);
    LOG_ERROR("[LAMPORT]", "SALTO ANORMAL local=%lu recebido=%lu delta=%lu alertas=%lu",
              (unsigned long)before, (unsigned long)received_ts,
              (unsigned long)(received_ts - before), (unsigned long)diag_jump_alerts);
  } else {
    LOG_VERBOSE("[LAMPORT]", "Update: local=%lu recebido=%lu depois=%lu updates=%lu",
                (unsigned long)before, (unsigned long)received_ts, (unsigned long)current,
                (unsigned long)diag_update_count);
  }

  /* Invariante: relógio sempre aumenta após update (exceto reset) */
  if (!reset) {
    DIAG_ASSERT(current > before, "[LAMPORT]",
                "Monotonicidade violada no update: antes=%lu depois=%lu recebido=%lu",
                (unsigned long)before, (unsigned long)current, (unsigned long)received_ts);
  }
}

void lamport_update_from_remote(uint32_t remote_ts) {
  lamport_update(remote_ts);
}

uint32_t lamport_get_current(void) {
  uint32_t save;
  shared_state_lock_enter(&save);
  uint32_t current = lamport_L;
  shared_state_lock_exit(save);
  return current;
}

void lamport_print_diagnostics(void) {
  uint32_t ts = lamport_get_current();
  LOG_NORMAL("[LAMPORT]", "[STATS] ts=%lu ticks=%lu updates=%lu jumps=%lu resets=%lu",
             (unsigned long)ts, (unsigned long)diag_tick_count, (unsigned long)diag_update_count,
             (unsigned long)diag_jump_alerts, (unsigned long)diag_resets);
}
