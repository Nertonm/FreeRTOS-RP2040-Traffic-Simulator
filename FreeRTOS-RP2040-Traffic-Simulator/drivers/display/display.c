/**
 * @file display.c
 * @author
 * @date 2026-03-01
 * @brief Implementação da interface simplificada para o display OLED SSD1306.
 *
 * Provê abstração para inicialização I2C, limpeza de buffer e renderização
 * de texto em áreas específicas do display.
 */

#include "display.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306_i2c.h"
#include <string.h>

/** @brief Pino SDA utilizado pelo display (padrão BitDogLab). */
#define DISPLAY_SDA 14
/** @brief Pino SCL utilizado pelo display (padrão BitDogLab). */
#define DISPLAY_SCL 15

/** @brief Buffer estático de pixels (RAM) para o display. */
static uint8_t ssd_buf[ssd1306_buffer_length];

/** @brief Definição da área total de renderização do quadro (fullscreen). */
static struct render_area frame_area;

void display_init(void) {
  // Inicializa o barramento I2C1 com a frequência configurada
  i2c_init(i2c1, ssd1306_i2c_clock * 1000);

  // Configura os pinos GPIO para a função I2C com pull-up
  gpio_set_function(DISPLAY_SDA, GPIO_FUNC_I2C);
  gpio_set_function(DISPLAY_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(DISPLAY_SDA);
  gpio_pull_up(DISPLAY_SCL);

  sleep_ms(10);

  // Envia os comandos de inicialização para o controlador SSD1306
  ssd1306_init();

  // Configura a área de renderização para cobrir todo o display (128x64)
  frame_area.start_column = 0;
  frame_area.end_column = ssd1306_width - 1;
  frame_area.start_page = 0;
  frame_area.end_page = ssd1306_n_pages - 1;
  calculate_render_area_buffer_length(&frame_area);

  // Limpa a tela na inicialização
  display_clear();
  display_show();
}

void display_clear(void) {
  // Zera todos os bytes do buffer de RAM (pixels desligados)
  memset(ssd_buf, 0, ssd1306_buffer_length);
}

void display_text(uint8_t linha, uint8_t col, const char *texto) {
  int16_t y = linha * 8;
  ssd1306_draw_string(ssd_buf, col, y, (char *)texto);
}

void display_show(void) {
  render_on_display(ssd_buf, &frame_area);
}
