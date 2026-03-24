/* === Maquina de semaforos === */

#include "tasks/task_semaphore.h"

#include "middleware/shared_state.h"
#include "tasks/task_clock.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug_log.h"

EventGroupHandle_t semaphore_events[MAX_LIGHTS_PER_REGION];

/* DESIGN: ciclo coordenado por grupos (pares/impares) evita comutacao simultanea
 * de todos os cruzamentos e melhora o fluxo local. */
static uint8_t g_is_green[MAX_LIGHTS_PER_REGION];
static uint8_t g_force_green_hold[MAX_LIGHTS_PER_REGION];

#define SEM_GREEN_TICKS 6u
#define SEM_ALL_RED_TICKS 1u
#define SEM_FORCE_GREEN_HOLD_TICKS 3u

typedef enum {
  SEM_PHASE_GROUP_A_GREEN = 0,
  SEM_PHASE_ALL_RED_A_TO_B,
  SEM_PHASE_GROUP_B_GREEN,
  SEM_PHASE_ALL_RED_B_TO_A
} semaphore_phase_t;

static semaphore_phase_t g_phase = SEM_PHASE_GROUP_A_GREEN;
static uint8_t g_phase_elapsed_ticks = 0;

static bool semaphore_group_a(uint8_t light_id) {
  return (light_id % 2u) == 0u;
}

static bool semaphore_phase_green_for_light(semaphore_phase_t phase, uint8_t light_id) {
  if (phase == SEM_PHASE_GROUP_A_GREEN) {
    return semaphore_group_a(light_id);
  }
  if (phase == SEM_PHASE_GROUP_B_GREEN) {
    return !semaphore_group_a(light_id);
  }
  return false;
}

static uint8_t semaphore_phase_duration(semaphore_phase_t phase) {
  if (phase == SEM_PHASE_GROUP_A_GREEN || phase == SEM_PHASE_GROUP_B_GREEN) {
    return SEM_GREEN_TICKS;
  }
  return SEM_ALL_RED_TICKS;
}

static semaphore_phase_t semaphore_next_phase(semaphore_phase_t phase) {
  switch (phase) {
  case SEM_PHASE_GROUP_A_GREEN:
    return SEM_PHASE_ALL_RED_A_TO_B;
  case SEM_PHASE_ALL_RED_A_TO_B:
    return SEM_PHASE_GROUP_B_GREEN;
  case SEM_PHASE_GROUP_B_GREEN:
    return SEM_PHASE_ALL_RED_B_TO_A;
  default:
    return SEM_PHASE_GROUP_A_GREEN;
  }
}

static void semaphore_apply_state(uint8_t light_id, bool green) {
  const uint8_t next = green ? 1u : 0u;
  if (g_is_green[light_id] == next) {
    return;
  }

  g_is_green[light_id] = next;
  if (green) {
    (void)xEventGroupSetBits(semaphore_events[light_id], SEM_GREEN_BIT);
    (void)xEventGroupClearBits(semaphore_events[light_id], SEM_RED_BIT);
  } else {
    (void)xEventGroupClearBits(semaphore_events[light_id], SEM_GREEN_BIT);
    (void)xEventGroupSetBits(semaphore_events[light_id], SEM_RED_BIT);
  }
  shared_state_set_semaphore_state(light_id, next);
}

void task_semaphore_init(void) {
  uint8_t i;
  g_phase = SEM_PHASE_GROUP_A_GREEN;
  g_phase_elapsed_ticks = 0;

  for (i = 0; i < MAX_LIGHTS_PER_REGION; i++) {
    semaphore_events[i] = xEventGroupCreate();
    configASSERT(semaphore_events[i] != NULL);

    g_is_green[i] = 0;
    g_force_green_hold[i] = 0;
    semaphore_apply_state(i, semaphore_group_a(i));
  }
}

void semaphore_wait_green(uint8_t light_id) {
  (void)semaphore_wait_green_timeout(light_id, portMAX_DELAY);
}

bool semaphore_wait_green_timeout(uint8_t light_id, TickType_t timeout_ticks) {
  if (light_id >= MAX_LIGHTS_PER_REGION) {
    return false;
  }

  EventBits_t bits = xEventGroupWaitBits(semaphore_events[light_id], SEM_GREEN_BIT, pdFALSE,
                                         pdFALSE, timeout_ticks);
  return (bits & SEM_GREEN_BIT) != 0;
}

void semaphore_force_green(uint8_t light_id) {
  if (light_id >= MAX_LIGHTS_PER_REGION) {
    return;
  }

  taskENTER_CRITICAL();
  g_force_green_hold[light_id] = SEM_FORCE_GREEN_HOLD_TICKS;
  taskEXIT_CRITICAL();
  semaphore_apply_state(light_id, true);

  LOG_NORMAL("[SEM]", "forca verde no semaforo %u", (unsigned)light_id);
}

void task_semaphore_run(void *params) {
  uint8_t i;
  (void)params;

  while (1) {
    task_clock_wait_tick();

    g_phase_elapsed_ticks++;
    if (g_phase_elapsed_ticks >= semaphore_phase_duration(g_phase)) {
      g_phase_elapsed_ticks = 0;
      g_phase = semaphore_next_phase(g_phase);
    }

    for (i = 0; i < MAX_LIGHTS_PER_REGION; i++) {
      taskENTER_CRITICAL();
      if (g_force_green_hold[i] > 0u) {
        g_force_green_hold[i]--;
        taskEXIT_CRITICAL();
        semaphore_apply_state(i, true);
      } else {
        taskEXIT_CRITICAL();
        semaphore_apply_state(i, semaphore_phase_green_for_light(g_phase, i));
      }
    }
  }
}

void task_semaphore(void *params) {
  task_semaphore_run(params);
}
