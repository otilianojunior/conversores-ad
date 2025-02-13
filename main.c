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
 *   - LED RGB: GPIO 11 (Vermelho), GPIO 12 (Verde) e GPIO 13 (Azul)
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
 #include "pico/bootrom.h"  // Para o reset USB boot
 #include "include/ssd1306.h"
 #include "include/font.h"
 
 //==================== Defines e Macros ====================
 #define I2C_PORT        i2c1
 #define I2C_SDA         14
 #define I2C_SCL         15
 #define SSD1306_ADDR    0x3C
 
 // Pinos do Joystick e Botões
 #define JOYSTICK_X_PIN  26   // ADC: canal 0 (GPIO26)
 #define JOYSTICK_Y_PIN  27   // ADC: canal 1 (GPIO27)
 #define JOYSTICK_PB     22   // Botão do Joystick
 #define BOTAO_A         5    // Botão A
 
 // Pinos dos LEDs RGB
 #define LED_RED         13
 #define LED_GREEN       11
 #define LED_BLUE        12
 
 // Botão B para o modo BOOTSEL
 #define BOOTSEL_BUTTON  6
 
 // Tempo de debounce (ms)
 #define DEBOUNCE_DELAY_MS 200
 
 // PWM: definindo o valor de wrap (12 bits para ficar compatível com ADC 0-4095)
 #define PWM_WRAP 4095
 
 //==================== Variáveis Globais Voláteis ====================
 // Flag para ativar/desativar os PWM dos LEDs (controlado pelo Botão A)
 volatile bool pwm_enabled = true;
 
 // Estado do LED Verde (controlado pelo botão do joystick)
 volatile bool green_led_on = false;
 
 // Variável para alternar o estilo da borda do display (0 ou 1)
 volatile uint8_t border_style = 0;
 
 // Variáveis para debouncing dos botões (armazenam o instante do último acionamento)
 static absolute_time_t last_interrupt_time_joystick = {0};
 static absolute_time_t last_interrupt_time_buttonA = {0};
 
 //==================== Rotina de Interrupção para os Botões ====================
 /*
  * Callback global de interrupção para os botões:
  * - Se o botão BOOTSEL (GPIO6) for pressionado, entra em modo boot.
  * - Se o botão do joystick (GPIO22) for pressionado, alterna o LED Verde e o estilo da borda.
  * - Se o botão A (GPIO5) for pressionado, ativa/desativa os PWM dos LEDs.
  */
 void gpio_callback(uint gpio, uint32_t events) {
     absolute_time_t now = get_absolute_time();
 
     if (gpio == BOOTSEL_BUTTON) {
         // Botão B: entra em modo BOOTSEL para reprogramação
         reset_usb_boot(0, 0);
     }
     else if (gpio == JOYSTICK_PB) {
         // Debouncing para o botão do joystick
         if (absolute_time_diff_us(last_interrupt_time_joystick, now) < DEBOUNCE_DELAY_MS * 1000)
             return;
         last_interrupt_time_joystick = now;
 
         // Alterna o estado do LED Verde
         green_led_on = !green_led_on;
         // Alterna o estilo da borda do display (0 <-> 1)
         border_style = (border_style + 1) % 2;
     }
     else if (gpio == BOTAO_A) {
         // Debouncing para o botão A
         if (absolute_time_diff_us(last_interrupt_time_buttonA, now) < DEBOUNCE_DELAY_MS * 1000)
             return;
         last_interrupt_time_buttonA = now;
 
         // Alterna a ativação dos PWM para os LEDs
         pwm_enabled = !pwm_enabled;
     }
 }
 
 //==================== Função Principal ====================
 int main() {
     // Inicializa as funções básicas da SDK
     stdio_init_all();
 
     // -------------- Configuração dos Botões --------------
     // Botão B para BOOTSEL (GPIO6)
     gpio_init(BOOTSEL_BUTTON);
     gpio_set_dir(BOOTSEL_BUTTON, GPIO_IN);
     gpio_pull_up(BOOTSEL_BUTTON);
 
     // Botão do Joystick (GPIO22)
     gpio_init(JOYSTICK_PB);
     gpio_set_dir(JOYSTICK_PB, GPIO_IN);
     gpio_pull_up(JOYSTICK_PB);
 
     // Botão A (GPIO5)
     gpio_init(BOTAO_A);
     gpio_set_dir(BOTAO_A, GPIO_IN);
     gpio_pull_up(BOTAO_A);
 
     /* Registra o callback de interrupção global para os botões.
        Ao chamar gpio_set_irq_enabled_with_callback() para o primeiro pino,
        definimos a função callback que será chamada para todas as interrupções. */
     gpio_set_irq_enabled_with_callback(BOOTSEL_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
     gpio_set_irq_enabled(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true);
     gpio_set_irq_enabled(BOTAO_A, GPIO_IRQ_EDGE_FALL, true);
 
     // -------------- Configuração do I2C para o Display SSD1306 --------------
     i2c_init(I2C_PORT, 400 * 1000);  // 400 kHz
     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA);
     gpio_pull_up(I2C_SCL);
 
     // Inicializa e configura o display SSD1306
     ssd1306_t ssd;
     ssd1306_init(&ssd, WIDTH, HEIGHT, false, SSD1306_ADDR, I2C_PORT);
     ssd1306_config(&ssd);
     ssd1306_fill(&ssd, false);
     ssd1306_send_data(&ssd);
 
     // -------------- Inicialização do ADC para o Joystick --------------
     adc_init();
     adc_gpio_init(JOYSTICK_X_PIN);  // ADC canal 0
     adc_gpio_init(JOYSTICK_Y_PIN);  // ADC canal 1
 
     // -------------- Inicialização do PWM para os LEDs RGB --------------
     // LED Vermelho (GPIO11)
     gpio_set_function(LED_RED, GPIO_FUNC_PWM);
     uint slice_red = pwm_gpio_to_slice_num(LED_RED);
     pwm_set_wrap(slice_red, PWM_WRAP);
     pwm_set_gpio_level(LED_RED, 0);
     pwm_set_enabled(slice_red, true);
 
     // LED Verde (GPIO12)
     gpio_set_function(LED_GREEN, GPIO_FUNC_PWM);
     uint slice_green = pwm_gpio_to_slice_num(LED_GREEN);
     pwm_set_wrap(slice_green, PWM_WRAP);
     pwm_set_gpio_level(LED_GREEN, 0);
     pwm_set_enabled(slice_green, true);
 
     // LED Azul (GPIO13)
     gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);
     uint slice_blue = pwm_gpio_to_slice_num(LED_BLUE);
     pwm_set_wrap(slice_blue, PWM_WRAP);
     pwm_set_gpio_level(LED_BLUE, 0);
     pwm_set_enabled(slice_blue, true);
 
     // Variáveis para armazenar as leituras do ADC
     uint16_t adc_value_x, adc_value_y;
 
     // Variáveis para os cálculos do PWM
     uint32_t red_duty, blue_duty, green_duty;
 
     // Variáveis para a posição do quadrado no display
     // O quadrado tem 8x8 pixels e o display é 128x64
     uint8_t square_x, square_y;
 
     // Loop principal
     while (true) {
         // ------------ Leitura do Joystick via ADC ------------
         // Seleciona o canal 0 para o eixo X (GPIO26)
         adc_select_input(0);
         adc_value_x = adc_read();
 
         // Seleciona o canal 1 para o eixo Y (GPIO27)
         adc_select_input(1);
         adc_value_y = adc_read();
 
         // ------------ Cálculo dos PWM para os LEDs ------------
         // Para os LEDs Vermelho e Azul: o brilho é proporcional à distância do valor central (2048)
         uint16_t diff_x = (adc_value_x >= 2048) ? (adc_value_x - 2048) : (2048 - adc_value_x);
         uint16_t diff_y = (adc_value_y >= 2048) ? (adc_value_y - 2048) : (2048 - adc_value_y);
 
         // Mapeia o desvio para o valor de duty cycle (0 a PWM_WRAP)
         red_duty  = ((uint32_t)diff_x * PWM_WRAP) / 2048;
         blue_duty = ((uint32_t)diff_y * PWM_WRAP) / 2048;
 
         // Para o LED Verde: se estiver ligado, fica em brilho máximo
         green_duty = green_led_on ? PWM_WRAP : 0;
 
         // Se os PWM estiverem desativados (botão A), os LEDs permanecem apagados
         if (!pwm_enabled) {
             red_duty = blue_duty = green_duty = 0;
         }
 
         // Atualiza os níveis PWM nos respectivos pinos
         pwm_set_gpio_level(LED_RED, red_duty);
         pwm_set_gpio_level(LED_GREEN, green_duty);
         pwm_set_gpio_level(LED_BLUE, blue_duty);
 
         // ------------ Atualização do Display SSD1306 ------------
         // Mapeia os valores do ADC para a posição do quadrado (8x8) no display 128x64.
         // Considera que (0,0) é o canto superior esquerdo.
         square_x = (adc_value_x * (128 - 8)) / 4095;
         square_y = (adc_value_y * (64 - 8)) / 4095;
 
         // Limpa o buffer do display
         ssd1306_fill(&ssd, false);
 
         // Desenha a borda do display conforme o estilo selecionado
         if (border_style == 0) {
             // Estilo 0: borda simples ao redor de todo o display
             ssd1306_rect(&ssd, 0, 0, 128, 64, 1, false);
         } else {
             // Estilo 1: borda dupla (uma externa e outra interna)
             ssd1306_rect(&ssd, 0, 0, 128, 64, 1, false);
             ssd1306_rect(&ssd, 2, 2, 124, 60, 1, false);
         }
 
         // Desenha o quadrado de 8x8 pixels na posição mapeada
         ssd1306_rect(&ssd, square_x, square_y, 8, 8, 1, true);
 
         // (Opcional) Pode-se exibir informações adicionais, por exemplo, o estado do PWM
         // Ex: ssd1306_draw_string(&ssd, pwm_enabled ? "PWM ON" : "PWM OFF", 30, 54);
 
         // Atualiza o display com os dados do buffer
         ssd1306_send_data(&ssd);
 
         // Pequeno delay para estabilizar a leitura e a atualização do display
         sleep_ms(50);
     }
     
     return 0;
 }
 