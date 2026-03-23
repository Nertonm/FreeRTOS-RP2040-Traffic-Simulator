/**
 * @file lamport.h
 * @brief Relógio lógico de Lamport para ordenação causal de eventos.
 *
 * Implementa um contador incremental para garantir a ordem parcial de eventos em um sistema
 * distribuído. O estado interno é protegido pelo mesmo spinlock do shared_state para garantir
 * atomicidade.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef LAMPORT_H
#define LAMPORT_H

#include <stdint.h>

/**
 * @brief Inicializa o relógio de Lamport (L = 0).
 *
 * @note Deve ser chamado após shared_state_init(), pois utiliza o mesmo spinlock de sincronização.
 */
void lamport_init(void);

/**
 * @brief Incrementa o relógio e retorna o novo valor (L = L + 1).
 *
 * Deve ser chamado imediatamente antes de cada envio de requisição RPC para carimbar a mensagem.
 *
 * @return uint32_t Novo valor do timestamp após incremento.
 */
uint32_t lamport_tick(void);

/**
 * @brief Atualiza o relógio local com base em um timestamp recebido.
 *
 * Aplica a regra: L = max(L_local, received_ts) + 1.
 * Deve ser chamado após receber uma resposta RPC bem-sucedida contendo o timestamp do servidor.
 *
 * @param[in] received_ts Timestamp retornado pelo servidor.
 */
void lamport_update(uint32_t received_ts);

/* DESIGN: alias semantico para sincronizacao remota entre nos (clock distribuido). */
void lamport_update_from_remote(uint32_t remote_ts);

/**
 * @brief Retorna o valor atual do relógio sem incrementá-lo.
 *
 * Útil para fins de depuração ou exibição de estado (ex: Core 1 / task_display).
 *
 * @return uint32_t Valor atual do timestamp Lamport.
 */
uint32_t lamport_get_current(void);

/**
 * @brief Imprime estatísticas de diagnóstico do relógio Lamport.
 *
 * Exibe via printf: ts atual, total de ticks, updates, saltos anormais e resets.
 * Pode ser chamada periodicamente pelo task_monitor ou sob demanda.
 */
void lamport_print_diagnostics(void);

#endif /* LAMPORT_H */
