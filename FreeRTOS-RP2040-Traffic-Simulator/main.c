/* === Main do simulador distribuido === */

#include "FreeRTOS.h"
#include "task.h"

#include "audio/buzzer.h"
#include "debug_log.h"
#include "drivers/display/display.h"
#include "drivers/ws2812/ws2812.h"
#include "hardware_config.h"
#include "middleware/app_queues.h"
#include "middleware/button_handler.h"
#include "middleware/lamport.h"
#include "middleware/shared_state.h"
#include "pico/stdlib.h"
#include "simulator/grid.h"
#include "tasks/task_ambulance.h"
#include "tasks/task_buttons.h"
#include "tasks/task_car.h"
#include "tasks/task_clock.h"
#include "tasks/task_display.h"
#include "tasks/task_monitor.h"
#include "tasks/task_rpc.h"
#include "tasks/task_semaphore.h"

#include <stdio.h>

#if NODE_ID == 0
#define NODE_CITY "Crato"
#elif NODE_ID == 1
#define NODE_CITY "Juazeiro"
#elif NODE_ID == 2
#define NODE_CITY "Barbalha"
#else
#define NODE_CITY "Desconhecido"
#endif

#define BOOT_STEP(label, call)                                                                     \
  do {                                                                                             \
    absolute_time_t _t0 = get_absolute_time();                                                     \
    call;                                                                                          \
    LOG_NORMAL("[BOOT]", "%s ok dt=%lu ms", label,                                                 \
               (unsigned long)(absolute_time_diff_us(_t0, get_absolute_time()) / 1000u));          \
  } while (0)

#define TASK_CREATE(fn, name, stack, arg, prio)                                                    \
  do {                                                                                             \
    BaseType_t _rc = xTaskCreate(fn, name, stack, arg, prio, 0);                                   \
    configASSERT(_rc == pdPASS);                                                                   \
    LOG_NORMAL("[BOOT]", "task %s criada p=%u", name, (unsigned)(prio));                           \
  } while (0)

int main(void) {
  /* DESIGN: todo estado compartilhado e primitivas FreeRTOS sao criados no boot
   * antes do scheduler para evitar alocacao dinamica durante runtime. */
  stdio_init_all();
  setvbuf(stdout, NULL, _IONBF, 0);
  sleep_ms(1200);

  LOG_NORMAL("[BOOT]", "node=%u cidade=%s", (unsigned)NODE_ID, NODE_CITY);

  BOOT_STEP("shared_state_init", shared_state_init());
  BOOT_STEP("lamport_init", lamport_init());
  BOOT_STEP("grid_init", grid_init((uint8_t)NODE_ID));
  BOOT_STEP("button_handler_init", button_handler_init());
  BOOT_STEP("display_init", display_init());
  BOOT_STEP("led_matrix_init", led_matrix_init());
  BOOT_STEP("buzzer_init", buzzer_init());
  BOOT_STEP("app_queues_init", app_queues_init());

  BOOT_STEP("task_clock_init", task_clock_init());
  BOOT_STEP("task_semaphore_init", task_semaphore_init());
  BOOT_STEP("task_car_init", task_car_init());

  TASK_CREATE(task_clock_run, "clock", 1024, NULL, 3);
  TASK_CREATE(task_semaphore_run, "semaphore", 512, NULL, 1);
  TASK_CREATE(task_buttons, "buttons", 512, NULL, 3);
  TASK_CREATE(task_rpc, "rpc", 4096, NULL, 2);
  TASK_CREATE(task_display, "display", 1024, NULL, 0);
  TASK_CREATE(task_monitor, "monitor", 768, NULL, 1);

  LOG_NORMAL("[BOOT]", "scheduler start cidade=%s", NODE_CITY);
  vTaskStartScheduler();

  LOG_ERROR("[BOOT]", "scheduler retornou");
  while (1) {
    tight_loop_contents();
  }
}
