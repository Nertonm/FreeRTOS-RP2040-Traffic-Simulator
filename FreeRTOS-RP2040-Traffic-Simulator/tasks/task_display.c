/* Renderiza o estado local no OLED e na matriz WS2812. */

#include "tasks/task_display.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug_log.h"
#include "drivers/display/display.h"
#include "drivers/ws2812/ws2812.h"
#include "hardware_config.h"         /* NODE_ID */
#include "middleware/shared_state.h" /* sim_snapshot_t */
#include "simulator/traffic_types.h" /* GRID_ROWS, GRID_COLS */
#include "simulator/grid.h"
#include "tasks/task_clock.h"

#include <stdio.h>

/* Configuracao de refresh */

#ifndef DISPLAY_PERIOD_MS
#define DISPLAY_PERIOD_MS 100u
#endif

#define DISPLAY_ROWS 5u
#define DISPLAY_COLS 5u

/* Paleta da matriz 5x5 */
#define LED_MAX 15u
#define LED_ROAD_B 6u
#define LED_CROSS_R 8u
#define LED_CROSS_G 4u
#define LED_BORDER_R 1u
#define LED_BORDER_G 3u
#define LED_BORDER_B 10u
#define LED_SEM_GREEN_G 11u
#define LED_SEM_GREEN_B 2u
#define LED_SEM_RED_R 11u
#define LED_CAR_W 10u
#define LED_TCP_R 7u
#define LED_TCP_B 9u
#define LED_AMB_R 15u
#define LED_AMB_G 8u

#define TCP_BLINK_PERIOD 10u
#define TCP_BLINK_ON 5u

/* Mapa ASCII para o OLED */

/** Converte o conteúdo de uma célula em um char para o OLED. */
static char cell_char(uint8_t y, uint8_t x, const sim_snapshot_t *s, Cell c) {
  /* Ambulância tem prioridade sobre qualquer outra renderização */
  if (s->ambulance_active && s->car_grid[y][x] == AMBULANCE_ID)
    return 'A';

  /* Carro normal */
  if (s->car_grid[y][x] > 0 && s->car_grid[y][x] != AMBULANCE_ID)
    return 'C';

  /* Semáforo */
  if (c.light_id >= 0) {
    uint8_t lid = (uint8_t)c.light_id % MAX_LIGHTS_PER_REGION;
    return (s->semaphore_state[lid] == 1) ? 'G' : 'R';
  }

  /* Célula vazia por tipo */
  if (c.type == WALL)
    return '#';

  return '.';
}

static void led_color_for_cell(const sim_snapshot_t *s, uint32_t frame, uint8_t gx, uint8_t gy,
                               uint8_t *r, uint8_t *g, uint8_t *b) {
  const uint8_t occ = s->car_grid[gy][gx];
  const bool amb_here = (occ == AMBULANCE_ID);
  const bool car_here = (occ > 0u && occ != AMBULANCE_ID);
  const Cell c = grid_get_cell(gy, gx);
  const int8_t sem_id = grid_led_light_id(gx, gy);
  const bool has_sem = (sem_id >= 0);
  const bool sem_green = has_sem && s->semaphore_state[(uint8_t)sem_id % MAX_LIGHTS_PER_REGION];

  *r = 0;
  *g = 0;
  *b = 0;

  if (amb_here) {
    *r = LED_AMB_R;
    *g = LED_AMB_G;
    *b = 0;
    return;
  }

  if (car_here) {
    *r = LED_CAR_W;
    *g = LED_CAR_W;
    *b = LED_CAR_W;
    return;
  }

  if (s->tcp_active && (gx == 0u || gx == (LED_COLS - 1u))) {
    const bool blink_on = (frame % TCP_BLINK_PERIOD) < TCP_BLINK_ON;
    *r = blink_on ? LED_TCP_R : 0;
    *b = blink_on ? LED_TCP_B : 0;
    return;
  }

  if (has_sem) {
    if (sem_green) {
      *g = LED_SEM_GREEN_G;
      *b = LED_SEM_GREEN_B;
    } else {
      *r = LED_SEM_RED_R;
    }
    return;
  }

  if (c.type == CELL_WALL) {
    return;
  }

  if (c.type == CELL_CROSS) {
    *r = LED_CROSS_R;
    *g = LED_CROSS_G;
  } else {
    *b = LED_ROAD_B;
  }

  if (c.type == CELL_BORDER_N || c.type == CELL_BORDER_S) {
    *r = LED_BORDER_R;
    *g = LED_BORDER_G;
    *b = LED_BORDER_B;
  }
}

