/**
 * @file ssd1306_i2c.c
 * @author
 * @date 2026-03-01
 * @brief Implementação de baixo nível para o controlador de display SSD1306 via I2C.
 *
 * Contém a lógica de comunicação de barramento, algoritmos de desenho de primitivas
 * gráficas (pontos, linhas, caracteres) e gerenciamento de buffers de renderização.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306_font.h"
#include "ssd1306_i2c.h"

/**
 * @brief Calcula o comprimento do buffer para uma área de renderização específica.
 *
 * @param[in,out] area Ponteiro para a estrutura que define a área de destino.
 */
void calculate_render_area_buffer_length(struct render_area *area) {
  area->buffer_length =
      (area->end_column - area->start_column + 1) * (area->end_page - area->start_page + 1);
}

/**
 * @brief Envia um único comando de controle para o controlador SSD1306.
 *
 * @param[in] command Byte de comando a ser enviado.
 */
void ssd1306_send_command(uint8_t command) {
  uint8_t buffer[2] = {0x80, command};
  i2c_write_blocking(i2c1, ssd1306_i2c_address, buffer, 2, false);
}

/**
 * @brief Envia uma lista sequencial de comandos para o hardware.
 *
 * @param[in] ssd Ponteiro para o array de comandos.
 * @param[in] number Quantidade de comandos no array.
 */
void ssd1306_send_command_list(uint8_t *ssd, int number) {
  for (int i = 0; i < number; i++) {
    ssd1306_send_command(ssd[i]);
  }
}

/**
 * @brief Transfere o buffer de pixels para a memória GDDRAM do display.
 *
 * Adiciona automaticamente o byte de controle 0x40 (Data stream) no início
 * da transmissão I2C.
 *
 * @param[in] ssd Array de pixels.
 * @param[in] buffer_length Tamanho do buffer em bytes.
 */
void ssd1306_send_buffer(uint8_t ssd[], int buffer_length) {
  uint8_t *temp_buffer = malloc(buffer_length + 1);

  temp_buffer[0] = 0x40;
  memcpy(temp_buffer + 1, ssd, buffer_length);

  i2c_write_blocking(i2c1, ssd1306_i2c_address, temp_buffer, buffer_length + 1, false);

  free(temp_buffer);
}

void ssd1306_init() {
  uint8_t commands[] = {
      ssd1306_set_display,
      ssd1306_set_memory_mode,
      0x00,
      ssd1306_set_display_start_line,
      ssd1306_set_segment_remap | 0x01,
      ssd1306_set_mux_ratio,
      ssd1306_height - 1,
      ssd1306_set_common_output_direction | 0x08,
      ssd1306_set_display_offset,
      0x00,
      ssd1306_set_common_pin_configuration,

#if ((ssd1306_width == 128) && (ssd1306_height == 32))
      0x02,
#elif ((ssd1306_width == 128) && (ssd1306_height == 64))
      0x12,
#else
      0x02,
#endif
      ssd1306_set_display_clock_divide_ratio,
      0x80,
      ssd1306_set_precharge,
      0xF1,
      ssd1306_set_vcomh_deselect_level,
      0x30,
      ssd1306_set_contrast,
      0xFF,
      ssd1306_set_entire_on,
      ssd1306_set_normal_display,
      ssd1306_set_charge_pump,
      0x14,
      ssd1306_set_scroll | 0x00,
      ssd1306_set_display | 0x01,
  };

  ssd1306_send_command_list(commands, count_of(commands));
}

void ssd1306_scroll(bool set) {
  uint8_t commands[] = {ssd1306_set_horizontal_scroll | 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0xFF,
                        ssd1306_set_scroll | (set ? 0x01 : 0)};

  ssd1306_send_command_list(commands, count_of(commands));
}

void render_on_display(uint8_t *ssd, struct render_area *area) {
  uint8_t commands[] = {ssd1306_set_column_address, area->start_column, area->end_column,
                        ssd1306_set_page_address,   area->start_page,   area->end_page};

  ssd1306_send_command_list(commands, count_of(commands));
  ssd1306_send_buffer(ssd, area->buffer_length);
}

void ssd1306_set_pixel(uint8_t *ssd, int x, int y, bool set) {
  assert(x >= 0 && x < ssd1306_width && y >= 0 && y < ssd1306_height);

  const int bytes_per_row = ssd1306_width;

  int byte_idx = (y / 8) * bytes_per_row + x;
  uint8_t byte = ssd[byte_idx];

  if (set) {
    byte |= 1 << (y % 8);
  } else {
    byte &= ~(1 << (y % 8));
  }

  ssd[byte_idx] = byte;
}

/**
 * @brief Desenha uma linha entre dois pontos usando o Algoritmo de Bresenham.
 */
