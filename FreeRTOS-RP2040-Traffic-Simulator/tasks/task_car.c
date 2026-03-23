/* === Logica de carro === */

#include "tasks/task_car.h"

#include "middleware/shared_state.h"
#include "middleware/app_queues.h"
#include "middleware/rpc_types.h"
#include "hardware_config.h"
#include "simulator/grid.h"
#include "tasks/task_clock.h"
#include "tasks/task_semaphore.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "debug_log.h"

#include <stdlib.h>
#include <string.h>

#define CAR_STACK_WORDS 512u
#define CAR_TASK_PRIORITY 2u
#define CAR_STALL_REROUTE_TICKS 4u
#define CAR_STALL_WARN_TICKS 12u

typedef struct {
  CarParams params;
  TaskHandle_t handle;
  bool in_use;
} CarSlot;

static CarSlot g_slots[MAX_CARS_PER_REGION];
static SemaphoreHandle_t g_slots_mutex;
static uint8_t g_spawn_reserved[GRID_ROWS][GRID_COLS];
static Direcao direcao_oposta(Direcao d);
static int16_t car_score_direction(uint8_t y, uint8_t x, Direcao current, Direcao candidate,
                                   const sim_snapshot_t *snap);

/* DESIGN: slots estaticos evitam malloc e garantem limite rigido de carros por regiao. */

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

static const char *car_dir_name(Direcao d) {
  if (d == DIRECAO_NORTE) {
    return "NORTE";
  }
  if (d == DIRECAO_SUL) {
    return "SUL";
  }
  if (d == DIRECAO_LESTE) {
    return "LESTE";
  }
  if (d == DIRECAO_OESTE) {
    return "OESTE";
  }
  return "NONE";
}

static const char *car_cell_type_name(CellType type) {
  if (type == CELL_ROAD_NS) {
    return "VIA_NS";
  }
  if (type == CELL_ROAD_EW) {
    return "VIA_EW";
  }
  if (type == CELL_ONE_EAST) {
    return "MAO_LESTE";
  }
  if (type == CELL_ONE_WEST) {
    return "MAO_OESTE";
  }
  if (type == CELL_BORDER_N) {
    return "BORDA_N";
  }
  if (type == CELL_BORDER_S) {
    return "BORDA_S";
  }
  if (type == CELL_CROSS) {
    return "CRUZAMENTO";
  }
  return "PAREDE";
}

static const char *car_turn_name(Direcao from, Direcao to) {
  if (from == to) {
    return "RETA";
  }
  if (direcao_oposta(from) == to) {
    return "RETORNO";
  }

  if (from == DIRECAO_NORTE) {
    if (to == DIRECAO_LESTE) {
      return "DIREITA";
    }
    if (to == DIRECAO_OESTE) {
      return "ESQUERDA";
    }
  } else if (from == DIRECAO_SUL) {
    if (to == DIRECAO_OESTE) {
      return "DIREITA";
    }
    if (to == DIRECAO_LESTE) {
      return "ESQUERDA";
    }
  } else if (from == DIRECAO_LESTE) {
    if (to == DIRECAO_SUL) {
      return "DIREITA";
    }
    if (to == DIRECAO_NORTE) {
      return "ESQUERDA";
    }
  } else if (from == DIRECAO_OESTE) {
    if (to == DIRECAO_NORTE) {
      return "DIREITA";
    }
    if (to == DIRECAO_SUL) {
      return "ESQUERDA";
    }
  }

  return "MANOBRA";
}

/**
 * @brief Calcula um score para decidir a melhor direção no cruzamento.
 *
 * @param[in] y Linha da célula atual.
 * @param[in] x Coluna da célula atual.
 * @param[in] current Direção atual do carro.
 * @param[in] candidate Direção candidata para o próximo movimento.
 * @param[in] snap Snapshot de ocupação e semáforos.
 * @return Score maior indica melhor opção de fluxo.
 */
