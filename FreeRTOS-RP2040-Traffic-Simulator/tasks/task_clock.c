/* === Task de relogio === */

#include "tasks/task_clock.h"

#include "middleware/app_queues.h"
#include "middleware/lamport.h"
#include "middleware/rpc_types.h"
#include "middleware/shared_state.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "debug_log.h"
#include "hardware_config.h"

#include <string.h>

EventGroupHandle_t g_tick_event = NULL;

static volatile uint32_t g_tick_counter = 0;
static volatile uint32_t g_tick_period_ms = TICK_MS;

void task_clock_init(void) {
  if (g_tick_event == NULL) {
    g_tick_event = xEventGroupCreate();
    configASSERT(g_tick_event != NULL);
  }
  g_tick_counter = 0;
  g_tick_period_ms = TICK_MS;
  shared_state_set_tick_count(0);
}

uint32_t task_clock_get_tick(void) {
  return g_tick_counter;
}

uint32_t task_clock_get_period_ms(void) {
  return g_tick_period_ms;
}

bool task_clock_set_period_ms(uint32_t period_ms) {
  if (period_ms < TICK_MS_MIN || period_ms > TICK_MS_MAX) {
    return false;
  }
  g_tick_period_ms = period_ms;
  return true;
}

bool task_clock_speed_up(void) {
  const uint32_t current = g_tick_period_ms;
  if (current <= TICK_MS_MIN) {
    return false;
  }
  return task_clock_set_period_ms(current - TICK_MS_STEP);
}

bool task_clock_speed_down(void) {
  const uint32_t current = g_tick_period_ms;
  if (current >= TICK_MS_MAX) {
    return false;
  }
  return task_clock_set_period_ms(current + TICK_MS_STEP);
}

void task_clock_wait_tick(void) {
  configASSERT(g_tick_event != NULL);
  (void)xEventGroupWaitBits(g_tick_event, TICK_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
}

void task_clock_sync_remote(uint32_t remote_tick, uint32_t remote_lamport) {
  lamport_update_from_remote(remote_lamport);
  g_tick_counter = remote_tick;
  shared_state_set_tick_count(remote_tick);
  (void)xEventGroupSetBits(g_tick_event, TICK_EVENT_BIT);
}

static void send_sync_tick_message(uint32_t tick_value, uint32_t lamport_value) {
  RpcMessage msg;
  QueueHandle_t q;

  q = app_queues_get_rpc_out();
  if (q == NULL) {
    return;
  }

  msg.type = MSG_SYNC_TICK;
  msg.src_node = (uint8_t)NODE_ID;
  msg.dst_node = 0xFFu;
  msg.payload_len = (uint8_t)(2u * sizeof(uint32_t));
  memcpy(msg.payload, &tick_value, sizeof(uint32_t));
  memcpy(msg.payload + sizeof(uint32_t), &lamport_value, sizeof(uint32_t));

  if (xQueueSend(q, &msg, 0) != pdTRUE) {
    LOG_VERBOSE("[CLOCK]", "fila RPC cheia para tick %lu", (unsigned long)tick_value);
  }
}

void task_clock_run(void *params) {
  sim_snapshot_t snap;
  (void)params;

  configASSERT(g_tick_event != NULL);

  while (1) {
    const uint32_t period_ms = g_tick_period_ms;
    vTaskDelay(pdMS_TO_TICKS(period_ms));

    shared_state_get_sim_snapshot(&snap);
    if (snap.paused) {
      continue;
    }

    g_tick_counter++;
    shared_state_set_tick_count(g_tick_counter);

#if NODE_ID == 1
    if (shared_state_get_connection_status() == STATUS_ONLINE) {
      uint32_t local_lamport = lamport_tick();
      send_sync_tick_message(g_tick_counter, local_lamport);
    }
#endif

    (void)xEventGroupSetBits(g_tick_event, TICK_EVENT_BIT);
  }
}

void task_clock(void *params) {
  task_clock_run(params);
}
