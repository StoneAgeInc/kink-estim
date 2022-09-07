//
// Created by Benjamin Riggs on 9/10/21.
//

#include "triacs.h"
#include "app_config.h"
#include "pin_config.h"

#include "nrf_gpio.h"

static uint8_t volatile triacs_output = 0;

#define X(triac_N, channel_N_low, channel_N_high) (1U << (channel_N_low)) | (1U << (channel_N_high))
static uint8_t const triac_map[DEVICE_TRIAC_COUNT] = {TRIAC_MAP_X};
#undef X

static void set_triacs(uint8_t output) {
    int i;
    for (i = 1 << (DEVICE_TRIAC_COUNT - 1); i; i >>= 1) {
        nrf_gpio_pin_write(SHIFT_DS_PIN, output & i);
        nrf_gpio_pin_set(SHIFT_SHCP_PIN);
        nrf_gpio_pin_clear(SHIFT_DS_PIN);
        nrf_gpio_pin_clear(SHIFT_SHCP_PIN);
    }
    nrf_gpio_pin_set(SHIFT_STCP_PIN);
    triacs_output = output & ((1 << DEVICE_TRIAC_COUNT) - 1);
    nrf_gpio_pin_clear(SHIFT_STCP_PIN);
}

void triacs_init(void) {
    set_triacs(0);
}

nrfx_err_t set_triac(uint8_t low_channel, uint8_t high_channel, bool enable) {
    uint8_t map = (1 << low_channel) | (1 << high_channel);
    int i;
    for (i = 0; i < DEVICE_TRIAC_COUNT; i++) {
        if (triac_map[i] == map) break;
    }
    if (i == DEVICE_TRIAC_COUNT) return NRFX_ERROR_INVALID_PARAM;

    uint8_t output = (triacs_output & ~(1 << i)) | (enable << i);
    set_triacs(output);

    return NRFX_SUCCESS;
}
