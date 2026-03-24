/**
 * @file task_buttons.c
 * @author
 * @date 2026-03-01
 * @brief Implementação da tarefa de processamento de cliques.
 *
 * Esta tarefa detecta novos cliques registrados pela ISR, emite feedback
 * sonoro via buzzer e solicita feedback visual via LED flash.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "audio/buzzer.h"
#include "debug_log.h"
#include "hardware/adc.h"
#include "hardware_config.h"
#include "middleware/app_queues.h"
#include "middleware/shared_state.h"
#include "simulator/grid.h"
#include "task_buttons.h"
#include "tasks/task_ambulance.h"
#include "tasks/task_car.h"
#include "tasks/task_clock.h"

/** @brief Frequência do beep de feedback de clique (Hz). */
#define CLICK_BEEP_FREQ_HZ 1200u
/** @brief Duração do beep de feedback de clique (ms). */
#define CLICK_BEEP_DURATION_MS 25u
#define ANALOG_POLL_MS 30u
#define JOY_ADC_TRIGGER_LOW 900u
#define JOY_ADC_TRIGGER_HIGH 3200u
#define JOY_ADC_REARM_LOW 1700u
#define JOY_ADC_REARM_HIGH 2400u

static uint8_t g_spawn_rr_car = 0u;
static uint8_t g_spawn_rr_amb = 0u;
static bool g_analog_ready_y = true;
static bool g_analog_ready_x = true;

static uint16_t joystick_read_x(void) {
  adc_select_input(1);
  (void)adc_read();
  return adc_read();
}

static uint16_t joystick_read_y(void) {
  adc_select_input(0);
  (void)adc_read();
  return adc_read();
}

static void toggle_pause_state(void) {
  sim_snapshot_t snap;
  shared_state_get_sim_snapshot(&snap);
  shared_state_set_paused(!snap.paused);
  LOG_NORMAL("[JOY]", "pause=%u", snap.paused ? 0u : 1u);
}

static void handle_analog_controls(void) {
  const uint16_t joy_x = joystick_read_x();
  const uint16_t joy_y = joystick_read_y();

  if (joy_y > JOY_ADC_REARM_LOW && joy_y < JOY_ADC_REARM_HIGH) {
    g_analog_ready_y = true;
  } else if (g_analog_ready_y) {
    if (joy_y >= JOY_ADC_TRIGGER_HIGH) {
      if (task_clock_speed_up()) {
        LOG_NORMAL("[JOY]", "velocidade + tick_ms=%lu", (unsigned long)task_clock_get_period_ms());
      } else {
        LOG_VERBOSE("[JOY]", "velocidade no maximo");
      }
      g_analog_ready_y = false;
    } else if (joy_y <= JOY_ADC_TRIGGER_LOW) {
      if (task_clock_speed_down()) {
        LOG_NORMAL("[JOY]", "velocidade - tick_ms=%lu", (unsigned long)task_clock_get_period_ms());
      } else {
        LOG_VERBOSE("[JOY]", "velocidade no minimo");
      }
      g_analog_ready_y = false;
    }
  }

  if (joy_x > JOY_ADC_REARM_LOW && joy_x < JOY_ADC_REARM_HIGH) {
    g_analog_ready_x = true;
  } else if (g_analog_ready_x) {
    toggle_pause_state();
    g_analog_ready_x = false;
  }
}

static bool next_spawn_point(uint8_t rr_seed, uint8_t *out_idx, SpawnPoint *out_spawn) {
  uint8_t count = 0;
  const SpawnPoint *spawns = grid_get_spawns(&count);
  const uint8_t idx = (count > 0u) ? (uint8_t)(rr_seed % count) : 0u;

  if (out_idx == NULL || out_spawn == NULL || spawns == NULL || count == 0u) {
    return false;
  }

  *out_idx = idx;
  *out_spawn = spawns[idx];
  return true;
}

static bool spawn_car_from_region_spawns(void) {
  uint8_t count = 0;
  const SpawnPoint *spawns = grid_get_spawns(&count);

  if (spawns == NULL || count == 0u) {
    return false;
  }

  for (uint8_t step = 0; step < count; step++) {
    uint8_t idx = 0;
    SpawnPoint spawn = {0};
    if (!next_spawn_point(g_spawn_rr_car, &idx, &spawn)) {
      return false;
    }

    CarParams cp = {0};
    cp.dir = spawn.dir;
    cp.x = spawn.col;
    cp.y = spawn.row;
    cp.velocidade = 2;

    if (task_car_spawn(cp)) {
      g_spawn_rr_car = (uint8_t)((idx + 1u) % count);
      return true;
    }

    g_spawn_rr_car = (uint8_t)((idx + 1u) % count);
  }

  return false;
}

static bool spawn_ambulance_from_region_spawns(void) {
  uint8_t count = 0;
  const SpawnPoint *spawns = grid_get_spawns(&count);

  if (spawns == NULL || count == 0u) {
    return false;
  }

  for (uint8_t step = 0; step < count; step++) {
    uint8_t idx = 0;
    SpawnPoint spawn = {0};
    if (!next_spawn_point(g_spawn_rr_amb, &idx, &spawn)) {
      return false;
    }

    AmbulanceParams ap = {0};
    ap.dir = spawn.dir;
    ap.x = spawn.col;
    ap.y = spawn.row;
    ap.velocidade = 1;

    if (task_ambulance_spawn(ap)) {
      g_spawn_rr_amb = (uint8_t)((idx + 1u) % count);
      return true;
    }

    g_spawn_rr_amb = (uint8_t)((idx + 1u) % count);
  }

  return false;
}

/**
 * @brief Loop principal da tarefa de botões.
 *
 * @param[in] param Parâmetro opcional da tarefa (não utilizado).
 */
void task_buttons(void *param) {
  (void)param;
  btn_event_t evt;

  adc_init();
  adc_gpio_init(JOY_VRX_PIN);
  adc_gpio_init(JOY_VRY_PIN);

  while (1) {
    if (xQueueReceive(app_queues_get_clicks(), &evt, pdMS_TO_TICKS(ANALOG_POLL_MS)) == pdTRUE) {
      /* Feedback visual: flash de LED */
      shared_state_set_led_flash_requested(true);
      /* Feedback sonoro */
      buzzer_tone(CLICK_BEEP_FREQ_HZ, CLICK_BEEP_DURATION_MS);

      if (evt == BTN_EVENT_SPAWN_CAR) {
        if (!spawn_car_from_region_spawns()) {
          LOG_VERBOSE("[BTN]", "spawn de carro ignorado: sem entrada livre");
        }
      } else if (evt == BTN_EVENT_SPAWN_AMBULANCE) {
        if (!spawn_ambulance_from_region_spawns()) {
          LOG_VERBOSE("[BTN]", "spawn de ambulancia ignorado: sem entrada livre");
        }
      } else if (evt == BTN_EVENT_TOGGLE_PAUSE) {
        toggle_pause_state();
      }
    }

    handle_analog_controls();
  }
}
