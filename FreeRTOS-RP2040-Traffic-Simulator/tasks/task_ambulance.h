/* === Task da ambulancia === */

#ifndef TASK_AMBULANCE_H
#define TASK_AMBULANCE_H

#include "simulator/traffic_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t y;
  uint8_t x;
  Direcao dir;
  uint8_t velocidade;
} AmbulanceParams;

bool task_ambulance_spawn(AmbulanceParams params);
void task_ambulance_run(void *params);

#endif
