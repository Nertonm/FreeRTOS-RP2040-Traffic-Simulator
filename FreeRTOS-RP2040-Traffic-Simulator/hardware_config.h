/**
 * @file hardware_config.h
 * @brief Definições de pinagem e configuração de hardware para o RP2040.
 *
 * Mapeia todos os periféricos utilizados (LEDs, Botões, Display, Buzzer,
 * Sensores) para os pinos GPIO correspondentes do microcontrolador.
 *
 * @author
 * @date 2026-03-01
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/** @name Pinagem de LEDs e Botões */
/** @{ */
#define LED_RED 13         /**< LED Vermelho (Digital). */
#define LED_GREEN 11       /**< LED Verde (Digital). */
#define LED_BLUE 12        /**< LED Azul (Digital). */
#define BUTTON1_PIN 5      /**< Pino do Botão A (Pressionado = LOW). */
#define BUTTON2_PIN 6      /**< Pino do Botão B (Pressionado = LOW). */
#define BUTTONSTICK_PIN 22 /**< Pino do Botão do Joystick. */
/** @} */

/** @name Matriz de LEDs WS2812B 5x5 */
/** @{ */
#define WS2812_PIN 7         /**< Pino de dados da matriz NeoPixel. */
#define WS2812_NUM_LEDS 25   /**< Total de LEDs na matriz. */
#define WS2812_IS_RGBW false /**< Indica se os LEDs possuem canal branco. */
/** @} */

/** @name Buzzer */
/** @{ */
#define BUZZER_A_PIN 21 /**< Pino do Buzzer passivo (PWM). */
/** @} */

/** @name Display OLED (I2C1) */
/** @{ */
#define I2C_PORT i2c1        /**< Instância I2C utilizada. */
#define I2C_SDA 14           /**< Pino SDA do display. */
#define I2C_SCL 15           /**< Pino SCL do display. */
#define OLED_I2C_ADDR 0x3C   /**< Endereço I2C do controlador SSD1306. */
#define OLED_I2C_FREQ 400000 /**< Frequência do barramento (400kHz). */

/* Macros legadas para compatibilidade */
#define OLED_SDA_PIN I2C_SDA
#define OLED_SCL_PIN I2C_SCL
#define OLED_I2C_PORT I2C_PORT
#define LED_RGB_RED_PIN LED_RED
#define LED_RGB_GREEN_PIN LED_GREEN
#define LED_RGB_BLUE_PIN LED_BLUE
#define I2C0_SDA_PIN 0
#define I2C0_SCL_PIN 1
#define I2C0_PORT i2c0
/** @} */

/** @name Joystick Analógico */
/** @{ */
#define JOY_VRX_PIN 27 /**< Eixo X (ADC1). */
#define JOY_VRY_PIN 26 /**< Eixo Y (ADC0). */
#define JOY_SW_PIN 22  /**< Botão do joystick (pull-up interno). */
/** @} */

/** @name Microfone Analógico */
/** @{ */
#define MIC_PIN 28 /**< Saída analógica do microfone (ADC2). */
/** @} */

/** @name Identificação do Nó */
/** @{ */
#ifndef NODE_ID
#define NODE_ID                                                                                    \
  0 /**< Identificador padrão do nó (pode ser sobrescrito via flags do                           \
       compilador). */
#endif
/** @} */

#endif // HARDWARE_CONFIG_H
