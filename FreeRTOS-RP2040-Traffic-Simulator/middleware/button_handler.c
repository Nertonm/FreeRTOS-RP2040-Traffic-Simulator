/**
 * @file button_handler.c
 * @brief Implementação do tratamento de botões físicos e interrupções GPIO.
 *
 * Gerencia a detecção de borda de descida nos pinos dos botões, aplica
 * lógica de debounce baseada em tempo e atualiza os contadores no estado compartilhado.
 *
 * @author
 * @date 2026-03-01
 */

#include "button_handler.h"
#include <stdio.h>

#include "hardware_config.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "shared_state.h"
#include "app_queues.h"

/** @name Variáveis de Controle de Debouncing */
/** @{ */
static volatile uint64_t last_a_us = 0; /**< Último timestamp do A. */
static volatile uint64_t last_b_us = 0; /**< Último timestamp do B. */
static volatile uint64_t last_c_us = 0; /**< Último timestamp do C. */
/** @} */

uint32_t button_handler_get_a_count(void) {
  return shared_state_get_pending_clicks();
}

uint32_t button_handler_get_b_count(void) {
  /**
   * @note Atualmente, o botão B solicita ativação de turbo em vez de incrementar um contador.
   * Retorna 0 por compatibilidade com a interface definida no header.
   */
  return 0;
}

/** @brief Contador de interrupções para diagnóstico */
static volatile uint32_t irq_total_count = 0;
static volatile uint32_t irq_accepted_count = 0;

/**
 * @brief Callback de interrupção (ISR) para eventos GPIO.
 *
 * Processa as interrupções de todos os botões configurados. Realiza o
 * debounce comparando o tempo atual com o último evento registrado.
 *
 * @param[in] gpio Número do pino GPIO que gerou a interrupção.
 * @param[in] events Máscara de eventos que ocorreram (ex: borda de descida).
 */
static void gpio_irq_callback(uint gpio, uint32_t events) {
  uint64_t now = time_us_64();
  btn_event_t evt;
  bool send = false;

  /* Botão A (Spawn Car) */
  if (gpio == BUTTON1_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
    irq_total_count++;
    if (now - last_a_us >= DEBOUNCE_US) {
      last_a_us = now;
      irq_accepted_count++;
      evt = BTN_EVENT_SPAWN_CAR;
      send = true;
    }
  }
  /* Botão B (Spawn Ambulance) */
  else if (gpio == BUTTON2_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
    if (now - last_b_us >= DEBOUNCE_US) {
      last_b_us = now;
      evt = BTN_EVENT_SPAWN_AMBULANCE;
      send = true;
    }
  }
  /* Botão C (Pause via Joystick SW) */
  else if (gpio == BUTTONSTICK_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
    if (now - last_c_us >= DEBOUNCE_US) {
      last_c_us = now;
      evt = BTN_EVENT_TOGGLE_PAUSE;
      send = true;
    }
  }

  if (send) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(app_queues_get_clicks(), &evt, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  }
}

uint32_t button_handler_get_irq_stats(uint32_t *total, uint32_t *accepted) {
  if (total)
    *total = irq_total_count;
  if (accepted)
    *accepted = irq_accepted_count;
  return irq_accepted_count;
}

void button_handler_init(void) {
  /* Configuração do Botão A */
  gpio_init(BUTTON1_PIN);
  gpio_set_dir(BUTTON1_PIN, GPIO_IN);
  gpio_pull_up(BUTTON1_PIN);

  /* Configuração do Botão B */
  gpio_init(BUTTON2_PIN);
  gpio_set_dir(BUTTON2_PIN, GPIO_IN);
  gpio_pull_up(BUTTON2_PIN);

  /* Registro do callback e habilitação das interrupções no Core 0 */
  gpio_set_irq_enabled_with_callback(BUTTON1_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);
  gpio_set_irq_enabled(BUTTON2_PIN, GPIO_IRQ_EDGE_FALL, true);

  /* Configuração do Botão C (Joystick SW) */
  gpio_init(BUTTONSTICK_PIN);
  gpio_set_dir(BUTTONSTICK_PIN, GPIO_IN);
  gpio_pull_up(BUTTONSTICK_PIN);
  gpio_set_irq_enabled(BUTTONSTICK_PIN, GPIO_IRQ_EDGE_FALL, true);
}
