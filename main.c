#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "pico/bootrom.h"
#include "ssd1306.h"    // Ajuste o caminho conforme sua estrutura de pastas

// ==================== Defines ====================
#define I2C_PORT         i2c1
#define I2C_SDA          14
#define I2C_SCL          15
#define SSD1306_ADDR     0x3C
#define WIDTH            128
#define HEIGHT           64

// Definições do Joystick e Botões
// Para o posicionamento correto, use o ADC0 para o eixo X e ADC1 para o eixo Y
#define JOYSTICK_X_PIN   26    // ADC0
#define JOYSTICK_Y_PIN   27    // ADC1
#define JOYSTICK_BTN     22    // Botão do joystick
#define BUTTON_A         5     // Alterna os PWM dos LEDs
#define BUTTON_B         6     // Botão para entrar em modo BOOTSEL

// LEDs RGB (usados via PWM)
#define LED_GREEN        11
#define LED_BLUE         12
#define LED_RED          13

// Calibração do joystick e zona morta
#define JOYSTICK_CENTER_X 1929
#define JOYSTICK_CENTER_Y 2019
#define DEADZONE          100

// Valor de wrap do PWM (12 bits)
#define PWM_WRAP 4095

// Tempo de debounce (ms)
#define DEBOUNCE_DELAY_MS 200

// ==================== Variáveis Globais ====================
volatile bool pwm_enabled = true;         // Habilita/desabilita os PWM (botão A)
volatile bool green_led_on = false;         // Estado do LED verde (toggle pelo botão do joystick)
volatile int border_style = 1;              // 1 ou 2, para alternar o estilo da borda

// Variáveis para debounce via interrupção
static absolute_time_t last_interrupt_time_joystick = {0};
static absolute_time_t last_interrupt_time_buttonA    = {0};
static absolute_time_t last_interrupt_time_buttonB    = {0};

// Objeto do display OLED
ssd1306_t ssd;

// ==================== Rotina de Interrupção ====================
void gpio_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();

    if (gpio == BUTTON_B) {
        if (absolute_time_diff_us(last_interrupt_time_buttonB, now) < DEBOUNCE_DELAY_MS * 1000)
            return;
        last_interrupt_time_buttonB = now;
        printf("[SISTEMA] Entrando em modo BOOTSEL\n");
        reset_usb_boot(0, 0);
    }
    else if (gpio == JOYSTICK_BTN) {
        if (absolute_time_diff_us(last_interrupt_time_joystick, now) < DEBOUNCE_DELAY_MS * 1000)
            return;
        last_interrupt_time_joystick = now;
        green_led_on = !green_led_on;
        border_style = (border_style == 1) ? 2 : 1;
        printf("[BOTÃO] Bordas: %d | LED Verde: %s\n", border_style, green_led_on ? "Ligado" : "Desligado");
    }
    else if (gpio == BUTTON_A) {
        if (absolute_time_diff_us(last_interrupt_time_buttonA, now) < DEBOUNCE_DELAY_MS * 1000)
            return;
        last_interrupt_time_buttonA = now;
        pwm_enabled = !pwm_enabled;
        printf("[PWM] Estado: %s\n", pwm_enabled ? "Ativado" : "Desativado");
    }
}

