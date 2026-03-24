/**
 * @file task_monitor.c
 * @author
 * @date 2026-03-01
 * @brief Implementação da tarefa de monitoramento de integridade do sistema.
 *
 * Realiza telemetria periódica via UART, reportando o estado das filas,
 * uso de memória heap e status de conectividade.
 */

#include "task_monitor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "config/firmware_config.h"
#include "debug_log.h"
#include "middleware/app_queues.h"
#include "middleware/button_handler.h"
#include "middleware/lamport.h"
#include "middleware/shared_state.h"
#include "rpc_client.h"
#include <stdio.h>

/* Número de períodos de MONITOR_PERIOD_MS entre dumps de diagnóstico completo */
#define DIAG_DUMP_EVERY_N 5u

/**
 * @brief Loop principal da tarefa de monitoramento.
 *
 * @param[in] param Parâmetro opcional da tarefa (não utilizado).
 */
void task_monitor(void *param) {
  (void)param;

  QueueHandle_t queue_clicks = app_queues_get_clicks();
  uint32_t monitor_iter = 0;

  while (1) {
    monitor_iter++;

    /* Coleta métricas do sistema */
    UBaseType_t depth = uxQueueMessagesWaiting(queue_clicks);
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_heap = xPortGetMinimumEverFreeHeapSize();
    uint32_t irq_total, irq_accepted;
    button_handler_get_irq_stats(&irq_total, &irq_accepted);
    uint32_t pending = shared_state_get_pending_clicks();
    connection_status_t s = shared_state_get_connection_status();
    uint32_t lamport_ts = lamport_get_current();

    /* Reporta telemetria estruturada */
    LOG_NORMAL("[STATS]",
               "queue=%lu heap=%lu min_heap=%lu pending=%lu irq=%lu/%lu "
               "lamport=%lu status=%s",
               (unsigned long)depth, (unsigned long)free_heap, (unsigned long)min_heap,
               (unsigned long)pending, (unsigned long)irq_accepted, (unsigned long)irq_total,
               (unsigned long)lamport_ts, conn_status_name((int)s));

    /* Verificação de invariantes de saúde */
    DIAG_CHECK(free_heap > 2048, "[ERROR]", "HEAP CRITICO heap_livre=%lu bytes",
               (unsigned long)free_heap);

    /* Dump completo de diagnóstico a cada DIAG_DUMP_EVERY_N períodos */
    if ((monitor_iter % DIAG_DUMP_EVERY_N) == 0) {
      rpc_print_diagnostics();
      lamport_print_diagnostics();
    }

    /* Aguarda o próximo ciclo de monitoramento definido em firmware_config.h */
    vTaskDelay(pdMS_TO_TICKS(MONITOR_PERIOD_MS));
  }
}
