/**
 * @file task_monitor.h
 * @brief Tarefa de monitoramento de integridade do sistema.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef TASK_MONITOR_H
#define TASK_MONITOR_H

/**
 * @brief Tarefa FreeRTOS para telemetria e depuração via UART.
 *
 * Monitora periodicamente o uso de heap, profundidade das filas e status
 * de conectividade, imprimindo os dados na saída serial.
 *
 * @param[in] param Parâmetro opcional da tarefa (não utilizado).
 */
void task_monitor(void *param);

#endif /* TASK_MONITOR_H */
