/**
 * @file app_queues.c
 * @brief Implementação da criação de filas para o FreeRTOS.
 *
 * Gerencia a alocação de memória para as filas de mensagens utilizadas
 * na comunicação inter-tarefas.
 */

#include "app_queues.h"
#include "config/firmware_config.h"

/** @brief Handle privado para a fila de cliques. */
static QueueHandle_t queue_clicks;

/** @brief Handle privado para a fila RPC do simulador. */
static QueueHandle_t queue_to_rpc;

void app_queues_init(void) {
  /* Cria a fila com o comprimento definido nas configurações */
  queue_clicks = xQueueCreate(CLICK_QUEUE_LEN, sizeof(btn_event_t));
  configASSERT(queue_clicks != NULL);

  /* Fila RPC para mensagens do simulador (ex: MSG_SYNC_TICK) */
  queue_to_rpc = xQueueCreate(10, sizeof(RpcMessage));
  configASSERT(queue_to_rpc != NULL);
}

QueueHandle_t app_queues_get_clicks(void) {
  return queue_clicks;
}
QueueHandle_t app_queues_get_rpc_out(void) {
  return queue_to_rpc;
}