// ==================== Função Principal ====================
int main() {
    stdio_init_all();

    // --------- Configuração dos Botões ---------
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(JOYSTICK_BTN);
    gpio_set_dir(JOYSTICK_BTN, GPIO_IN);
    gpio_pull_up(JOYSTICK_BTN);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    // Registra a rotina de interrupção (callback) para os três botões
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(JOYSTICK_BTN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_A, GPIO_IRQ_EDGE_FALL, true);

    // --------- Configuração do I2C e Display SSD1306 ---------
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, SSD1306_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // --------- Inicialização do ADC para o Joystick ---------
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN); // ADC0 para eixo X
    adc_gpio_init(JOYSTICK_Y_PIN); // ADC1 para eixo Y

    // --------- Configuração do PWM para os LEDs RGB ---------
    // LED Vermelho
    gpio_set_function(LED_RED, GPIO_FUNC_PWM);
    uint slice_red = pwm_gpio_to_slice_num(LED_RED);
    pwm_set_wrap(slice_red, PWM_WRAP);
    pwm_set_gpio_level(LED_RED, 0);
    pwm_set_enabled(slice_red, true);

    // LED Azul
    gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);
    uint slice_blue = pwm_gpio_to_slice_num(LED_BLUE);
    pwm_set_wrap(slice_blue, PWM_WRAP);
    pwm_set_gpio_level(LED_BLUE, 0);
    pwm_set_enabled(slice_blue, true);

    // LED Verde
    gpio_set_function(LED_GREEN, GPIO_FUNC_PWM);
    uint slice_green = pwm_gpio_to_slice_num(LED_GREEN);
    pwm_set_wrap(slice_green, PWM_WRAP);
    pwm_set_gpio_level(LED_GREEN, 0);
    pwm_set_enabled(slice_green, true);

    // --------- Variáveis para posicionamento do quadrado (8x8) ---------
    int x_pos = 59; // Posição inicial (interna)
    int y_pos = 29;

    while (true) {
        // Leitura dos valores ADC do joystick
        adc_select_input(0); // Eixo X
        uint16_t x_val = adc_read();
        adc_select_input(1); // Eixo Y
        uint16_t y_val = adc_read();

        // Calcula os desvios a partir do centro (calibração)
        int adjusted_x = x_val - JOYSTICK_CENTER_X;
        int adjusted_y = y_val - JOYSTICK_CENTER_Y;

        // Atualiza a posição (movimento incremental) considerando uma zona morta
        if (abs(adjusted_y) > DEADZONE) {
            x_pos += (adjusted_y * 5) / 2048;
        }
        if (abs(adjusted_x) > DEADZONE) {
            y_pos += (adjusted_x * 5) / 2048;
        }

        // Garante que o quadrado permaneça dentro dos limites do display (8x8)
        if (x_pos < 0) x_pos = 0;
        if (x_pos > WIDTH - 8) x_pos = WIDTH - 8;
        if (y_pos < 0) y_pos = 0;
        if (y_pos > HEIGHT - 8) y_pos = HEIGHT - 8;

        // Para o desenho, inverte-se as coordenadas (troca os eixos)
        int disp_x = y_pos;  // posição horizontal
        int disp_y = x_pos;  // posição vertical

        // Imprime os valores do joystick e a posição calculada
        printf("[JOYSTICK] X: %4d | Y: %4d | Pos: (%3d, %3d)\n", x_val, y_val, disp_x, disp_y);

        // --------- Atualiza os níveis dos LEDs via PWM ---------
        if (pwm_enabled) {
            // LED Vermelho: intensidade proporcional ao desvio no eixo X
            uint32_t red_duty = ((uint32_t)(adjusted_x + 2048) * PWM_WRAP) / 4096;
            // LED Azul: intensidade proporcional ao desvio no eixo Y
            uint32_t blue_duty = ((uint32_t)(adjusted_y + 2048) * PWM_WRAP) / 4096;
            // LED Verde: totalmente aceso se estiver ligado (toggle)
            uint32_t green_duty = green_led_on ? PWM_WRAP : 0;

            pwm_set_gpio_level(LED_RED, red_duty);
            pwm_set_gpio_level(LED_BLUE, blue_duty);
            pwm_set_gpio_level(LED_GREEN, green_duty);
        } else {
            pwm_set_gpio_level(LED_RED, 0);
            pwm_set_gpio_level(LED_BLUE, 0);
            pwm_set_gpio_level(LED_GREEN, 0);
        }

        // --------- Atualiza o display OLED ---------
        ssd1306_fill(&ssd, false);
        // Desenha as bordas conforme o estilo selecionado
        if (border_style == 1) {
            ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, 1, false);
        } else if (border_style == 2) {
            ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, 1, false);
            ssd1306_rect(&ssd, 1, 1, WIDTH - 2, HEIGHT - 2, 1, false);
            ssd1306_rect(&ssd, 2, 2, WIDTH - 4, HEIGHT - 4, 1, false);
        }
        // Desenha o quadrado representando a posição do joystick
        ssd1306_rect(&ssd, disp_x, disp_y, 8, 8, 1, true);
        ssd1306_send_data(&ssd);

        sleep_ms(20);
    }

    return 0;
}
