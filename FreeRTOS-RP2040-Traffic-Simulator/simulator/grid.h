/* === Grid local === */

#ifndef GRID_H
#define GRID_H

#include "FreeRTOS.h"
#include "semphr.h"

#include "simulator/traffic_types.h"

#include <stdbool.h>
#include <stdint.h>

extern SemaphoreHandle_t cell_mutex[GRID_ROWS][GRID_COLS];

void grid_init(uint8_t node_id);
bool grid_acquire_cell(uint8_t y, uint8_t x);
void grid_release_cell(uint8_t y, uint8_t x);
const Cell grid_get_cell(uint8_t y, uint8_t x);
int8_t grid_get_light_id(uint8_t y, uint8_t x);
bool grid_can_enter(uint8_t y, uint8_t x, Direction dir);
Direction grid_cell_directions(uint8_t y, uint8_t x);

/* DESIGN: a API retorna ponteiro para lista estatica de spawns da regiao. */
const SpawnPoint *grid_get_spawns(uint8_t *out_count);

/* Mapeamento LED: retorna id do semaforo no LED (led_x,led_y), ou -1 */
int8_t grid_led_light_id(uint8_t led_x, uint8_t led_y);

/* Compatibilidade com modulos existentes. */
bool grid_in_bounds(uint8_t y, uint8_t x);
void grid_dump(void);

#endif
