cmake_minimum_required(VERSION 3.15)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../external/libprv_nRF5/cmake)
include(magic)

#set(SOFTDEVICE s140)
set(SD_VERSION 7.2.0)
# In softdevice release notes pdf
set(SD_FIRMWARE_ID 0x0100)

set(HEAP_SIZE 0x200)
# Worst-case stack usage for s140: 0x600
set(STACK_SIZE 0x4600)

set(ENABLE_SPIM3 1)
set(ENABLE_USB_SERIAL 1)

set(HARDWARE_REV 5)

project(zappy_board VERSION 0.4.0
        DESCRIPTION "zappy_board"
        LANGUAGES C CXX ASM)

# Include board functions
include(compile_targets_nRF52840)

target_sources("${PROJECT_NAME}" PRIVATE
#        "${CMAKE_CURRENT_SOURCE_DIR}/src/audio_adc.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/battery_charger.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/battery_monitor.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/board2board_host.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/buttons.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/display.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/pattern_control.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/prv_ble.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/pulse_control.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/serial_parser.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/storage.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/timers.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/triacs.c"
        "$<$<BOOL:${ENABLE_USB_SERIAL}>:${CMAKE_CURRENT_SOURCE_DIR}/src/usb_serial.c>"
        )

target_include_directories("${PROJECT_NAME}" PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common"
        )
