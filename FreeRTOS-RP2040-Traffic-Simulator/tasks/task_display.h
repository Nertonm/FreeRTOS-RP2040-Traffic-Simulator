/**
 * @file task_display.h
 * @brief Tarefa de gerenciamento da interface visual (OLED e LEDs).
 *
 * @author
 * @date 2026-03-01
 */

#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H

/**
 * @brief Tarefa FreeRTOS responsável pela atualização periódica do display OLED.
 *
 * Lê o estado global do dispositivo (scores, status de conexão, modo turbo)
 * e atualiza o buffer do display SSD1306 e os LEDs WS2812.
 *
 * @param[in] param Parâmetro opcional da tarefa (não utilizado).
 */
void task_display(void *param);

#endif /* TASK_DISPLAY_H */
