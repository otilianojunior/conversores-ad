/*
 * Projeto: Controle de LEDs RGB com Joystick e Display SSD1306
 *
 * Objetivos:
 *  - Ler os valores analógicos do joystick (eixos X e Y) usando o ADC do RP2040.
 *  - Controlar os LEDs RGB via PWM:
 *       * LED Azul: brilho proporcional ao eixo Y (0 nos 2048 centrais, máximo nos extremos).
 *       * LED Vermelho: brilho proporcional ao eixo X (0 nos 2048 centrais, máximo nos extremos).
 *       * LED Verde: estado alternado a cada acionamento do botão do joystick.
 *  - Exibir no display SSD1306 (128x64) um quadrado 8x8 que se move proporcionalmente ao joystick.
 *  - Alterar o estilo da borda do display a cada acionamento do botão do joystick.
 *  - Com o botão A alternar a ativação dos PWM para os LEDs.
 *  - Utilizar interrupções (com debouncing) para os botões.
 *
 * Componentes conectados:
 *   - LED RGB: GPIO 11 (Verde), GPIO 12 (Azul) e GPIO 13 (Vermelho)
 *   - Joystick analógico: eixo X na GPIO 26 e eixo Y na GPIO 27
 *   - Botão do Joystick: GPIO 22
 *   - Botão A: GPIO 5
 *   - Display SSD1306: I2C nos pinos GPIO 14 (SDA) e GPIO 15 (SCL)
 *   - Botão B (para modo BOOTSEL): GPIO 6
 *
 * Autor: Seu Nome
 *
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include "pico/stdlib.h"
 #include "hardware/adc.h"
 #include "hardware/i2c.h"
 #include "hardware/pwm.h"
 #include "hardware/irq.h"
 #include "pico/bootrom.h" // Para o reset USB boot
 #include "include/ssd1306.h"
 #include "include/font.h"
 
 //==================== Defines e Macros ====================
 #define I2C_PORT       i2c1
 #define I2C_SDA        14
 #define I2C_SCL        15
 #define SSD1306_ADDR   0x3C
 #define WIDTH          128
 #define HEIGHT         64
 
 // Pinos do Joystick e Botões
 #define JOYSTICK_X_PIN 27  // ADC: canal 0 (GPIO26)
 #define JOYSTICK_Y_PIN 26  // ADC: canal 1 (GPIO27)
 #define JOYSTICK_PB    22  // Botão do Joystick
 #define BOTAO_A        5   // Botão A
 
 // Pinos dos LEDs RGB
 #define LED_GREEN      11  // LED Verde
 #define LED_BLUE       12  // LED Azul
 #define LED_RED        13  // LED Vermelho
 
 // Botão B para o modo BOOTSEL
 #define BOOTSEL_BUTTON 6
 
 // Tempo de debounce (ms)
 #define DEBOUNCE_DELAY_MS 200
 
 // PWM: valor de wrap (12 bits, compatível com ADC 0-4095)
 #define PWM_WRAP 4095
 
 //==================== Variáveis Globais Voláteis ====================
 volatile bool pwm_enabled = true;    // Ativa/desativa os PWM (controlado pelo Botão A)
 volatile bool green_led_on = false;    // Estado do LED Verde (alternado pelo botão do joystick)
 volatile uint8_t border_style = 0;     // Alterna o estilo da borda do display (0 ou 1)
 
 // Variáveis para debouncing dos botões
 static absolute_time_t last_interrupt_time_joystick = {0};
 static absolute_time_t last_interrupt_time_buttonA = {0};
 
 //==================== Rotina de Interrupção para os Botões ====================
 void gpio_callback(uint gpio, uint32_t events) {
     absolute_time_t now = get_absolute_time();
 
     if (gpio == BOOTSEL_BUTTON) {
         // Botão B: entra em modo BOOTSEL para reprogramação
         reset_usb_boot(0, 0);
     } else if (gpio == JOYSTICK_PB) {
         // Debounce para o botão do joystick
         if (absolute_time_diff_us(last_interrupt_time_joystick, now) < DEBOUNCE_DELAY_MS * 1000)
             return;
         last_interrupt_time_joystick = now;
 
         // Alterna o estado do LED Verde e o estilo da borda
         green_led_on = !green_led_on;
         border_style = (border_style + 1) % 2;
     } else if (gpio == BOTAO_A) {
         // Debounce para o botão A
         if (absolute_time_diff_us(last_interrupt_time_buttonA, now) < DEBOUNCE_DELAY_MS * 1000)
             return;
         last_interrupt_time_buttonA = now;
 
         // Alterna a ativação dos PWM dos LEDs
         pwm_enabled = !pwm_enabled;
     }
 }
 
 //==================== Função Principal =====================
 int main() {
     stdio_init_all();
 
     // -------------- Configuração dos Botões --------------
     gpio_init(BOOTSEL_BUTTON);
     gpio_set_dir(BOOTSEL_BUTTON, GPIO_IN);
     gpio_pull_up(BOOTSEL_BUTTON);
 
     gpio_init(JOYSTICK_PB);
     gpio_set_dir(JOYSTICK_PB, GPIO_IN);
     gpio_pull_up(JOYSTICK_PB);
 
     gpio_init(BOTAO_A);
     gpio_set_dir(BOTAO_A, GPIO_IN);
     gpio_pull_up(BOTAO_A);
 
     gpio_set_irq_enabled_with_callback(BOOTSEL_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
     gpio_set_irq_enabled(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true);
     gpio_set_irq_enabled(BOTAO_A, GPIO_IRQ_EDGE_FALL, true);
 
     // -------------- Configuração do I2C para o Display SSD1306 --------------
     i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA);
     gpio_pull_up(I2C_SCL);
 
     ssd1306_t ssd;
     ssd1306_init(&ssd, WIDTH, HEIGHT, false, SSD1306_ADDR, I2C_PORT);
     ssd1306_config(&ssd);
     ssd1306_fill(&ssd, false);
     ssd1306_send_data(&ssd);
 
     // -------------- Inicialização do ADC para o Joystick --------------
     adc_init();
     adc_gpio_init(JOYSTICK_X_PIN);
     adc_gpio_init(JOYSTICK_Y_PIN);
 
     // -------------- Inicialização do PWM para os LEDs RGB --------------
     // LED Verde (GPIO11)
     gpio_set_function(LED_GREEN, GPIO_FUNC_PWM);
     uint slice_green = pwm_gpio_to_slice_num(LED_GREEN);
     pwm_set_wrap(slice_green, PWM_WRAP);
     pwm_set_gpio_level(LED_GREEN, 0);
     pwm_set_enabled(slice_green, true);
 
     // LED Azul (GPIO12)
     gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);
     uint slice_blue = pwm_gpio_to_slice_num(LED_BLUE);
     pwm_set_wrap(slice_blue, PWM_WRAP);
     pwm_set_gpio_level(LED_BLUE, 0);
     pwm_set_enabled(slice_blue, true);
 
     // LED Vermelho (GPIO13)
     gpio_set_function(LED_RED, GPIO_FUNC_PWM);
     uint slice_red = pwm_gpio_to_slice_num(LED_RED);
     pwm_set_wrap(slice_red, PWM_WRAP);
     pwm_set_gpio_level(LED_RED, 0);
     pwm_set_enabled(slice_red, true);
 
     // Variáveis para leituras do ADC e PWM
     uint16_t adc_value_x, adc_value_y;
     uint32_t red_duty, blue_duty, green_duty;
 
     // Para desenhar o ponteiro: quadrado de 8x8 pixels
     int square_width = 8, square_height = 8;
     int square_x, square_y;
 
     /*
      * Para centralizar o quadrado, mapeamos os valores do ADC para a resolução do display
      * e subtraímos metade do tamanho do quadrado. Assim, quando o ADC ler 2047 (centro),
      * teremos:
      *   pos_center_x ≈ (2047 * 128) / 4095 ≈ 64
      *   pos_center_y ≈ (2047 * 64)  / 4095 ≈ 32
      * E o quadrado será desenhado com seu canto superior esquerdo em:
      *   square_x = 64 - 4 = 60
      *   square_y = 32 - 4 = 28
      */
 
     while (true) {
         // ------------ Leitura do Joystick via ADC ------------
         adc_select_input(0);
         adc_value_x = adc_read();
         adc_select_input(1);
         adc_value_y = adc_read();
 
         // ------------ Cálculo dos PWM para os LEDs ------------
         // LED Vermelho (eixo X)
         uint16_t diff_x = (adc_value_x >= 2048) ? (adc_value_x - 2048) : (2048 - adc_value_x);
         // LED Azul (eixo Y)
         uint16_t diff_y = (adc_value_y >= 2048) ? (adc_value_y - 2048) : (2048 - adc_value_y);
 
         red_duty  = ((uint32_t)diff_x * PWM_WRAP) / 2048;
         blue_duty = ((uint32_t)diff_y * PWM_WRAP) / 2048;
         green_duty = green_led_on ? PWM_WRAP : 0;
 
         if (!pwm_enabled) {
             red_duty = blue_duty = green_duty = 0;
         }
 
         pwm_set_gpio_level(LED_RED, red_duty);
         pwm_set_gpio_level(LED_GREEN, green_duty);
         pwm_set_gpio_level(LED_BLUE, blue_duty);
 
         // ------------ Atualização do Display SSD1306 ------------
         // Mapeia os valores do ADC para a posição central do quadrado no display.
         int pos_center_x = ((adc_value_x * (WIDTH - square_width)) / 4095) + (square_width / 2);
         int pos_center_y = ((adc_value_y * (HEIGHT - square_height)) / 4095) + (square_height / 2);
 
         // Calcula a posição do canto superior esquerdo do quadrado
         square_x = pos_center_x - (square_width / 2);
         square_y = pos_center_y - (square_height / 2);
 
         // Clamp para manter o quadrado dentro dos limites do display
         if (square_x < 0) square_x = 0;
         if (square_x > WIDTH - square_width) square_x = WIDTH - square_width;
         if (square_y < 0) square_y = 0;
         if (square_y > HEIGHT - square_height) square_y = HEIGHT - square_height;
 
         ssd1306_fill(&ssd, false);
         if (border_style == 0) {
             ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, 1, false);
         } else {
             ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, 1, false);
             ssd1306_rect(&ssd, 2, 2, WIDTH - 4, HEIGHT - 4, 1, false);
         }
         ssd1306_rect(&ssd, square_x, square_y, square_width, square_height, 1, true);
         ssd1306_send_data(&ssd);
 
         sleep_ms(50);
     }
 
     return 0;
 }
 