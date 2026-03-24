/**
 * @file ws2812.c
 * @brief Implementação do driver para LEDs WS2812 via PIO.
 *
 * Gerencia a temporização precisa para controle de LEDs NeoPixel,
 * incluindo mapeamento de coordenadas (x, y) para a topologia de
 * fiação em serpentina da placa BitDogLab.
 *
 * @author
 * @date 2026-03-01
 */

#include "ws2812.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware_config.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"
#include <stdio.h>

/** @brief Instância do periférico PIO utilizada. */
static PIO pio_inst;

/** @brief Índice da máquina de estado (State Machine) do PIO. */
static uint sm_inst;

/**
 * @brief Estrutura de pixel no padrão GRB (Green-Red-Blue).
 * @note Este é o padrão exigido pela maioria dos módulos WS2812.
 */
typedef struct {
  uint8_t G, R, B;
} pixel_t;

/** @brief Buffer de RAM contendo as cores de todos os LEDs da matriz. */
static pixel_t led_buffer[WS2812_NUM_LEDS];

void led_matrix_init(void) {
  uint offset;
  printf("[WS2812] Inicializando modo BitDogLab (GPIO %d)...\n", WS2812_PIN);
  fflush(stdout);

  // Inicialização do pino GPIO
  gpio_init(WS2812_PIN);
  gpio_set_dir(WS2812_PIN, GPIO_OUT);
  gpio_put(WS2812_PIN, 0);
  sleep_us(300); // Pulso de reset inicial para os LEDs

  // Tenta carregar o programa no PIO0 ou PIO1 conforme disponibilidade
  pio_inst = pio0;
  if (!pio_can_add_program(pio_inst, &ws2812_program)) {
    pio_inst = pio1;
  }

  offset = pio_add_program(pio_inst, &ws2812_program);
  sm_inst = pio_claim_unused_sm(pio_inst, true);

  // Configura a máquina de estado com clock de 800kHz
  ws2812_program_init(pio_inst, sm_inst, offset, WS2812_PIN, 800000.f, false);

  printf("[WS2812] Pronto: PIO %d, SM %d\n", pio_get_index(pio_inst), sm_inst);
  fflush(stdout);

  led_clear_all();
}

/**
 * @brief Envia o conteúdo do buffer de RAM para o hardware via PIO.
 *
 * Realiza o empacotamento dos 24 bits (GRB) em uma palavra de 32 bits
 * antes de enviar para a FIFO do PIO.
 */
static void np_write(void) {
  for (uint i = 0; i < WS2812_NUM_LEDS; i++) {
    // Bits 31-24: G, Bits 23-16: R, Bits 15-8: B
    uint32_t color = ((uint32_t)led_buffer[i].G << 24) | ((uint32_t)led_buffer[i].R << 16) |
                     ((uint32_t)led_buffer[i].B << 8);
    pio_sm_put_blocking(pio_inst, sm_inst, color);
  }
  sleep_us(100); // Pulso de Latch (fim de quadro)
}

void led_set(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index >= WS2812_NUM_LEDS)
    return;

  led_buffer[index].R = r;
  led_buffer[index].G = g;
  led_buffer[index].B = b;

  np_write();
}

void led_clear_all(void) {
  for (uint i = 0; i < WS2812_NUM_LEDS; i++) {
    led_buffer[i].R = 0;
    led_buffer[i].G = 0;
    led_buffer[i].B = 0;
  }
  np_write();
}

/**
 * @brief Converte coordenadas cartesianas (x, y) para o índice linear do buffer.
 *
 * Implementa o mapeamento em "serpentina" específico da matriz BitDogLab:
 * - Linhas pares (0, 2, 4): Direita para Esquerda.
 * - Linhas ímpares (1, 3): Esquerda para Direita.
 *
 * @param[in] x Posição horizontal (0 esquerda a 4 direita).
 * @param[in] y Posição vertical (0 baixo a 4 cima).
 * @return int Índice correspondente no array led_buffer.
 */
static int get_index(int x, int y) {
  if (y % 2 == 0) {
    return y * 5 + (4 - x);
  } else {
    return y * 5 + x;
  }
}

/**
 * @brief Bitmaps pré-definidos para representação de números na matriz 5x5.
 * @note Cada bit representa um LED (bit 0 = (4,0), bit 24 = (0,4)).
 */
static const uint32_t final_bitmaps[10] = {
    0x0E8C62E, // 0
    0x046108E, // 1
    0x1F0FE1F, // 2
    0x1F0FC3F, // 3
    0x118FC21, // 4
    0x1F87C3F, // 5
    0x1F87E3F, // 6
    0x1F08888, // 7
    0x1F8FE3F, // 8
    0x1F8FC3F  // 9
};

void led_matrix_draw_number(uint8_t num, uint8_t r, uint8_t g, uint8_t b) {
  if (num > 9)
    return;

  uint32_t bitmap = final_bitmaps[num];

  // Limpa o buffer antes de aplicar o novo número
  for (int i = 0; i < 25; i++) {
    led_buffer[i].R = 0;
    led_buffer[i].G = 0;
    led_buffer[i].B = 0;
  }

  // Mapeia os bits do bitmap para o buffer seguindo a lógica da matriz
  for (int y = 0; y < 5; y++) {
    for (int x = 0; x < 5; x++) {
      int bit_pos = y * 5 + (4 - x);
      if (bitmap & (1 << bit_pos)) {
        int index = get_index(x, y);
        led_buffer[index].R = r;
        led_buffer[index].G = g;
        led_buffer[index].B = b;
      }
    }
  }
  np_write();
}

void led_matrix_set_all(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < 25; i++) {
    led_buffer[i].R = r;
    led_buffer[i].G = g;
    led_buffer[i].B = b;
  }
  np_write();
}

void led_set_xy_buf(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  int idx = get_index(x, y);
  if (idx < 0 || idx >= WS2812_NUM_LEDS)
    return;

  led_buffer[idx].R = r;
  led_buffer[idx].G = g;
  led_buffer[idx].B = b;
  /* DESIGN: nao chama np_write aqui para evitar transmissao por LED. */
}

void led_flush(void) {
  np_write();
}
