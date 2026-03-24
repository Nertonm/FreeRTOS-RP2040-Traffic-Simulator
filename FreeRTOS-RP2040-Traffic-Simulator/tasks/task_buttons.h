/**
 * @file task_buttons.h
 * @brief Tarefa de processamento de entrada de usuários (botões).
 *
 * @author
 * @date 2026-03-01
 */

#ifndef TASK_BUTTONS_H
#define TASK_BUTTONS_H

/**
 * @brief Tarefa FreeRTOS que processa cliques pendentes.
 *
 * Consome cliques registrados via interrupção no shared_state, atualiza
 * a pontuação local e encaminha os dados para a fila de envio RPC.
 * Também dispara feedbacks visuais imediatos.
 *
 * @param[in] param Parâmetro opcional da tarefa (não utilizado).
 */
void task_buttons(void *param);

#endif /* TASK_BUTTONS_H */
