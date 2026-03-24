/* === Logica da ambulancia === */

#include "tasks/task_ambulance.h"

#include "middleware/shared_state.h"
#include "simulator/grid.h"
#include "tasks/task_car.h"
#include "tasks/task_clock.h"
#include "tasks/task_semaphore.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug_log.h"

#define AMBULANCE_STACK_WORDS 768u
#define AMBULANCE_PRIORITY 4u
#define AMBULANCE_RADAR_CELLS 5u

static AmbulanceParams g_ambulance_params;
static bool g_ambulance_running = false;

/* DESIGN: task_ambulance reutiliza task_car_mover para evitar duplicacao da
 * maquina de movimento, alterando apenas semaforo e radar de prioridade. */

static void direction_step(uint8_t y, uint8_t x, Direcao d, int16_t *ny, int16_t *nx) {
  *ny = (int16_t)y;
  *nx = (int16_t)x;

  if (d == DIRECAO_NORTE) {
    (*ny)--;
  } else if (d == DIRECAO_SUL) {
    (*ny)++;
  } else if (d == DIRECAO_LESTE) {
    (*nx)++;
  } else if (d == DIRECAO_OESTE) {
    (*nx)--;
  }
}

static void ambulance_radar(const CarState *car) {
  uint8_t step;
  int16_t y = car->y;
  int16_t x = car->x;
  Direcao dir = car->dir;

  for (step = 0; step < AMBULANCE_RADAR_CELLS; step++) {
    Cell c;
    direction_step((uint8_t)y, (uint8_t)x, dir, &y, &x);

    if (y < 0 || y >= GRID_ROWS || x < 0 || x >= GRID_COLS) {
      break;
    }

    c = grid_get_cell((uint8_t)y, (uint8_t)x);
    if (c.type == CELL_WALL) {
      break;
    }

    if (c.light_id >= 0) {
      semaphore_force_green((uint8_t)c.light_id % MAX_LIGHTS_PER_REGION);
    }

    if (c.type == CELL_CROSS) {
      dir = task_car_definir_direcao_cruzamento((uint8_t)y, (uint8_t)x, dir);
    }
  }
}

bool task_ambulance_spawn(AmbulanceParams params) {
  BaseType_t rc;

  taskENTER_CRITICAL();
  if (g_ambulance_running) {
    taskEXIT_CRITICAL();
    return false;
  }
  g_ambulance_running = true;
  taskEXIT_CRITICAL();

  g_ambulance_params = params;
  if (g_ambulance_params.velocidade == 0u) {
    g_ambulance_params.velocidade = 1u;
  }

  rc = xTaskCreate(task_ambulance_run, "ambulance", AMBULANCE_STACK_WORDS, &g_ambulance_params,
                   AMBULANCE_PRIORITY, NULL);
  if (rc != pdPASS) {
    taskENTER_CRITICAL();
    g_ambulance_running = false;
    taskEXIT_CRITICAL();
  }
  return rc == pdPASS;
}

void task_ambulance_run(void *params) {
  AmbulanceParams *ap = (AmbulanceParams *)params;
  CarState amb;

  shared_state_set_ambulance_active(true);

  amb.id = 0;
  amb.y = ap->y;
  amb.x = ap->x;
  amb.dir = ap->dir;
  amb.velocidade = (ap->velocidade == 0u) ? 1u : ap->velocidade;
  amb.tick_count = 0;
  amb.active = true;

  while (!grid_acquire_cell(amb.y, amb.x)) {
    task_clock_wait_tick();
  }
  shared_state_set_car_cell(amb.y, amb.x, 255u);

  while (amb.active) {
    task_clock_wait_tick();

    amb.tick_count++;
    if (amb.tick_count < amb.velocidade) {
      continue;
    }
    amb.tick_count = 0;

    ambulance_radar(&amb);

    if (task_car_mover(&amb, false) == CAR_STEP_EXITED) {
      break;
    }
  }

  shared_state_set_ambulance_active(false);
  taskENTER_CRITICAL();
  g_ambulance_running = false;
  taskEXIT_CRITICAL();
  LOG_NORMAL("[AMB]", "ambulancia encerrada");
  vTaskDelete(NULL);
}
