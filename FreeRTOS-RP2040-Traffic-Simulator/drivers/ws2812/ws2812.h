/**
 * @file ws2812.h
 * @brief Driver de controle para LEDs endereçáveis WS2812 (NeoPixel).
 *
 * Utiliza o periférico PIO do RP2040 para gerar o timing preciso
 * exigido pelo protocolo de comunicação de um único fio dos LEDs.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Inicializa a matriz de LEDs WS2812 via PIO.
 *
 * Configura a máquina de estado do PIO e os pinos GPIO conforme definido
 * em hardware_config.h.
 */
void led_matrix_init(void);

/**
 * @brief Define a cor de um LED específico na matriz.
 *
 * @param[in] index Índice do LED na cadeia (0 a WS2812_NUM_LEDS - 1).
 * @param[in] r Intensidade do componente Vermelho (0-255).
 * @param[in] g Intensidade do componente Verde (0-255).
 * @param[in] b Intensidade do componente Azul (0-255).
 */
void led_set(int index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Apaga todos os LEDs da matriz.
 */
void led_clear_all(void);

/**
 * @brief Desenha a representação visual de um dígito na matriz 5x5.
 *
 * @param[in] num O dígito a ser exibido (0 a 9).
 * @param[in] r Componente Vermelho.
 * @param[in] g Componente Verde.
 * @param[in] b Componente Azul.
 */
void led_matrix_draw_number(uint8_t num, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Define uma cor única para todos os LEDs da matriz simultaneamente.
 *
 * @param[in] r Componente Vermelho.
 * @param[in] g Componente Verde.
 * @param[in] b Componente Azul.
 */
void led_matrix_set_all(uint8_t r, uint8_t g, uint8_t b);

/* Atualiza so o buffer interno (sem enviar ao hardware). */
void led_set_xy_buf(int x, int y, uint8_t r, uint8_t g, uint8_t b);

/* Envia o buffer inteiro ao hardware (uma unica transmissao PIO). */
void led_flush(void);

#endif // WS2812_H