void ssd1306_draw_line(uint8_t *ssd, int x_0, int y_0, int x_1, int y_1, bool set) {
  int dx = abs(x_1 - x_0);
  int dy = -abs(y_1 - y_0);
  int sx = x_0 < x_1 ? 1 : -1;
  int sy = y_0 < y_1 ? 1 : -1;
  int error = dx + dy;
  int error_2;

  while (true) {
    ssd1306_set_pixel(ssd, x_0, y_0, set);
    if (x_0 == x_1 && y_0 == y_1) {
      break;
    }

    error_2 = 2 * error;

    if (error_2 >= dy) {
      error += dy;
      x_0 += sx;
    }
    if (error_2 <= dx) {
      error += dx;
      y_0 += sy;
    }
  }
}

/**
 * @brief Obtém o índice do glifo da fonte para um determinado caractere ASCII.
 *
 * @param[in] character Caractere ASCII.
 * @return int Índice no array de fonte (ssd1306_font.h).
 */
inline int ssd1306_get_font(uint8_t character) {
  if (character >= 'A' && character <= 'Z') {
    return character - 'A' + 1;
  } else if (character >= '0' && character <= '9') {
    return character - '0' + 27;
  } else
    return 0;
}

void ssd1306_draw_char(uint8_t *ssd, int16_t x, int16_t y, uint8_t character) {
  if (x > ssd1306_width - 8 || y > ssd1306_height - 8) {
    return;
  }

  y = y / 8;

  character = toupper(character);
  int idx = ssd1306_get_font(character);
  int fb_idx = y * 128 + x;

  for (int i = 0; i < 8; i++) {
    ssd[fb_idx++] = font[idx * 8 + i];
  }
}

void ssd1306_draw_string(uint8_t *ssd, int16_t x, int16_t y, char *string) {
  if (x > ssd1306_width - 8 || y > ssd1306_height - 8) {
    return;
  }

  while (*string) {
    ssd1306_draw_char(ssd, x, y, *string++);
    x += 8;
  }
}

/**
 * @brief Envia um comando para o display através da estrutura de controle ssd1306_t.
 * @param[in] ssd Ponteiro para a struct de controle.
 * @param[in] command Byte de comando.
 */
void ssd1306_command(ssd1306_t *ssd, uint8_t command) {
  ssd->port_buffer[1] = command;
  i2c_write_blocking(ssd->i2c_port, ssd->address, ssd->port_buffer, 2, false);
}

void ssd1306_config(ssd1306_t *ssd) {
  ssd1306_command(ssd, ssd1306_set_display | 0x00);
  ssd1306_command(ssd, ssd1306_set_memory_mode);
  ssd1306_command(ssd, 0x01);
  ssd1306_command(ssd, ssd1306_set_display_start_line | 0x00);
  ssd1306_command(ssd, ssd1306_set_segment_remap | 0x01);
  ssd1306_command(ssd, ssd1306_set_mux_ratio);
  ssd1306_command(ssd, ssd1306_height - 1);
  ssd1306_command(ssd, ssd1306_set_common_output_direction | 0x08);
  ssd1306_command(ssd, ssd1306_set_display_offset);
  ssd1306_command(ssd, 0x00);
  ssd1306_command(ssd, ssd1306_set_common_pin_configuration);
  ssd1306_command(ssd, 0x12);
  ssd1306_command(ssd, ssd1306_set_display_clock_divide_ratio);
  ssd1306_command(ssd, 0x80);
  ssd1306_command(ssd, ssd1306_set_precharge);
  ssd1306_command(ssd, 0xF1);
  ssd1306_command(ssd, ssd1306_set_vcomh_deselect_level);
  ssd1306_command(ssd, 0x30);
  ssd1306_command(ssd, ssd1306_set_contrast);
  ssd1306_command(ssd, 0xFF);
  ssd1306_command(ssd, ssd1306_set_entire_on);
  ssd1306_command(ssd, ssd1306_set_normal_display);
  ssd1306_command(ssd, ssd1306_set_charge_pump);
  ssd1306_command(ssd, 0x14);
  ssd1306_command(ssd, ssd1306_set_display | 0x01);
}

void ssd1306_init_bm(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc,
                     uint8_t address, i2c_inst_t *i2c) {
  ssd->width = width;
  ssd->height = height;
  ssd->pages = height / 8U;
  ssd->address = address;
  ssd->i2c_port = i2c;
  ssd->bufsize = ssd->pages * ssd->width + 1;
  ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));
  ssd->ram_buffer[0] = 0x40;
  ssd->port_buffer[0] = 0x80;
}

void ssd1306_send_data(ssd1306_t *ssd) {
  ssd1306_command(ssd, ssd1306_set_column_address);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->width - 1);
  ssd1306_command(ssd, ssd1306_set_page_address);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->pages - 1);
  i2c_write_blocking(ssd->i2c_port, ssd->address, ssd->ram_buffer, ssd->bufsize, false);
}

void ssd1306_draw_bitmap(ssd1306_t *ssd, const uint8_t *bitmap) {
  for (int i = 0; i < ssd->bufsize - 1; i++) {
    ssd->ram_buffer[i + 1] = bitmap[i];

    ssd1306_send_data(ssd);
  }
}
