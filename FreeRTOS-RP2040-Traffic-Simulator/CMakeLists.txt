# CMakeLists.txt — FreeRTOS-RP2040-Traffic-Simulator
# Baseado no clicker-rp2040, adaptado para simulador de tráfego distribuído.
# Gera 3 binários: norte.uf2 (NODE_ID=0), centro.uf2 (NODE_ID=1), sul.uf2 (NODE_ID=2)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Evita kit/cache de host gcc quando o projeto exige toolchain ARM
if (CMAKE_C_COMPILER MATCHES "ccache/bin/gcc$" OR CMAKE_C_COMPILER MATCHES "/gcc$")
    unset(CMAKE_C_COMPILER CACHE)
endif()
if (CMAKE_CXX_COMPILER MATCHES "ccache/bin/g\\+\\+$" OR CMAKE_CXX_COMPILER MATCHES "/g\\+\\+$")
    unset(CMAKE_CXX_COMPILER CACHE)
endif()

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (NOT EXISTS ${picoVscode} AND DEFINED ENV{PICO_SDK_PATH})
    get_filename_component(_PICO_SDK_SDK_DIR "$ENV{PICO_SDK_PATH}" DIRECTORY)
    get_filename_component(_PICO_SDK_ROOT_DIR "${_PICO_SDK_SDK_DIR}" DIRECTORY)
    set(picoVscode ${_PICO_SDK_ROOT_DIR}/cmake/pico-vscode.cmake)
endif()
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# Fallback do toolchain ARM quando pico-vscode.cmake não configura compilador
if (DEFINED ENV{PICO_SDK_PATH})
    get_filename_component(_PICO_SDK_SDK_DIR "$ENV{PICO_SDK_PATH}" DIRECTORY)
    get_filename_component(_PICO_SDK_ROOT_DIR "${_PICO_SDK_SDK_DIR}" DIRECTORY)
    set(_PICO_ARM_GCC ${_PICO_SDK_ROOT_DIR}/toolchain/${toolchainVersion}/bin/arm-none-eabi-gcc)
    set(_PICO_ARM_GXX ${_PICO_SDK_ROOT_DIR}/toolchain/${toolchainVersion}/bin/arm-none-eabi-g++)
    if (EXISTS ${_PICO_ARM_GCC} AND EXISTS ${_PICO_ARM_GXX})
        set(CMAKE_C_COMPILER ${_PICO_ARM_GCC} CACHE FILEPATH "ARM GCC compiler" FORCE)
        set(CMAKE_CXX_COMPILER ${_PICO_ARM_GXX} CACHE FILEPATH "ARM G++ compiler" FORCE)
    endif()
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(FreeRTOS-RP2040-Traffic-Simulator C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ---------------------------------------------------------------------------
# Importa FreeRTOS Kernel (RP2040 port) após pico_sdk_init()
# Aponta para o third_party do clicker-rp2040 já baixado
# ---------------------------------------------------------------------------
# DESIGN: usa caminho padrão apenas quando o usuário não fornecer -DFREERTOS_KERNEL_PATH.
if (NOT DEFINED FREERTOS_KERNEL_PATH OR FREERTOS_KERNEL_PATH STREQUAL "")
    set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/../../clicker-rp2040/third_party/FreeRTOS-Kernel"
        CACHE PATH "Path to FreeRTOS Kernel")
