//
// Created by Benjamin Riggs on 3/3/20.
//

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "hal/nrf_gpio.h"

#if HARDWARE_REV == 5
    // DAC pins.
    #define DAC_SCL_PIN             NRF_GPIO_PIN_MAP(0,  8)
    #define DAC_CS_PIN              NRF_GPIO_PIN_MAP(1,  8)
    #define DAC_SDO_PIN             NRF_GPIO_PIN_MAP(0,  6)
    #define DAC_SDI_PIN             NRF_GPIO_PIN_MAP(0,  7)
    #define DAC_LDAC_PIN            NRF_GPIO_PIN_MAP(1,  9)

    // Inter-board data SPI
    #define B2B_SCL_PIN             NRF_GPIO_PIN_MAP(0,  5)
    #define B2B_SDI_PIN             NRF_GPIO_PIN_MAP(0, 27)
    #define B2B_SDO_PIN             NRF_GPIO_PIN_MAP(0, 26)
    #define B2B_CSN_PIN             NRF_GPIO_PIN_MAP(0,  4)
    #define B2B_FLASH_CS_PIN        NRF_GPIO_PIN_MAP(0, 18)

    // N-FET gate pins.
    #define GATE_0A_PIN             NRF_GPIO_PIN_MAP(1,  3)
    #define GATE_0B_PIN             NRF_GPIO_PIN_MAP(1,  5)
    #define GATE_1A_PIN             NRF_GPIO_PIN_MAP(1,  7)
    #define GATE_1B_PIN             NRF_GPIO_PIN_MAP(1,  6)
    #define GATE_2A_PIN             NRF_GPIO_PIN_MAP(1,  4)
    #define GATE_2B_PIN             NRF_GPIO_PIN_MAP(1, 15)
    #define GATE_3A_PIN             NRF_GPIO_PIN_MAP(0,  3)
    #define GATE_3B_PIN             NRF_GPIO_PIN_MAP(0,  2)

    // Jack detection pins.  TODO: determine if active high or low.
    #define JACK_SWITCH_0_PIN       NRF_GPIO_PIN_MAP(1, 14)
    #define JACK_SWITCH_1_PIN       NRF_GPIO_PIN_MAP(1, 13)
    #define JACK_SWITCH_2_PIN       NRF_GPIO_PIN_MAP(1, 12)
    #define JACK_SWITCH_3_PIN       NRF_GPIO_PIN_MAP(1, 11)

    // Current shunt sense pins.
    #define CURRENT_SENSE_0_PIN     NRF_GPIO_PIN_MAP(0, 28)
    #define CURRENT_SENSE_1_PIN     NRF_GPIO_PIN_MAP(0, 29)
    #define CURRENT_SENSE_2_PIN     NRF_GPIO_PIN_MAP(0, 31)
    #define CURRENT_SENSE_3_PIN     NRF_GPIO_PIN_MAP(0, 30)

    // IMU pins.
    #define IMU_BOOTN_PIN           NRF_GPIO_PIN_MAP(1,  1)
    #define IMU_WAKE_PIN            NRF_GPIO_PIN_MAP(1,  2)
    #define IMU_NRST_PIN            NRF_GPIO_PIN_MAP(0,  9)
    #define IMU_INTN_PIN            NRF_GPIO_PIN_MAP(0, 10)
    #define IMU_CSN_PIN             NRF_GPIO_PIN_MAP(0, 24)
    #define IMU_SCLK_PIN            NRF_GPIO_PIN_MAP(1,  0)
    #define IMU_MOSI_PIN            NRF_GPIO_PIN_MAP(0, 25)
    #define IMU_MISO_PIN            NRF_GPIO_PIN_MAP(0, 22)

    // Battery charger interrupt pin.
    #define BAT_INT_PIN             NRF_GPIO_PIN_MAP(0, 19)
    // Battery monitor info pin
    #define BATTERY_INFO_PIN        NRF_GPIO_PIN_MAP(0, 17)

    // I2C, shared by the battery charger and battery monitor.
    #define I2C_SDA_PIN             NRF_GPIO_PIN_MAP(0, 15)
    #define I2C_SCL_PIN             NRF_GPIO_PIN_MAP(0, 16)

    // Audio input pins.
    #define AUDIO_BCK_PIN           NRF_GPIO_PIN_MAP(0, 20)
    #define AUDIO_DOUT_PIN          NRF_GPIO_PIN_MAP(0, 21)
    #define AUDIO_LRCK_PIN          NRF_GPIO_PIN_MAP(0, 23)

    #define USB_SELECT_PIN          NRF_GPIO_PIN_MAP(0, 13)     // Pull low to 'claim' USB from mux

    #define SOFT_POWER_PIN          NRF_GPIO_PIN_MAP(1, 10)     //Active low.

    // Triac shift register pins.
    #define SHIFT_SHCP_PIN          NRF_GPIO_PIN_MAP(0, 11)
    #define SHIFT_STCP_PIN          NRF_GPIO_PIN_MAP(0, 14)
    #define SHIFT_DS_PIN            NRF_GPIO_PIN_MAP(0, 12)

    // GPIO inputs.
    #define GPIO_INPUT_PINS {   \
        JACK_SWITCH_0_PIN,      \
        JACK_SWITCH_1_PIN,      \
        JACK_SWITCH_2_PIN,      \
        JACK_SWITCH_3_PIN,      \
        BAT_INT_PIN,            \
        SOFT_POWER_PIN,         \
    }

    // GPIO outputs.
    #define GPIO_OUTPUT_PINS {  \
        USB_SELECT_PIN,         \
        SHIFT_SHCP_PIN,         \
        SHIFT_STCP_PIN,         \
        SHIFT_DS_PIN,           \
    }
#endif  // PCB_REVISION

#endif // PIN_CONFIG_H
