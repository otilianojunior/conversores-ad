<table>
  <tr>
    <td>
      <img src="assets/logo.jpeg" alt="Logo do Projeto" width="150">
    </td>
    <td>
      <h1>💡 Projeto: Conversores na RP2040 - BitDogLab</h1>
    </td>
  </tr>
</table>

## 📋 Descrição Geral

Este projeto tem como objetivo consolidar os conceitos sobre o uso de **conversores analógico-digitais (ADC)** no microcontrolador **RP2040**, além de explorar as funcionalidades da placa de desenvolvimento **BitDogLab**. A proposta inclui a manipulação de um **joystick analógico** para controlar a intensidade de LEDs RGB via **PWM**, bem como a exibição de sua posição em um **display OLED SSD1306** via **I2C**.

---

## 🎯 Objetivos

- **Compreender o funcionamento do ADC** no RP2040 para interpretar sinais analógicos do joystick.
- **Utilizar PWM** para controlar a intensidade de LEDs RGB com base nos valores do joystick.
- **Exibir no display SSD1306** um quadrado móvel que represente a posição do joystick.
- **Aplicar o protocolo I2C** na comunicação com o display OLED.
- **Implementar interrupções (IRQ)** para os botões.
- **Tratar debounce** via software para evitar leituras inconsistentes dos botões.

---

## 🛠 Componentes Utilizados

- **LED RGB (GPIOs: 11, 12 e 13)**: Utilizado para representar os eixos X e Y do joystick.
- **Joystick (GPIOs: 26 e 27 - ADC0 e ADC1)**: Responsável pela entrada analógica de posição.
- **Botão do Joystick (GPIO 22)**: Alterna o LED verde e modifica a borda do display.
- **Botão A (GPIO 5)**: Ativa ou desativa os LEDs controlados por PWM.
- **Display OLED SSD1306 (I2C - GPIOs 14 e 15)**: Exibe a posição do joystick e estilos de borda.

---

## 🗂 Estrutura do Projeto

```plaintext
Joystick_Display/
├── assets/
│   ├── logo.jpeg
│   ├── placa.gif
│   └── wokwi.gif
├── wokwi/
│   ├── diagram.json
│   └── wokwi.toml
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── main.c
└── README.md
```

---

## 🚀 Funcionalidades do Projeto

1. **Controle de LEDs RGB via Joystick:**
   - **LED Azul:** Ajusta o brilho com base no eixo Y do joystick.
   - **LED Vermelho:** Ajusta o brilho com base no eixo X do joystick.
   - **LED Verde:** Ligado/desligado pelo botão do joystick.

2. **Movimentação do quadrado no Display SSD1306:**
   - O quadrado de 8x8 pixels se move conforme os valores do joystick.
   - A borda do display muda de estilo a cada pressionamento do botão do joystick.

3. **Interrupções e Debouncing nos Botões:**
   - **Botão do Joystick:** Alterna o LED verde e modifica a borda do display.
   - **Botão A:** Ativa/desativa os LEDs RGB controlados por PWM.
   - **Tratamento de debounce** implementado para evitar leituras incorretas.

4. **Configuração Inicial do Joystick:**
   - **Verifique as coordenadas do seu joystick na primeira utilização** e ajuste os valores no código para garantir um funcionamento adequado.

   ```c
   #define CENTRO_X_JOYSTICK 1922
   #define CENTRO_Y_JOYSTICK 2025
   ```

> _Observação:_ O diagrama original da matriz de LEDs foi adaptado a partir do repositório do professor [Wilton Lacerda Silva](https://github.com/wiltonlacerda) e modificado para esta atividade.

---

## 🔧 Requisitos Técnicos

- **Interrupções:** Implementadas para os botões.
- **Debouncing:** Evita acionamentos múltiplos inesperados.
- **Comunicação I2C:** Utilizada para conectar o display OLED SSD1306.
- **ADC:** Leitura dos valores do joystick para controle do movimento e intensidade luminosa.
- **PWM:** Controle de brilho suave dos LEDs RGB.
- **Código Bem Estruturado:** Organizado e comentado para fácil entendimento.

---

## ⚙️ Instalação e Execução

### 1. Configuração do Ambiente

- Certifique-se de que o **Pico SDK** está instalado corretamente.
- Instale dependências necessárias para compilação.

### 2. Clonando o Repositório

```bash
git clone https://github.com/otilianojunior/conversores-ad.git
```

### 3. Compilação e Envio do Código

```bash
mkdir build
cd build
cmake ..
make
```

Após a compilação, copie o arquivo `.uf2` gerado para o Raspberry Pi Pico (modo bootloader ativado).

### 4. Testes

- **Simulação no Wokwi:**  
  <p align="center">
    <img src="assets/wokwi.gif" width="50%">
  </p>

- **Execução na Placa BitDogLab:**  
  Grave o código no RP2040 e interaja com os botões e joystick.  
  <p align="center">
    <img src="assets/placa.gif">
  </p>

---

## 📁 Entregáveis

- Código-fonte presente neste repositório.
- Vídeo demonstrativo: [Vídeo](https://drive.google.com/file/d/1ffY0v1yrHNm2p_ohVFK9X0PiHqzneg3u/view?usp=sharing)

---

## ✅ Conclusão

Este projeto proporciona um excelente aprendizado sobre **ADC, PWM, I2C e interrupções** no RP2040, permitindo a integração entre hardware e software de forma interativa e funcional.

---

_Desenvolvido por Otiliano Júnior_