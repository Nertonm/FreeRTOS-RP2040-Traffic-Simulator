/* === Task de semaforos === */

#ifndef TASK_SEMAPHORE_H
#define TASK_SEMAPHORE_H

#include "FreeRTOS.h"
#include "event_groups.h"

#include <stdbool.h>
#include <stdint.h>

#include "simulator/traffic_types.h"

#define SEM_GREEN_BIT ((EventBits_t)(1u << 0))
#define SEM_RED_BIT ((EventBits_t)(1u << 1))

extern EventGroupHandle_t semaphore_events[MAX_LIGHTS_PER_REGION];

void task_semaphore_init(void);
void semaphore_wait_green(uint8_t light_id);
bool semaphore_wait_green_timeout(uint8_t light_id, TickType_t timeout_ticks);
void semaphore_force_green(uint8_t light_id);
void task_semaphore_run(void *params);

/* Compatibilidade com nome antigo. */
void task_semaphore(void *params);

#endif
