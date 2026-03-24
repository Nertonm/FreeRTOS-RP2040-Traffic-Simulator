/**
 * @file buzzer.h
 * @brief Driver de controle para feedback sonoro via buzzer piezoelétrico.
 *
 * Utiliza o periférico PWM do RP2040 para gerar frequências audíveis.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/**
 * @brief Inicializa o pino do buzzer para operação PWM.
 *
 * Configura o GPIO e o slice PWM correspondente conforme definido
 * nas configurações de hardware.
 */
void buzzer_init(void);

/**
 * @brief Emite um tom com frequência e duração especificadas.
 *
 * @note Esta função não é bloqueante (utiliza timers internos para desligar o som).
 *
 * @param[in] freq_hz Frequência do tom desejada em Hz.
 * @param[in] duration_ms Duração do tom em milissegundos.
 */
void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

#endif // BUZZER_H
