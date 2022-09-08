//
// Created by Benjamin Riggs on 12/20/19.
//

#include <string.h>

#include "app_config.h"
#include "pin_config.h"
#include "prv_utils.h"
#include "board2board_host.h"
#include "buttons.h"
#include "timers.h"
#include "pulse_control.h"
#include "pattern_control.h"
#include "battery_charger.h"
#include "battery_monitor.h"
#include "storage.h"
#include "triacs.h"
#include "audio_adc.h"
#include "display.h"

#ifdef ENABLE_USB_SERIAL
#include "usb_serial.h"
#endif

#include "prv_power_manager.h"

#ifdef DEBUG
#include "nrf_stack_guard.h"
#endif

#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_timer.h"
#include "nrf_drv_clock.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_delay.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdh.h"
#endif

// #define TEST

/**
 * @brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of an assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
    DEBUG_BREAKPOINT;
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

static bool shutdown_outputs(void) {
    // TODO: Signal display board
    for (int channel = 0; channel < DEVICE_CHANNEL_COUNT; ++channel) {
        pulse_shutdown(channel);
    }
    return true;
}

static bool shutdown_power(void) {
    return battery_charger_shutdown();
}

static void pins_init(void) {
    //Output pins.
    uint8_t output_pins[] = GPIO_OUTPUT_PINS;
    for (int i = 0; i < NRFX_ARRAY_SIZE(output_pins); i++) {
        nrf_gpio_cfg_output(output_pins[i]);
    }

    //Input pins.
    uint8_t input_pins[] = GPIO_INPUT_PINS;
    for (int i = 0; i < NRFX_ARRAY_SIZE(input_pins); i++) {
        nrf_gpio_cfg_input(input_pins[i], NRF_GPIO_PIN_NOPULL);
    }
}

static void clock_init(void) {
    // app_usb requires legacy nrf_drv-* driver enabled.
    if (nrf_drv_clock_init_check() && nrf_drv_clock_lfclk_is_running() && nrf_drv_clock_hfclk_is_running()) {
        return;
    }

    APP_ERROR_CHECK(nrf_drv_clock_init());

    // 'if' checks because the softdevice may have started them
    if (!nrf_drv_clock_hfclk_is_running()) nrf_drv_clock_hfclk_request(NULL);
    if (!nrf_drv_clock_lfclk_is_running()) nrf_drv_clock_lfclk_request(NULL);

    while (!nrf_drv_clock_lfclk_is_running() || !nrf_drv_clock_hfclk_is_running()) { /* Waiting for clock */ }
}


#ifdef TEST
    // Switching frequency in Hz
    #define GATE_SWITCHING_FREQUENCY        10
    // Channel for which we need to switch the gate pins for test
    #define GATE_CHANNEL                0
    // Duration for which we need to switch the gate pins for test
    #define GATE_DURATION                    1

    #define GATE_OUTPUT_PINS {  \
        GATE_0A_PIN, \
        GATE_0B_PIN, \
        GATE_1A_PIN, \
        GATE_1B_PIN, \
        GATE_2A_PIN, \
        GATE_2B_PIN, \
        GATE_3A_PIN, \
        GATE_3B_PIN, \
    }

void test_controls()
{
    //  initialize gate pins and clear 
    uint8_t output_gate_pins[] = GATE_OUTPUT_PINS;
    for (int i = 0; i < NRFX_ARRAY_SIZE(output_gate_pins); i++) {
        nrf_gpio_cfg_output(output_gate_pins[i]);
        nrf_gpio_pin_clear(output_gate_pins[i]);
    }

    // calculate iterations we need to run for the whole GATE_DURATION
    uint32_t iterations = GATE_DURATION * GATE_SWITCHING_FREQUENCY;
    // calculate delat between each toggle
    uint32_t delay = 500/GATE_SWITCHING_FREQUENCY;

    for(int i=0 ; i<iterations; i++)
    {
        nrf_gpio_pin_clear(output_gate_pins[(2 * GATE_CHANNEL) + 1]);
        nrf_gpio_pin_set(output_gate_pins[2 * GATE_CHANNEL]);
        nrf_delay_ms(delay);
        nrf_gpio_pin_clear(output_gate_pins[2 * GATE_CHANNEL]);
        nrf_gpio_pin_set(output_gate_pins[(2 * GATE_CHANNEL) + 1]);
        nrf_delay_ms(delay);
    }

    // clear all pins again after the test is complete
    for (int i = 0; i < NRFX_ARRAY_SIZE(output_gate_pins); i++) {
        nrf_gpio_pin_clear(output_gate_pins[i]);
    }
}

#endif // TEST

int main(void) {

    // Set DC/DC converter output on VDD. Only applicable when power supplied via VDDH.
    prv_set_UICR(UICR_REGOUT0_VOUT_3V3, UICR_NFCPINS_PROTECT_NFC);

    // Enable on-chip DC/DC converters
    // Inductor supplied by MDBT50Q Module internally
    NRF_POWER->DCDCEN = POWER_DCDCEN_DCDCEN_Enabled << POWER_DCDCEN_DCDCEN_Pos;
    // Requires external inductor, and fixed silicon, if enabled w/o both, CHIP WILL BRICK
    NRF_POWER->DCDCEN0 = POWER_DCDCEN0_DCDCEN_Enabled << POWER_DCDCEN0_DCDCEN_Pos;

    // uint8_t output_pins[] = GPIO_OUTPUT_PINS;
    // for (int i = 0; i < NRFX_ARRAY_SIZE(output_pins); i++) {
    //     nrf_gpio_cfg_output(output_pins[i]);
    // }

    // for (int i = 0; i < NRFX_ARRAY_SIZE(output_pins); i++) {
    //     nrf_gpio_pin_set(output_pins[i]);
    // }   

    // Wait for USB muxer
    nrfx_coredep_delay_us(500000);

    prv_logging_init();

    #ifdef DEBUG
    NRF_STACK_GUARD_INIT();
    #endif

    APP_SCHED_INIT(sizeof(void *), SCHEDULER_QUEUE_SIZE);

    // Start app timer before everything because it's used in multiple places.
    APP_ERROR_CHECK(app_timer_init());
    // Must come after app_timer_init.
    // prv_power_manager_init(shutdown_outputs, shutdown_power);

    pins_init();
    buttons_init();
    display_init();
    clock_init();
    battery_monitor_init();
    storage_init();
    triacs_init();
    pulse_init();
    patterns_init();

    // Ensure state is completely initialized before displaying stuff and accepting connections.
    app_sched_execute();
    // Don't initialize external comms until everything else has been initialized.
    board2board_host_init();
    #if ENABLE_USB_SERIAL
    nrf_gpio_cfg(USB_SELECT_PIN,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_DISCONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_H0S1,
                 NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_pin_clear(USB_SELECT_PIN);
    // nrf_gpio_pin_set(USB_SELECT_PIN);
    usb_init();
//    audio_adc_init();
    #endif

    timers_init();

#ifdef TEST

    test_controls();
#endif

    set_power(0, CHANNEL_0_POWER);
    set_power(1, CHANNEL_1_POWER);
    set_power(2, CHANNEL_2_POWER);
    set_power(3, CHANNEL_3_POWER);

    while (true) {
        #if ENABLE_USB_SERIAL
        process_usb_events();
        #endif
        app_sched_execute();
        // prv_wait();
    }
}