static int16_t car_score_direction(uint8_t y, uint8_t x, Direcao current, Direcao candidate,
                                   const sim_snapshot_t *snap) {
  int16_t ny;
  int16_t nx;
  int16_t score = 0;
  Cell next_cell;

  if (snap == NULL) {
    return -32768;
  }

  direction_step(y, x, candidate, &ny, &nx);
  if (ny < 0 || ny >= GRID_ROWS || nx < 0 || nx >= GRID_COLS) {
    return -300;
  }

  if (!grid_can_enter((uint8_t)ny, (uint8_t)nx, candidate)) {
    return -300;
  }

  next_cell = grid_get_cell((uint8_t)ny, (uint8_t)nx);
  if (next_cell.type == CELL_WALL) {
    return -300;
  }

  if (candidate == current) {
    score += 8;
  } else {
    score += 5;
  }

  if (snap->car_grid[ny][nx] == 0u) {
    score += 12;
  } else {
    score -= 16;
  }

  if (next_cell.light_id >= 0) {
    uint8_t lid = (uint8_t)next_cell.light_id % MAX_LIGHTS_PER_REGION;
    if (snap->semaphore_state[lid] == 1u) {
      score += 5;
    } else {
      score -= 5;
    }
  }

  if (grid_can_enter((uint8_t)ny, (uint8_t)nx, current) &&
      snap->car_grid[(uint8_t)ny][(uint8_t)nx] == 0u) {
    score += 2;
  }

  return score;
}

static Direcao direcao_oposta(Direcao d) {
  if (d == DIRECAO_NORTE) {
    return DIRECAO_SUL;
  }
  if (d == DIRECAO_SUL) {
    return DIRECAO_NORTE;
  }
  if (d == DIRECAO_LESTE) {
    return DIRECAO_OESTE;
  }
  if (d == DIRECAO_OESTE) {
    return DIRECAO_LESTE;
  }
  return DIRECAO_NONE;
}

static void car_mark_active_count(void) {
  uint8_t i;
  uint8_t active = 0;

  for (i = 0; i < MAX_CARS_PER_REGION; i++) {
    if (g_slots[i].in_use) {
      active++;
    }
  }
  shared_state_set_car_count(active);
}

static bool car_spawn_cell_available(uint8_t y, uint8_t x) {
  sim_snapshot_t snap;
  if (!grid_in_bounds(y, x)) {
    return false;
  }

  if (g_spawn_reserved[y][x] != 0u) {
    return false;
  }

  shared_state_get_sim_snapshot(&snap);
  return snap.car_grid[y][x] == 0u;
}

static bool transfer_neighbor_for_exit(Direcao dir, uint8_t *dst_node) {
  if (dst_node == NULL) {
    return false;
  }

  if (dir == DIRECAO_LESTE) {
#if NODE_ID == 0
    *dst_node = 1u;
    return true;
#elif NODE_ID == 1
    *dst_node = 2u;
    return true;
#else
    return false;
#endif
  }

  if (dir == DIRECAO_OESTE) {
#if NODE_ID == 2
    *dst_node = 1u;
    return true;
#elif NODE_ID == 1
    *dst_node = 0u;
    return true;
#else
    return false;
#endif
  }

  return false;
}

static void transfer_publish_exit(const CarState *car) {
  QueueHandle_t q;
  RpcMessage msg;
  uint8_t dst_node;

  if (car == NULL || !transfer_neighbor_for_exit(car->dir, &dst_node)) {
    return;
  }

  q = app_queues_get_rpc_out();
  if (q == NULL) {
    return;
  }

  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_TRANSFER_CAR;
  msg.src_node = (uint8_t)NODE_ID;
  msg.dst_node = dst_node;
  msg.payload_len = (uint8_t)sizeof(CarState);
  memcpy(msg.payload, car, sizeof(CarState));

  if (xQueueSend(q, &msg, pdMS_TO_TICKS(10)) == pdTRUE) {
    shared_state_set_tcp_active(true);
    LOG_NORMAL("[CAR]", "transfer enfileirada %u->%u y=%u dir=%u", (unsigned)NODE_ID,
               (unsigned)dst_node, (unsigned)car->y, (unsigned)car->dir);
  } else {
    LOG_ERROR("[CAR]", "falha ao enfileirar transferencia %u->%u", (unsigned)NODE_ID,
              (unsigned)dst_node);
  }
}

