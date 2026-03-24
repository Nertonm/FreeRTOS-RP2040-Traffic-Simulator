/**
 * @file task_rpc.h
 * @brief Tarefa de comunicação de rede e sincronização de dados.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef TASK_RPC_H
#define TASK_RPC_H

/**
 * @brief Tarefa FreeRTOS que gerencia a conexão WiFi e chamadas RPC.
 *
 * Responsável por realizar o discovery do servidor, manter a conexão ativa,
 * enviar cliques pendentes e sincronizar as pontuações globais.
 *
 * @param[in] param Parâmetro opcional da tarefa (não utilizado).
 */
void task_rpc(void *param);

#endif /* TASK_RPC_H */
