//
// Created by Benjamin Riggs on 12/6/19.
//

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "libprv_nRF5_config.h"

/**
 * Application Config
 */
#define DEVICE_NAME                 "Zappy Box"

#define DEVICE_CHANNEL_COUNT        4   // Logical channels
#define _CHANNEL_ARR_MAX            3   // Literal number (not macro) 1 less than device channel count for convenience

#define DEVICE_TRIAC_COUNT 6
#define X(triac_N, channel_N_low, channel_N_high) (1U << (channel_N_low)) | (1U << (channel_N_high))
/// Bitfield channel mapping of triac connections.
#define TRIAC_MAP_X \
    X(0, 2, 3), \
    X(1, 1, 2), \
    X(2, 0, 1), \
    X(3, 1, 3), \
    X(4, 0, 3), \
    X(5, 0, 2)
#undef X

#define INITIAL_CHANNEL_POWER       0   // Off.
#define MAX_CHANNEL_POWER_LEVEL     0xFFF   // 12-bit DAC
#define MAX_PATTERN_ADJUST          0xFFF   // 4k of adjustment is probably enough
#define MAX_PULSE_WIDTH             200     // in micro-seconds

#define CHANNEL_1_POWER             1
#define CHANNEL_2_POWER             1
#define CHANNEL_3_POWER             1
#define CHANNEL_4_POWER             1

#define DEFAULT_PULSE_DELAY         7000    // in micro-seconds
#define DEFAULT_PULSE_WIDTH         140     // in micro-seconds
#define DEFAULT_INTERPULSE_DELAY    0   // in micro-seconds, can be negative

// No TENS effect > ~500 Hz. Should probably also limit power delivery.
#define MIN_PULSE_VALUE             2000 // Minimum initial pulse edge, in micro-seconds

#define SCHEDULER_QUEUE_SIZE 12

// Undef first to avoid a LOT of warnings
#undef NRFX_USBD_CONFIG_IRQ_PRIORITY
#define NRFX_USBD_CONFIG_IRQ_PRIORITY ON_BOARD_COMMS_IRQ_PRIORITY

/**
 * Peripheral Configuration
 */
// AUDIO ADC
#define NRFX_I2S_ENABLED 1
// Battery charger & IMU I2C
#define NRFX_TWIM_ENABLED 1
#define NRFX_TWIM1_ENABLED 1
#define I2C_TWIM_INSTANCE 1
// SPI
#define NRFX_SPIM_ENABLED 1
// Power level DAC
#define NRFX_SPIM0_ENABLED 1
#define DAC_SPIM_INSTANCE 0
// Inter-board data
#define NRFX_SPIM3_ENABLED 1
#define NRFX_SPIM_EXTENDED_ENABLED 1
#define BOARD2BOARD_SPIM_INSTANCE 3
// Channel pulse timers
#define NRFX_TIMER_ENABLED 1
#define NRFX_TIMER1_ENABLED 1
#define NRFX_TIMER2_ENABLED 1
#define NRFX_TIMER3_ENABLED 1
#define NRFX_TIMER4_ENABLED 1
// Timer 0 reserved for SoftDevice
#define CHANNEL0_TIMER_INSTANCE 1
#define CHANNEL1_TIMER_INSTANCE 2
#define CHANNEL2_TIMER_INSTANCE 3
#define CHANNEL3_TIMER_INSTANCE 4
// GPIOTE & PPI to control MOSFETs from pulse timers
#define NRFX_GPIOTE_ENABLED 1
#define NRFX_PPI_ENABLED 1
// Software Interrupts and Event Generator Unit for pulses
#define NRFX_SWI_ENABLED 1
#define NRFX_EGU_ENABLED 1
// Enable legacy power driver, used by app_usbd
#define POWER_ENABLED 1

#endif //APP_CONFIG_H