endif()
if (EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
else()
    message(FATAL_ERROR
        "FreeRTOS_Kernel_import.cmake não encontrado em ${FREERTOS_KERNEL_PATH}.\n"
        "Configure FREERTOS_KERNEL_PATH ou copie o FreeRTOS-Kernel para third_party/.")
endif()

# ---------------------------------------------------------------------------
# Credenciais WiFi (injetadas via env ou cmake -D)
# ---------------------------------------------------------------------------
if(DEFINED ENV{WIFI_SSID})
    set(WIFI_SSID "$ENV{WIFI_SSID}")
endif()
if(DEFINED ENV{WIFI_PASSWORD})
    set(WIFI_PASSWORD "$ENV{WIFI_PASSWORD}")
endif()
if(DEFINED ENV{FALLBACK_SERVER_IP})
    set(FALLBACK_SERVER_IP "$ENV{FALLBACK_SERVER_IP}")
endif()

if(DEFINED WIFI_SSID AND DEFINED WIFI_PASSWORD)
    message(STATUS "Credenciais WiFi configuradas para a rede: ${WIFI_SSID}")
else()
    message(WARNING
        "WIFI_SSID e WIFI_PASSWORD não fornecidos! A conexão WiFi falhará em runtime.")
endif()

# ---------------------------------------------------------------------------
# Fontes comuns a todos os nós.
# Arquivos em tasks/, middleware/, simulator/ e drivers/ são incluídos aqui.
# Arquivos que ainda não existem são listados como NOTFOUND até serem criados.
# ---------------------------------------------------------------------------
set(FIRMWARE_SOURCES
    # --- ponto de entrada ---
    main.c
    freertos_hooks.c

    # --- camada rpc/discovery (stub local para build) ---
    rpc_client.c
    discovery/service_disc.c

    # --- middleware ---
    middleware/shared_state.c
    middleware/app_queues.c
    middleware/button_handler.c
    middleware/lamport.c

    # --- tasks existentes (herdadas do clicker, serão adaptadas) ---
    tasks/task_buttons.c
    tasks/task_display.c
    tasks/task_rpc.c
    tasks/task_monitor.c

    # --- tasks do simulador ---
    tasks/task_car.c
    tasks/task_ambulance.c
    tasks/task_semaphore.c
    tasks/task_clock.c

    # --- lógica do simulador ---
    simulator/grid.c
    # simulator/traffic_manager.c

    # --- drivers de hardware ---
    drivers/display/display.c
    drivers/display/ssd1306_i2c.c
    drivers/ws2812/ws2812.c

    # --- áudio ---
    audio/buzzer.c
)

# ---------------------------------------------------------------------------
# add_node_target(<NODE_NUM> <NODE_NAME> <ROLE>)
#
# Cria um target executável "<NODE_NAME>" com NODE_ID=<NODE_NUM> fixado em
# tempo de compilação. O binário de saída é nomeado <NODE_NAME>.uf2.
#
# Papéis:
#   NODE_ID=0  →  Norte  (Crato)
#   NODE_ID=1  →  Centro (Juazeiro) — servidor TCP
#   NODE_ID=2  →  Sul    (Barbalha)
# ---------------------------------------------------------------------------
function(add_node_target NODE_NUM NODE_NAME ROLE)
    # DESIGN: tres binarios independentes compartilham as mesmas fontes e
    # diferem por NODE_ID em compile-time para simplificar deploy por placa.
    add_executable(${NODE_NAME} ${FIRMWARE_SOURCES})

    pico_set_program_name(${NODE_NAME} "${NODE_NAME}")
    pico_set_program_version(${NODE_NAME} "0.1")

    # -------------------------------------------------------------------
    # Cabeçalhos PIO
    # O ws2812.pio agora fica em drivers/ws2812/ (diferente do clicker).
    # Apenas o primeiro target (nord/NODE_ID=0) gera os headers; os demais
    # herdam via add_dependencies para evitar multiple-output conflicts.
    # -------------------------------------------------------------------
    if(NODE_NUM EQUAL 0)
        pico_generate_pio_header(${NODE_NAME}
            ${CMAKE_CURRENT_LIST_DIR}/drivers/ws2812/ws2812.pio)
    else()
        target_include_directories(${NODE_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
        add_dependencies(${NODE_NAME} norte)
    endif()

    # Saída serial via UART e USB
    pico_enable_stdio_uart(${NODE_NAME} 1)
    pico_enable_stdio_usb(${NODE_NAME} 1)

    # Bibliotecas do SDK e FreeRTOS (idênticas ao clicker)
    target_link_libraries(${NODE_NAME}
        pico_stdlib
        pico_multicore
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel-Heap4
        hardware_i2c
        hardware_pio
        hardware_pwm
        hardware_adc
    )

    # Diretórios de include
    target_include_directories(${NODE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/audio
        ${CMAKE_CURRENT_LIST_DIR}/drivers/display
        ${CMAKE_CURRENT_LIST_DIR}/drivers/ws2812
        ${CMAKE_CURRENT_LIST_DIR}/middleware
        ${CMAKE_CURRENT_LIST_DIR}/simulator
        ${CMAKE_CURRENT_LIST_DIR}/tasks
    )

    # Definições de compilação
    target_compile_definitions(${NODE_NAME} PRIVATE
        DISPLAY_SSD1306=1
        PICO_HEAP_SIZE=0x14000
        configTICK_RATE_HZ=1000
        NODE_ID=${NODE_NUM}
        # NODE_ROLE é uma string descriptiva opcional para logs/debug
        NODE_ROLE="${ROLE}"
    )

    # Credenciais WiFi
    if(DEFINED WIFI_SSID AND DEFINED WIFI_PASSWORD)
        target_compile_definitions(${NODE_NAME} PRIVATE
            WIFI_SSID="${WIFI_SSID}"
            WIFI_PASSWORD="${WIFI_PASSWORD}"
        )
    endif()

    # IP do servidor RPC central (NODE_ID=1 / centro)
    if(DEFINED FALLBACK_SERVER_IP)
        target_compile_definitions(${NODE_NAME} PRIVATE
            FALLBACK_SERVER_IP="${FALLBACK_SERVER_IP}"
        )
    endif()

    # Nome exato do arquivo de saída (deve vir ANTES de pico_add_extra_outputs)
    set_target_properties(${NODE_NAME} PROPERTIES OUTPUT_NAME "${NODE_NAME}")

    # Gera .uf2, .elf, .bin, .map, .hex
    pico_add_extra_outputs(${NODE_NAME})

    message(STATUS
        "Target '${NODE_NAME}' criado — NODE_ID=${NODE_NUM} (${ROLE}) → ${NODE_NAME}.uf2")
endfunction()

# ---------------------------------------------------------------------------
# Cria os três nós do simulador
# ---------------------------------------------------------------------------
#                  ID   nome     papel
add_node_target(   0   norte   "Crato")
add_node_target(   1   centro  "Juazeiro-TCP-Server")
add_node_target(   2   sul     "Barbalha")

# Target agregado: cmake --build build/ --target all_nodes
add_custom_target(all_nodes DEPENDS norte centro sul)

# ---------------------------------------------------------------------------
# Alias para a extensão VS Code do Pico SDK ('firmware' → norte / NODE_ID=0)
# ---------------------------------------------------------------------------
add_executable(firmware ALIAS norte)