static void try_reroute_from_current_cell(CarState *car) {
  Direcao new_dir;
  const Cell cur = grid_get_cell(car->y, car->x);

  if (car == NULL || cur.type != CELL_CROSS) {
    return;
  }

  new_dir = task_car_definir_direcao_cruzamento(car->y, car->x, car->dir);
  car->dir = new_dir;
}

/**
 * @brief Seleciona a direção de saída em cruzamento priorizando melhor fluxo.
 *
 * @param[in] y Linha do cruzamento.
 * @param[in] x Coluna do cruzamento.
 * @param[in] dir_atual Direção atual do veículo.
 * @return Direção escolhida para o próximo movimento.
 */
Direcao task_car_definir_direcao_cruzamento(uint8_t y, uint8_t x, Direcao dir_atual) {
  Direcao candidatas[3];
  int16_t scores[3];
  int16_t best_score = -32768;
  uint8_t n = 0;
  uint8_t best_count = 0;
  Direcao oposta = direcao_oposta(dir_atual);
  Direction mask;
  const Cell c = grid_get_cell(y, x);
  sim_snapshot_t snap;

  if (c.type != CELL_CROSS) {
    return dir_atual;
  }

  shared_state_get_sim_snapshot(&snap);
  mask = grid_cell_directions(y, x);

  for (uint8_t bit = 0; bit < 4; bit++) {
    Direcao d = (Direcao)(1u << bit);
    int16_t ny;
    int16_t nx;

    if ((mask & d) == 0 || d == oposta) {
      continue;
    }

    direction_step(y, x, d, &ny, &nx);
    if (ny < 0 || ny >= GRID_ROWS || nx < 0 || nx >= GRID_COLS) {
      continue;
    }

    if (grid_can_enter((uint8_t)ny, (uint8_t)nx, d)) {
      int16_t s;

      s = car_score_direction(y, x, dir_atual, d, &snap);
      candidatas[n++] = d;
      scores[n - 1u] = s;

      if (s > best_score) {
        best_score = s;
        best_count = 1u;
      } else if (s == best_score) {
        best_count++;
      }

      if (n == 3) {
        break;
      }
    }
  }

  if (n == 0) {
    return dir_atual;
  }

  if (best_count <= 1u) {
    for (uint8_t i = 0; i < n; i++) {
      if (scores[i] == best_score) {
        return candidatas[i];
      }
    }
  }

  {
    uint8_t pick = (uint8_t)(rand() % best_count);
    uint8_t seen = 0;
    for (uint8_t i = 0; i < n; i++) {
      if (scores[i] != best_score) {
        continue;
      }

      if (seen == pick) {
        return candidatas[i];
      }
      seen++;
    }
  }

  return candidatas[0];
}

/**
 * @brief Executa um passo de movimentação do carro.
 *
 * @param[in,out] car Estado do carro.
 * @param[in] respeitar_semaforo Se true, aguarda sinal verde nas células com semáforo.
 * @return Resultado do passo (aguardou, moveu ou saiu da região).
 */
