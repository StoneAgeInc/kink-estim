//
// Created by rca on 2020-04-07.
//

#include "buttons.h"
#include "pin_config.h"
#include "prv_power_manager.h"
#include "prv_debouncer.h"
#include "timers.h"

#define MAX_COUNT (POWER_BUTTON_OFF_HOLD_MS) * (BUTTON_SCAN_UPDATE_FREQ_Hz) / 1000
static prv_debouncer_t power_debouncer;

void button_scan(void) {
    static int32_t countdown = MAX_COUNT;
    bool pressed;
    if (prv_debounce(&power_debouncer, !nrf_gpio_pin_read(SOFT_POWER_PIN), (uint8_t *) &pressed)) {
        countdown = MAX_COUNT;
    } else if (countdown > 0) {
        countdown--;
    } else if (countdown == 0 && pressed) {
        countdown--;
        prv_shutdown();
    }
}

void buttons_init(void) {
    nrf_gpio_cfg_input(SOFT_POWER_PIN, NRF_GPIO_PIN_PULLUP);
    prv_debounce_init(&power_debouncer, (BUTTON_DEBOUNCE_MS) * (BUTTON_SCAN_UPDATE_FREQ_Hz) / 1000,
                      nrf_gpio_pin_read(SOFT_POWER_PIN));
}

#undef MAX_COUNT
