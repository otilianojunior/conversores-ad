#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "pico/bootrom.h"
#include "ssd1306.h"

// ==================== Definições ====================
#define PORTA_I2C i2c1
#define SDA_I2C 14
#define SCL_I2C 15
#define ENDERECO_SSD1306 0x3C
#define LARGURA 128
#define ALTURA 64

// Definições do Joystick e Botões
#define PINO_X_JOYSTICK 26
#define PINO_Y_JOYSTICK 27 
#define BOTAO_JOYSTICK 22  
#define BOTAO_A 5         
#define BOTAO_B 6  

// LEDs RGB (usados via PWM)
#define LED_VERDE 11
#define LED_AZUL 12
#define LED_VERMELHO 13

// Calibração do joystick e zona morta
#define CENTRO_X_JOYSTICK 1922
#define CENTRO_Y_JOYSTICK 2025
#define ZONA_MORTA 60

// Valor de wrap do PWM (12 bits)
#define PWM_WRAP 4095

// Tempo de debounce (ms)
#define ATRASO_DEBOUNCE_MS 200

// ==================== Variáveis Globais ====================
volatile bool pwm_ativado = true;       // Habilita/desabilita os PWM (botão A)
volatile bool led_verde_ligado = false;   // Estado do LED verde (toggle pelo botão do joystick)
volatile int estilo_borda = 1;            // 1, 2 ou 3 para alternar o estilo da borda

// Variáveis para debounce via interrupção
static absolute_time_t ultimo_tempo_interrupcao_joystick = {0};
static absolute_time_t ultimo_tempo_interrupcao_botaoA = {0};
static absolute_time_t ultimo_tempo_interrupcao_botaoB = {0};

// Objeto do display OLED
ssd1306_t oled;

// ==================== Rotina de Interrupção ====================
void callback_gpio(uint pino, uint32_t eventos)
{
    absolute_time_t agora = get_absolute_time();

    if (pino == BOTAO_B)
    {
        if (absolute_time_diff_us(ultimo_tempo_interrupcao_botaoB, agora) < ATRASO_DEBOUNCE_MS * 1000)
            return;
        ultimo_tempo_interrupcao_botaoB = agora;
        printf("[SISTEMA] Entrando em modo BOOTSEL\n");
        reset_usb_boot(0, 0);
    }
    else if (pino == BOTAO_JOYSTICK)
    {
        if (absolute_time_diff_us(ultimo_tempo_interrupcao_joystick, agora) < ATRASO_DEBOUNCE_MS * 1000)
            return;
        ultimo_tempo_interrupcao_joystick = agora;
        led_verde_ligado = !led_verde_ligado;
        estilo_borda = (estilo_borda % 3) + 1;
        printf("[BOTÃO] Bordas: %d | LED Verde: %s\n", estilo_borda, led_verde_ligado ? "Ligado" : "Desligado");
    }
    else if (pino == BOTAO_A)
    {
        if (absolute_time_diff_us(ultimo_tempo_interrupcao_botaoA, agora) < ATRASO_DEBOUNCE_MS * 1000)
            return;
        ultimo_tempo_interrupcao_botaoA = agora;
        pwm_ativado = !pwm_ativado;
        printf("[PWM] Estado: %s\n", pwm_ativado ? "Ativado" : "Desativado");
    }
}

