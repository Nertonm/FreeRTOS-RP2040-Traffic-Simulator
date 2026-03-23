/**
 * @file ssd1306_i2c.h
 * @brief Driver de baixo nível para o controlador de display OLED SSD1306 via
 * I2C.
 *
 * Contém definições de comandos, estruturas de dados e protótipos para
 * manipulação direta do buffer do display e envio de dados via protocolo I2C.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef ssd1306_inc_h
#define ssd1306_inc_h

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdlib.h>

/** @name Dimensões do Display */
/** @{ */
#define ssd1306_height 64 /**< Altura do display em pixels. */
#define ssd1306_width 128 /**< Largura do display em pixels. */
/** @} */

/** @name Configurações I2C */
/** @{ */
#define ssd1306_i2c_address _u(0x3C) /**< Endereço I2C padrão do SSD1306. */
#define ssd1306_i2c_clock 400        /**< Frequência do clock I2C em kHz. */
/** @} */

/** @name Comandos do Controlador SSD1306 */
/** @{ */
#define ssd1306_set_memory_mode _u(0x20)
#define ssd1306_set_column_address _u(0x21)
#define ssd1306_set_page_address _u(0x22)
#define ssd1306_set_horizontal_scroll _u(0x26)
#define ssd1306_set_scroll _u(0x2E)
#define ssd1306_set_display_start_line _u(0x40)
#define ssd1306_set_contrast _u(0x81)
#define ssd1306_set_charge_pump _u(0x8D)
#define ssd1306_set_segment_remap _u(0xA0)
#define ssd1306_set_entire_on _u(0xA4)
#define ssd1306_set_all_on _u(0xA5)
#define ssd1306_set_normal_display _u(0xA6)
#define ssd1306_set_inverse_display _u(0xA7)
#define ssd1306_set_mux_ratio _u(0xA8)
#define ssd1306_set_display _u(0xAE)
#define ssd1306_set_common_output_direction _u(0xC0)
#define ssd1306_set_common_output_direction_flip _u(0xC0)
#define ssd1306_set_display_offset _u(0xD3)
#define ssd1306_set_display_clock_divide_ratio _u(0xD5)
#define ssd1306_set_precharge _u(0xD9)
#define ssd1306_set_common_pin_configuration _u(0xDA)
#define ssd1306_set_vcomh_deselect_level _u(0xDB)
/** @} */

/** @name Parâmetros de Paginação */
/** @{ */
#define ssd1306_page_height _u(8)
#define ssd1306_n_pages (ssd1306_height / ssd1306_page_height)
#define ssd1306_buffer_length (ssd1306_n_pages * ssd1306_width)
/** @} */

#define ssd1306_write_mode _u(0xFE)
#define ssd1306_read_mode _u(0xFF)

/**
 * @brief Define uma área de renderização no display.
 */
struct render_area {
  uint8_t start_column; /**< Coluna inicial. */
  uint8_t end_column;   /**< Coluna final. */
  uint8_t start_page;   /**< Página inicial. */
  uint8_t end_page;     /**< Página final. */

  int buffer_length; /**< Comprimento calculado do buffer para esta área. */
};

/**
 * @brief Estrutura de controle para o display SSD1306.
 */
typedef struct {
  uint8_t width;          /**< Largura configurada. */
  uint8_t height;         /**< Altura configurada. */
  uint8_t pages;          /**< Número de páginas. */
  uint8_t address;        /**< Endereço I2C. */
  i2c_inst_t *i2c_port;   /**< Instância I2C do SDK (i2c0 ou i2c1). */
  bool external_vcc;      /**< Flag indicando se utiliza VCC externo. */
  uint8_t *ram_buffer;    /**< Ponteiro para o buffer de RAM (pixels). */
  size_t bufsize;         /**< Tamanho total do buffer de RAM. */
  uint8_t port_buffer[2]; /**< Buffer interno para transmissões curtas. */
} ssd1306_t;

/* --- Funções para manipulação de buffer de texto --- */

/**
 * @brief Calcula o comprimento do buffer necessário para uma área de
 * renderização.
 * @param[in,out] area Ponteiro para a struct de área de renderização.
 */
void calculate_render_area_buffer_length(struct render_area *area);

/**
 * @brief Inicializa o display com as configurações padrão de texto.
 */
void ssd1306_init(void);

/**
 * @brief Transmite uma área de memória para uma região específica do display.
 * @param[in] ssd Ponteiro para o buffer de pixels.
 * @param[in] area Ponteiro para a definição da área de destino.
 */
void render_on_display(uint8_t *ssd, struct render_area *area);

/**
 * @brief Define ou limpa um pixel no buffer.
 * @param[in,out] ssd Buffer de pixels.
 * @param[in] x Posição X.
 * @param[in] y Posição Y.
 * @param[in] set Verdadeiro para ligar, falso para desligar.
 */
void ssd1306_set_pixel(uint8_t *ssd, int x, int y, bool set);

/**
 * @brief Desenha uma linha no buffer.
 *
 * @param[in,out] ssd Buffer de pixels.
 * @param[in] x_0 X inicial.
 * @param[in] y_0 Y inicial.
 * @param[in] x_1 X final.
 * @param[in] y_1 Y final.
 * @param[in] set Verdadeiro para ligar, falso para desligar.
 */
void ssd1306_draw_line(uint8_t *ssd, int x_0, int y_0, int x_1, int y_1, bool set);

/**
 * @brief Desenha um único caractere no buffer.
 *
 * @param[in,out] ssd Buffer de pixels.
 * @param[in] x Posição X.
 * @param[in] y Posição Y.
 * @param[in] character Caractere ASCII a ser desenhado.
 */
void ssd1306_draw_char(uint8_t *ssd, int16_t x, int16_t y, uint8_t character);

/**
 * @brief Desenha uma string de texto no buffer.
 *
 * @param[in,out] ssd Buffer de pixels.
 * @param[in] x Posição X.
 * @param[in] y Posição Y.
 * @param[in] string Ponteiro para a string de texto.
 */
void ssd1306_draw_string(uint8_t *ssd, int16_t x, int16_t y, char *string);

/**
 * @brief Ativa ou desativa o scroll horizontal do display.
 * @param[in] set Verdadeiro para ativar, falso para desativar.
 */
void ssd1306_scroll(bool set);

/* --- Funções para manipulação via struct ssd1306_t (Bitmap) --- */

/**
 * @brief Inicializa a estrutura de controle do display para uso com bitmaps.
 *
 * @param[out] ssd Ponteiro para a estrutura a ser inicializada.
 * @param[in] width Largura do display.
 * @param[in] height Altura do display.
 * @param[in] external_vcc Uso de VCC externo.
 * @param[in] address Endereço I2C.
 * @param[in] i2c Instância I2C.
 */
void ssd1306_init_bm(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc,
                     uint8_t address, i2c_inst_t *i2c);

/**
 * @brief Envia comandos de configuração para o hardware.
 * @param[in] ssd Ponteiro para a estrutura de controle.
 */
void ssd1306_config(ssd1306_t *ssd);

/**
 * @brief Envia o buffer completo da estrutura para o display.
 * @param[in] ssd Ponteiro para a estrutura de controle.
 */
void ssd1306_send_data(ssd1306_t *ssd);

/**
 * @brief Copia um bitmap completo para o buffer interno.
 * @param[in,out] ssd Ponteiro para a estrutura de controle.
 * @param[in] bitmap Ponteiro para os dados do bitmap.
 */
void ssd1306_draw_bitmap(ssd1306_t *ssd, const uint8_t *bitmap);

#endif
