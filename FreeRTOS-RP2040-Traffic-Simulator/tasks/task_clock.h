/* === Task de relogio global === */

#ifndef TASK_CLOCK_H
#define TASK_CLOCK_H

#include "FreeRTOS.h"
#include "event_groups.h"

#include <stdbool.h>
#include <stdint.h>

#define TICK_MS 400u
#define TICK_EVENT_BIT ((EventBits_t)(1u << 0))
#define TICK_MS_MIN 80u
#define TICK_MS_MAX 600u
#define TICK_MS_STEP 20u

extern EventGroupHandle_t g_tick_event;

void task_clock_init(void);
void task_clock_run(void *params);
void task_clock_wait_tick(void);
uint32_t task_clock_get_tick(void);
void task_clock_sync_remote(uint32_t remote_tick, uint32_t remote_lamport);
uint32_t task_clock_get_period_ms(void);
bool task_clock_set_period_ms(uint32_t period_ms);
bool task_clock_speed_up(void);
bool task_clock_speed_down(void);

/* Compatibilidade com nomes antigos usados no codigo atual. */
void task_clock(void *params);

#endif