CarStepResult task_car_mover(CarState *car, bool respeitar_semaforo) {
  int16_t ny;
  int16_t nx;
  Cell destino;
  Direcao dir_before = DIRECAO_NONE;
  uint8_t occupancy_marker = 0u;
  sim_snapshot_t snap;

  if (car == NULL || !car->active) {
    return CAR_STEP_WAIT;
  }

  direction_step(car->y, car->x, car->dir, &ny, &nx);

  shared_state_get_sim_snapshot(&snap);
  occupancy_marker = snap.car_grid[car->y][car->x];
  if (occupancy_marker == 0u) {
    occupancy_marker = (uint8_t)(car->id + 1u);
  }

  if (ny < 0 || ny >= GRID_ROWS || nx < 0 || nx >= GRID_COLS) {
    transfer_publish_exit(car);
    shared_state_set_car_cell(car->y, car->x, 0);
    grid_release_cell(car->y, car->x);
    car->active = false;
    return CAR_STEP_EXITED;
  }

  destino = grid_get_cell((uint8_t)ny, (uint8_t)nx);
  if (destino.type == CELL_WALL) {
    return CAR_STEP_WAIT;
  }

  if (!grid_can_enter((uint8_t)ny, (uint8_t)nx, car->dir)) {
    LOG_VERBOSE("[CAR]", "carro=%u bloqueado: entrada invalida em (%u,%u) tipo=%s dir=%s",
                (unsigned)car->id, (unsigned)ny, (unsigned)nx, car_cell_type_name(destino.type),
                car_dir_name(car->dir));
    return CAR_STEP_WAIT;
  }

  if (respeitar_semaforo && destino.light_id >= 0) {
    const TickType_t sem_wait = pdMS_TO_TICKS(task_clock_get_period_ms());
    if (!semaphore_wait_green_timeout((uint8_t)destino.light_id % MAX_LIGHTS_PER_REGION,
                                      sem_wait)) {
      try_reroute_from_current_cell(car);
      LOG_VERBOSE("[CAR]", "carro=%u aguardou semaforo=%d em (%u,%u) e nao passou",
                  (unsigned)car->id, (int)destino.light_id, (unsigned)ny, (unsigned)nx);
      return CAR_STEP_WAIT;
    }
  }

  if (!grid_acquire_cell((uint8_t)ny, (uint8_t)nx)) {
    try_reroute_from_current_cell(car);
    return CAR_STEP_WAIT;
  }

  shared_state_set_car_cell(car->y, car->x, 0);
  grid_release_cell(car->y, car->x);

  car->y = (uint8_t)ny;
  car->x = (uint8_t)nx;
  dir_before = car->dir;

  if (destino.type == CELL_CROSS) {
    car->dir = task_car_definir_direcao_cruzamento(car->y, car->x, car->dir);
    LOG_NORMAL("[CAR]", "carro=%u via=%s (%u,%u) curva=%s (%s->%s)", (unsigned)car->id,
               car_cell_type_name(destino.type), (unsigned)car->y, (unsigned)car->x,
               car_turn_name(dir_before, car->dir), car_dir_name(dir_before),
               car_dir_name(car->dir));
  } else {
    LOG_VERBOSE("[CAR]", "carro=%u seguiu via=%s para (%u,%u) dir=%s", (unsigned)car->id,
                car_cell_type_name(destino.type), (unsigned)car->y, (unsigned)car->x,
                car_dir_name(car->dir));
  }

  shared_state_set_car_cell(car->y, car->x, occupancy_marker);
  return CAR_STEP_MOVED;
}

void task_car_init(void) {
  memset(g_slots, 0, sizeof(g_slots));
  memset(g_spawn_reserved, 0, sizeof(g_spawn_reserved));
  g_slots_mutex = xSemaphoreCreateMutex();
  configASSERT(g_slots_mutex != NULL);
  shared_state_set_car_count(0);
}

/**
 * @brief Cria uma task de carro em um ponto inicial válido.
 *
 * @param[in] params Parâmetros de spawn.
 * @return true se o carro foi criado com sucesso.
 */
