#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

void vAssertCalled(const char *file, int line) {
  taskDISABLE_INTERRUPTS();
  printf("[FREERTOS] ASSERT %s:%d\n", file, line);
  for (;;) {
  }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  (void)xTask;
  taskDISABLE_INTERRUPTS();
  printf("[FREERTOS] Stack overflow em task: %s\n",
         (pcTaskName != NULL) ? pcTaskName : "<unknown>");
  for (;;) {
  }
}

void vApplicationIdleHook(void) {
  /* Hook ocioso intencionalmente vazio. */
}

void vApplicationMallocFailedHook(void) {
  taskDISABLE_INTERRUPTS();
  printf("[FREERTOS] Malloc failed\n");
  for (;;) {
  }
}
