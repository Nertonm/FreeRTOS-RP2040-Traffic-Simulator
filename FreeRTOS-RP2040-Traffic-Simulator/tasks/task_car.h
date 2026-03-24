/* === Task de carro === */

#ifndef TASK_CAR_H
#define TASK_CAR_H

#include "FreeRTOS.h"
#include "task.h"

#include "simulator/traffic_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t id;
  uint8_t y;
  uint8_t x;
  Direcao dir;
  uint8_t velocidade;
} CarParams;

typedef enum { CAR_STEP_WAIT = 0, CAR_STEP_MOVED, CAR_STEP_EXITED } CarStepResult;

void task_car_init(void);
bool task_car_spawn(CarParams params);
void task_car_run(void *params);

/* Funcoes comuns reutilizadas por task_ambulance. */
Direcao task_car_definir_direcao_cruzamento(uint8_t y, uint8_t x, Direcao dir_atual);
CarStepResult task_car_mover(CarState *car, bool respeitar_semaforo);

#endif
