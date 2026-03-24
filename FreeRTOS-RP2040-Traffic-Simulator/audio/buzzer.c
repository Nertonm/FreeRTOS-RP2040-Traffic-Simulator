/**
 * @file buzzer.c
 * @author
 * @date 2026-03-01
 * @brief Implementação do driver de áudio para controle de buzzer via PWM.
 *
 * Gerencia a geração de frequências audíveis utilizando o periférico PWM
 * do RP2040, com suporte a desligamento automático via alarmes do sistema.
 */

#include "buzzer.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware_config.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/** @brief Número do slice PWM associado ao pino do buzzer. */
static uint slice_num;

/**
 * @brief Callback do alarme do sistema para desligar o buzzer.
 *
 * Esta função é chamada automaticamente pelo temporizador de hardware
 * após o término da duração de um tom.
 *
 * @param[in] id ID do alarme (gerado pelo sistema).
 * @param[in] user_data Dados de usuário (não utilizados).
 * @return int64_t Retorna 0 para indicar que o alarme não deve ser repetido.
 */
static int64_t buzzer_off_callback(alarm_id_t id, void *user_data) {
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0); // Define duty cycle como 0
  return 0;                                     // Não repete o alarme
}

void buzzer_init(void) {
  // Configura o pino como função PWM
  gpio_set_function(BUZZER_A_PIN, GPIO_FUNC_PWM);

  // Obtém o número do slice PWM para o pino configurado
  slice_num = pwm_gpio_to_slice_num(BUZZER_A_PIN);

  // Inicializa o PWM com nível 0 (desligado)
  pwm_set_clkdiv(slice_num, 1.0f);
  pwm_set_wrap(slice_num, 65535);
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
  pwm_set_enabled(slice_num, true);
}

void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms) {
  if (freq_hz == 0)
    return;

  // Calcula o valor de wrap para a frequência desejada baseada no clock do sistema
  // Fórmula: freq = clock / (wrap + 1)
  uint32_t clock_hz = clock_get_hz(clk_sys);
  uint32_t wrap = (clock_hz / freq_hz) - 1;

  // Se o wrap for maior que 16 bits (65535), ajusta o divisor de clock (clkdiv)
  float div = 1.0f;
  if (wrap > 65535) {
    div = (float)wrap / 65535.0f;
    wrap = 65535;
  }

  pwm_set_clkdiv(slice_num, div);
  pwm_set_wrap(slice_num, wrap);

  // Define um duty cycle de aproximadamente 5% para emitir som sem distorção excessiva
  pwm_set_chan_level(slice_num, PWM_CHAN_B, wrap / 20);

  // Agenda o desligamento do som através de um alarme não bloqueante
  add_alarm_in_ms(duration_ms, buzzer_off_callback, NULL, false);
}