static void render_traffic_matrix(void) {
  static uint32_t frame = 0;
  sim_snapshot_t s;

  frame++;

  shared_state_get_sim_snapshot(&s);

  for (int ly = 0; ly < LED_ROWS; ly++) {
    for (int lx = 0; lx < LED_COLS; lx++) {
      uint8_t r = 0;
      uint8_t g = 0;
      uint8_t b = 0;

      led_color_for_cell(&s, frame, (uint8_t)lx, (uint8_t)ly, &r, &g, &b);

      led_set_xy_buf(lx, ly, r, g, b);
    }
  }

  led_flush();
}

void task_display(void *param) {
  (void)param;

  /* Buffers reutilizados a cada frame */
  char line[24];
  char grid_row[16];

  LOG_NORMAL("[DISP]", "task iniciada NODE_ID=%d period=%u ms", NODE_ID,
             (unsigned)DISPLAY_PERIOD_MS);

  for (;;) {
    sim_snapshot_t s;
    shared_state_get_sim_snapshot(&s);

    display_clear();

    /* Linha 0: header  "NORTE|Tick:1234|C:03" (21 chars) */
    snprintf(line, sizeof(line), "%-6s|T:%-4lu|C:%02u", s.region_name,
             (unsigned long)(s.tick_count), (unsigned)s.car_count);
    display_text(0, 0, line);

    /* Linhas 1-5: grid 5×5 em ASCII  ".CCAR#GR." etc. */
    for (uint8_t y = 0; y < DISPLAY_ROWS; y++) {
      uint8_t pos = 0;

      /* Prefixo: número da linha */
      grid_row[pos++] = '0' + y;
      grid_row[pos++] = '|';

      for (uint8_t x = 0; x < DISPLAY_COLS; x++) {
        const Cell c = grid_get_cell(y, x);
        grid_row[pos++] = cell_char(y, x, &s, c);
        if (x < DISPLAY_COLS - 1)
          grid_row[pos++] = ' ';
      }
      grid_row[pos] = '\0';
      display_text((uint8_t)(1 + y), 0, grid_row);
    }

    /* Linha 6: indicador  "AMB! " ou "      " */
    if (s.ambulance_active) {
      display_text(6, 0, "!AMBULANCIA     !");
    } else if (s.tcp_active) {
      display_text(6, 0, "TCP sync...     >");
    } else {
      display_text(6, 0, "");
    }

    /* Linha 7: footer com periodo atual e multiplicador de velocidade */
    {
      const uint32_t tick_ms = task_clock_get_period_ms();
      const uint32_t speed_x10 = (TICK_MS * 10u + (tick_ms / 2u)) / tick_ms;

      if (s.paused) {
        snprintf(line, sizeof(line), "|| %3lums %lu.%lux", (unsigned long)tick_ms,
                 (unsigned long)(speed_x10 / 10u), (unsigned long)(speed_x10 % 10u));
        display_text(7, 0, line);

        snprintf(line, sizeof(line), "W:%s", s.wifi_status);
        display_text(7, 14, line);
      } else {
        snprintf(line, sizeof(line), ">  %3lums %lu.%lux", (unsigned long)tick_ms,
                 (unsigned long)(speed_x10 / 10u), (unsigned long)(speed_x10 % 10u));
        display_text(7, 0, line);

        snprintf(line, sizeof(line), "W:%s", s.wifi_status);
        display_text(7, 14, line);
      }
    }

    display_show();

    render_traffic_matrix();

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
  }
}