bool task_car_spawn(CarParams params) {
  BaseType_t rc;
  uint8_t slot = MAX_CARS_PER_REGION;
  uint8_t i;
  const Cell spawn_cell = grid_get_cell(params.y, params.x);

  if (!grid_in_bounds(params.y, params.x)) {
    return false;
  }

  if (params.dir == DIRECAO_NONE) {
    return false;
  }

  if (spawn_cell.type == CELL_WALL || !grid_can_enter(params.y, params.x, params.dir)) {
    return false;
  }

  if (xSemaphoreTake(g_slots_mutex, portMAX_DELAY) != pdTRUE) {
    return false;
  }

  if (!car_spawn_cell_available(params.y, params.x)) {
    (void)xSemaphoreGive(g_slots_mutex);
    return false;
  }

  for (i = 0; i < MAX_CARS_PER_REGION; i++) {
    if (!g_slots[i].in_use) {
      slot = i;
      break;
    }
  }

  if (slot == MAX_CARS_PER_REGION) {
    (void)xSemaphoreGive(g_slots_mutex);
    return false;
  }

  params.id = slot;
  g_slots[slot].params = params;
  g_slots[slot].in_use = true;
  g_spawn_reserved[params.y][params.x] = 1u;

  rc = xTaskCreate(task_car_run, "car", CAR_STACK_WORDS, &g_slots[slot].params, CAR_TASK_PRIORITY,
                   &g_slots[slot].handle);
  if (rc != pdPASS) {
    g_spawn_reserved[params.y][params.x] = 0u;
    g_slots[slot].in_use = false;
    (void)xSemaphoreGive(g_slots_mutex);
    return false;
  }

  car_mark_active_count();
  (void)xSemaphoreGive(g_slots_mutex);
  return true;
}

/**
 * @brief Loop principal da task de carro.
 *
 * @param[in] params Ponteiro para CarParams.
 */
void task_car_run(void *params) {
  CarParams *cp = (CarParams *)params;
  CarState car;
  uint8_t wait_streak = 0;

  configASSERT(cp != NULL);

  car.id = cp->id;
  car.y = cp->y;
  car.x = cp->x;
  car.dir = cp->dir;
  car.velocidade = (cp->velocidade == 0u) ? 1u : cp->velocidade;
  car.tick_count = 0;
  car.active = true;

  while (!grid_acquire_cell(car.y, car.x)) {
    task_clock_wait_tick();
  }

  if (xSemaphoreTake(g_slots_mutex, portMAX_DELAY) == pdTRUE) {
    g_spawn_reserved[car.y][car.x] = 0u;
    (void)xSemaphoreGive(g_slots_mutex);
  }

  shared_state_set_car_cell(car.y, car.x, (uint8_t)(car.id + 1u));

  while (car.active) {
    CarStepResult step;

    task_clock_wait_tick();

    car.tick_count++;
    if (car.tick_count < car.velocidade) {
      continue;
    }
    car.tick_count = 0;

    step = task_car_mover(&car, true);
    if (step == CAR_STEP_EXITED) {
      break;
    }

    if (step == CAR_STEP_MOVED) {
      wait_streak = 0;
    } else {
      wait_streak++;
      if ((wait_streak % CAR_STALL_REROUTE_TICKS) == 0u) {
        try_reroute_from_current_cell(&car);
      }

      if ((wait_streak % CAR_STALL_WARN_TICKS) == 0u) {
        LOG_NORMAL("[CAR]", "carro=%u congestionado em (%u,%u) dir=%s espera=%u", (unsigned)car.id,
                   (unsigned)car.y, (unsigned)car.x, car_dir_name(car.dir),
                   (unsigned)wait_streak);
      }
    }
  }

  if (xSemaphoreTake(g_slots_mutex, portMAX_DELAY) == pdTRUE) {
    g_slots[car.id].in_use = false;
    g_slots[car.id].handle = NULL;
    car_mark_active_count();
    (void)xSemaphoreGive(g_slots_mutex);
  }

  vTaskDelete(NULL);
}
