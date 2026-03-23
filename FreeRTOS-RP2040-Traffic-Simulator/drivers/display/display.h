/**
 * @file display.h
 * @brief Interface de alto nível para controle do display OLED.
 *
 * @author
 * @date 2026-03-01
 */

#pragma once
#include <stdint.h>

/**
 * @brief Inicializa o hardware do display e limpa o buffer interno.
 */
void display_init(void);

/**
 * @brief Limpa todo o conteúdo do buffer de renderização do display.
 */
void display_clear(void);

/**
 * @brief Adiciona um texto ao buffer do display em uma posição específica.
 *
 * @param[in] linha Índice da linha (normalmente 0 a 7 para SSD1306).
 * @param[in] col Índice da coluna (0 a 127).
 * @param[in] texto Ponteiro para a string de caracteres a ser exibida.
 */
void display_text(uint8_t linha, uint8_t col, const char *texto);

/**
 * @brief Transmite o buffer de renderização para o hardware do display via I2C.
 * Deve ser chamado para que as alterações feitas por display_text se tornem visíveis.
 */
void display_show(void);
