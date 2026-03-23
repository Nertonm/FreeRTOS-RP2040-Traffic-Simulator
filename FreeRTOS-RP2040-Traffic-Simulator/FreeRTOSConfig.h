/**
 * @file FreeRTOSConfig.h
 * @author
 * @date 2026-03-01
 * @brief Arquivo de configuração do kernel FreeRTOS para o RP2040.
 *
 * Contém parâmetros fundamentais para o comportamento do escalonador,
 * gerenciamento de memória (heap), tratamento de interrupções e
 * funcionalidades habilitadas do kernel.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "pico.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @name Configurações Básicas do Escalonador */
/** @{ */
#define configUSE_PREEMPTION 1 /**< 1: Escalonamento preemptivo ativado. */
#define configCPU_CLOCK_HZ                                                                         \
  133000000 /**< Clock nominal do RP2040 (133 MHz).                                                \
             */
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000 /**< Frequência do Tick (1ms). */
#endif
#define configMAX_PRIORITIES 8       /**< Número máximo de níveis de prioridade (0 a 7). */
#define configMINIMAL_STACK_SIZE 256 /**< Tamanho mínimo da pilha da tarefa Idle. */
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#define configMAX_TASK_NAME_LEN 16
#define configUSE_TIME_SLICING 1
#define configUSE_TICK_HOOK 0
/** @} */

/** @name Configurações de Memória e Heap */
/** @{ */
#define configSUPPORT_DYNAMIC_ALLOCATION 1 /**< Habilita alocação dinâmica (pvPortMalloc). */
#define configSUPPORT_STATIC_ALLOCATION 0
#define configTOTAL_HEAP_SIZE (80 * 1024) /**< Tamanho total do Heap do FreeRTOS (80KB). */
/** @} */

/** @name Detecção de Erros e Depuração */
/** @{ */
#define configCHECK_FOR_STACK_OVERFLOW 2 /**< 2: Detecção robusta de estouro de pilha. */
#define configUSE_MALLOC_FAILED_HOOK 1   /**< Chama vApplicationMallocFailedHook em caso de erro. */
#define configUSE_IDLE_HOOK 1
/** @} */

/** @name Configurações Específicas para RP2040 (Pico SDK) */
/** @{ */
#define configNUMBER_OF_CORES 1 /**< Atualmente configurado para rodar em 1 núcleo. */
#define configRUN_MULTIPLE_PRIORITIES 1
#define configUSE_CORE_AFFINITY 0
/** @} */

/** @name Timers e Primitivos de Sincronização */
/** @{ */
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH 16
#define configTIMER_TASK_STACK_DEPTH 512

#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configQUEUE_REGISTRY_SIZE 8

#define configSUPPORT_PICO_SYNC_INTEROP 1
#define configSUPPORT_PICO_TIME_INTEROP 1
/** @} */

/** @name Configurações para lwIP (Rede) */
/** @{ */
#define configLWIP_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define configLWIP_TASK_STACK_DEPTH 512
/** @} */

/**
 * @brief Macro ASSERT customizada para reportar erros fatais via UART.
 */
void vAssertCalled(const char *file, int line);
#define configASSERT(x)                                                                            \
  do {                                                                                             \
    if ((x) == 0)                                                                                  \
      vAssertCalled(__FILE__, __LINE__);                                                           \
  } while (0)

/** @name Funcionalidades do Kernel (INCLUDE) */
/** @{ */
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTimerPendFunctionCall 1
#define INCLUDE_xSemaphoreGetMutexHolder 1
/** @} */

#ifdef __cplusplus
}
#endif

#endif // FREERTOS_CONFIG_H