// ==================== Função Principal ====================
int main()
{
    stdio_init_all();

    // --------- Configuração dos Botões ---------
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_init(BOTAO_JOYSTICK);
    gpio_set_dir(BOTAO_JOYSTICK, GPIO_IN);
    gpio_pull_up(BOTAO_JOYSTICK);

    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    // Registra a rotina de interrupção (callback) para os três botões
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &callback_gpio);
    gpio_set_irq_enabled(BOTAO_JOYSTICK, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BOTAO_A, GPIO_IRQ_EDGE_FALL, true);

    // --------- Configuração do I2C e Display SSD1306 ---------
    i2c_init(PORTA_I2C, 400 * 1000); // 400 kHz
    gpio_set_function(SDA_I2C, GPIO_FUNC_I2C);
    gpio_set_function(SCL_I2C, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_I2C);
    gpio_pull_up(SCL_I2C);

    ssd1306_init(&oled, LARGURA, ALTURA, false, ENDERECO_SSD1306, PORTA_I2C);
    ssd1306_config(&oled);
    ssd1306_fill(&oled, false);
    ssd1306_send_data(&oled);

    // --------- Inicialização do ADC para o Joystick ---------
    adc_init();
    adc_gpio_init(PINO_X_JOYSTICK); // ADC0 para eixo X
    adc_gpio_init(PINO_Y_JOYSTICK); // ADC1 para eixo Y

    // --------- Configuração do PWM para os LEDs RGB ---------
    // LED Vermelho
    gpio_set_function(LED_VERMELHO, GPIO_FUNC_PWM);
    uint slice_vermelho = pwm_gpio_to_slice_num(LED_VERMELHO);
    pwm_set_wrap(slice_vermelho, PWM_WRAP);
    pwm_set_gpio_level(LED_VERMELHO, 0);
    pwm_set_enabled(slice_vermelho, true);

    // LED Azul
    gpio_set_function(LED_AZUL, GPIO_FUNC_PWM);
    uint slice_azul = pwm_gpio_to_slice_num(LED_AZUL);
    pwm_set_wrap(slice_azul, PWM_WRAP);
    pwm_set_gpio_level(LED_AZUL, 0);
    pwm_set_enabled(slice_azul, true);

    // LED Verde
    gpio_set_function(LED_VERDE, GPIO_FUNC_PWM);
    uint slice_verde = pwm_gpio_to_slice_num(LED_VERDE);
    pwm_set_wrap(slice_verde, PWM_WRAP);
    pwm_set_gpio_level(LED_VERDE, 0);
    pwm_set_enabled(slice_verde, true);

    // --------- Variáveis para posicionamento do quadrado (8x8) ---------
    // Posições iniciais para o quadrado
    const int pos_inicial_x = 59; // eixo vertical (pos_x)
    const int pos_inicial_y = 29; // eixo horizontal (pos_y)
    int pos_x = pos_inicial_x;
    int pos_y = pos_inicial_y;

    while (true)
    {
        // Leitura dos valores ADC do joystick
        adc_select_input(0); // Eixo X
        uint16_t valor_x = adc_read();
        adc_select_input(1); // Eixo Y
        uint16_t valor_y = adc_read();

        // Calcula os desvios a partir do centro (calibração)
        int ajustado_x = valor_x - CENTRO_X_JOYSTICK;
        int ajustado_y = valor_y - CENTRO_Y_JOYSTICK;

        // Atualiza a posição (movimento incremental) considerando a zona morta
        // Eixo vertical (influenciado pelo valor de ajustado_y):
        if (abs(ajustado_y) > ZONA_MORTA)
        {
            pos_x += (ajustado_y * 5) / 2048;
        }
        else
        {
            // Se o joystick estiver "solto", retorna gradualmente à posição inicial
            if (pos_x < pos_inicial_x)
                pos_x++;
            else if (pos_x > pos_inicial_x)
                pos_x--;
        }

        // Eixo horizontal (influenciado pelo valor de ajustado_x):
        if (abs(ajustado_x) > ZONA_MORTA)
        {
            pos_y -= (ajustado_x * 5) / 2048;
        }
        else
        {
            // Retorna gradualmente à posição inicial
            if (pos_y < pos_inicial_y)
                pos_y++;
            else if (pos_y > pos_inicial_y)
                pos_y--;
        }

        // Garante que o quadrado permaneça dentro dos limites do display (8x8)
        if (pos_x < 0)
            pos_x = 0;
        if (pos_x > LARGURA - 8)
            pos_x = LARGURA - 8;
        if (pos_y < 0)
            pos_y = 0;
        if (pos_y > ALTURA - 8)
            pos_y = ALTURA - 8;

        // Para o desenho, inverte-se as coordenadas (troca os eixos)
        int disp_x = pos_y; // posição horizontal
        int disp_y = pos_x; // posição vertical

        // Imprime os valores do joystick e a posição calculada
        printf("[JOYSTICK] X: %4d | Y: %4d | Pos: (%3d, %3d)\n", valor_x, valor_y, disp_x, disp_y);

        // --------- Atualiza os níveis dos LEDs via PWM ---------
        if (pwm_ativado)
        {
            // Calcula a intensidade considerando a zona morta:
            uint32_t valor_y_pwm = (abs(ajustado_x) > ZONA_MORTA) ? (abs(ajustado_x) - ZONA_MORTA) : 0;
            uint32_t valor_x_pwm = (abs(ajustado_y) > ZONA_MORTA) ? (abs(ajustado_y) - ZONA_MORTA) : 0;
            // Define o intervalo máximo efetivo (para mapeamento linear)
            uint32_t max_range = 2048 - ZONA_MORTA;

            // LED Vermelho: intensidade proporcional ao desvio horizontal (eixo X)
            uint32_t duty_vermelho = (valor_x_pwm * PWM_WRAP) / max_range;
            // LED Azul: intensidade proporcional ao desvio vertical (eixo Y)
            uint32_t duty_azul = (valor_y_pwm * PWM_WRAP) / max_range;
            // LED Verde: totalmente aceso se estiver ligado (toggle)
            uint32_t duty_verde = led_verde_ligado ? PWM_WRAP : 0;

            pwm_set_gpio_level(LED_VERMELHO, duty_vermelho);
            pwm_set_gpio_level(LED_AZUL, duty_azul);
            pwm_set_gpio_level(LED_VERDE, duty_verde);
        }
        else
        {
            pwm_set_gpio_level(LED_VERMELHO, 0);
            pwm_set_gpio_level(LED_AZUL, 0);
            pwm_set_gpio_level(LED_VERDE, 0);
        }

        // --------- Atualiza o display OLED ---------
        ssd1306_fill(&oled, false);
        // Desenha as bordas conforme o estilo selecionado
        if (estilo_borda == 1)
        {
            ssd1306_rect(&oled, 0, 0, LARGURA, ALTURA, 1, false);
        }
        else if (estilo_borda == 2)
        {
            ssd1306_rect(&oled, 0, 0, LARGURA, ALTURA, 1, false);
            ssd1306_rect(&oled, 1, 1, LARGURA - 2, ALTURA - 2, 1, false);
            ssd1306_rect(&oled, 2, 2, LARGURA - 4, ALTURA - 4, 1, false);
        } else if (estilo_borda == 3) {
            // Parâmetros do traço
            int dash = 4; // tamanho do traço (em pixels)
            int gap  = 2; // tamanho do intervalo entre traços
        
            // Borda superior e inferior
            for (int x = 0; x < LARGURA; x += dash + gap) {
                // Borda superior
                int end_x = (x + dash < LARGURA) ? x + dash : LARGURA;
                for (int i = x; i < end_x; i++) {
                    ssd1306_pixel(&oled, i, 0, 1);
                }
                // Borda inferior
                end_x = (x + dash < LARGURA) ? x + dash : LARGURA;
                for (int i = x; i < end_x; i++) {
                    ssd1306_pixel(&oled, i, ALTURA - 1, 1);
                }
            }
        
            // Borda esquerda e direita
            for (int y = 0; y < ALTURA; y += dash + gap) {
                int end_y = (y + dash < ALTURA) ? y + dash : ALTURA;
                for (int j = y; j < end_y; j++) {
                    ssd1306_pixel(&oled, 0, j, 1);
                    ssd1306_pixel(&oled, LARGURA - 1, j, 1);
                }
            }
        }

        // Desenha o quadrado representando a posição do joystick
        ssd1306_rect(&oled, disp_x, disp_y, 8, 8, 1, true);
        ssd1306_send_data(&oled);

        sleep_ms(20);
    }

    return 0;
}
