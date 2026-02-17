#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Configurações básicas */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      133000000

// Aumentado para 1ms (lwIP requer 10ms, mas 1ms é melhor para o simulador)
#define configTICK_RATE_HZ                      1000

// Aumentado para 5 para permitir mais tarefas (lwIP + simulação)
#define configMAX_PRIORITIES                    5

// Aumentado para 256 para tarefas mais complexas (lwIP + simulação)
#define configMINIMAL_STACK_SIZE                256

// Aumentado para 16 para nomes de tarefas mais longos (para depuração)
#define configMAX_TASK_NAME_LEN                 16

// lwIP requer 8 bits, mas 16 bits é mais eficiente para o RP2040
#define configUSE_16_BIT_TICKS                  0

// Permitir que a tarefa ociosa ceda para outras tarefas de mesma prioridade
#define configIDLE_SHOULD_YIELD                 1

/* *** DUAL-CORE SMP *** */
#define configNUM_CORES                         2
#define configUSE_CORE_AFFINITY                 1
#define configRUN_MULTIPLE_PRIORITIES           1

/* *** MEMÓRIA - Aumentada para WiFi *** */
#define configTOTAL_HEAP_SIZE                   (128*1024)

/* *** SINCRONIZAÇÃO (para o projeto) *** */
// Habilitar mutexes e semáforos para sincronização entre tarefas (lwIP + simulação)
#define configUSE_MUTEXES                       1
// Habilitar semáforos de contagem para controle de recursos (lwIP + simulação)
#define configUSE_COUNTING_SEMAPHORES           1
// Habilitar mutexes recursivos para casos de bloqueio aninhado (lwIP + simulação)
#define configUSE_RECURSIVE_MUTEXES             1
// Habilitar notificações de tarefas para comunicação eficiente entre tarefas (lwIP + simulação)
#define configUSE_TASK_NOTIFICATIONS            1
// Habilitar conjuntos de filas para aguardar em múltiplas filas/semaforos (lwIP + simulação)
#define configUSE_QUEUE_SETS                    1

/* *** TIMERS (necessário para lwIP) *** */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            1024

/* *** PICO W - CYW43 + lwIP *** */
#define configSUPPORT_PICO_SYNC_INTEROP         1
#define configSUPPORT_PICO_TIME_INTEROP         1

/* Debug */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_TRACE_FACILITY                1

/* APIs */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/* Hooks */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);

#endif
