# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(conversores-ad C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Adicione o arquivo main.c e o ssd1306.c (localizado na pasta include)
add_executable(conversores-ad 
    main.c
    include/ssd1306.c
)

pico_set_program_name(conversores-ad "conversores-ad")
pico_set_program_version(conversores-ad "0.1")

# Habilita UART/USB conforme necessário
pico_enable_stdio_uart(conversores-ad 1)
pico_enable_stdio_usb(conversores-ad 1)

# Adiciona as bibliotecas padrão do Pico
target_link_libraries(conversores-ad
        hardware_adc
        hardware_i2c
        hardware_pwm
        pico_stdlib
)

# Adiciona o diretório de includes (para que #include "ssd1306.h" funcione corretamente)
target_include_directories(conversores-ad PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(conversores-ad)
