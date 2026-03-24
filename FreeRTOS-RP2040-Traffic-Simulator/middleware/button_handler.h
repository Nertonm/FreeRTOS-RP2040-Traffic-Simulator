/**
 * @file button_handler.h
 * @brief Gerenciamento de botões físicos e tratamento de interrupções.
 *
 * Provê abstração para os botões da placa (A e B), incluindo lógica de
 * debounce e detecção de pressionamento longo (long press).
 *
 * @author
 * @date 2026-03-01
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>

/**
 * @brief Limiar de debounce configurável em microssegundos.
 * @note 80ms elimina bouncing mecânico sem comprometer responsividade (~12 cliques/s).
 */
#define DEBOUNCE_US 80000

/**
 * @brief Tempo necessário em microssegundos para considerar um pressionamento longo.
 * @note Padrão de 800ms para distinguir press longo do botão B.
 */
#define LONG_PRESS_US 800000

/**
 * @brief Inicializa os pinos de entrada e configura interrupções para os botões.
 *
 * @note Deve ser chamado pelo Core 0 no início da aplicação.
 */
void button_handler_init(void);

/**
 * @brief Obtém a contagem de pressionamentos curtos acumulada no botão A.
 * @return uint32_t Número de cliques registrados.
 */
uint32_t button_handler_get_a_count(void);

/**
 * @brief Obtém a contagem de pressionamentos acumulada no botão B.
 * @return uint32_t Número de cliques registrados.
 */
uint32_t button_handler_get_b_count(void);

/**
 * @brief Obtém estatísticas de interrupções para diagnóstico.
 *
 * @param[out] total Total de interrupções recebidas (pode ser NULL).
 * @param[out] accepted Interrupções aceitas após debounce (pode ser NULL).
 * @return uint32_t Valor de accepted.
 */
uint32_t button_handler_get_irq_stats(uint32_t *total, uint32_t *accepted);

#endif // BUTTON_HANDLER_H
