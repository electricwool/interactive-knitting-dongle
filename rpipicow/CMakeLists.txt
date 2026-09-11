cmake_minimum_required(VERSION 3.13)

# ---- Target board ----------------------------------------------------------
# Must be a CACHE var set BEFORE include(pico_sdk_import.cmake): the SDK
# resolves the board at include time. This board is an RP2040, so default to
# pico. For an RP2350 board, reconfigure with -DPICO_BOARD=pico2.
if (NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico CACHE STRING "Target RP2xxx board" FORCE)
endif()

include(pico_sdk_import.cmake)

project(rpipicow C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_executable(rpipicow
    src/main.c
    src/ftdi.c
    src/usb_descriptors.c
    src/uart_link.c
    src/uart_router.c
    src/reed_sensor.c
)

# tusb_config.h and board_config.h live here.
target_include_directories(rpipicow PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(rpipicow PRIVATE
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(rpipicow)
