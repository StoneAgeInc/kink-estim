//
// Created by Benjamin Riggs on 1/23/20.
//

#include <memory.h>

#include "prv_utils.h"
#include "pulse_control.h"
#include "pin_config.h"
#include "display.h"

#include "hal/nrf_gpio.h"
#include "hal/nrf_timer.h"
#include "nrfx_timer.h"
#include "nrfx_gpiote.h"
#include "nrfx_ppi.h"
#include "nrfx_swi.h"
#include "nrfx_spim.h"

#ifdef DEBUG
#include "nrf_log.h"
#endif

uint8_t volatile channels_active = 0;

#define INITIAL_PULSE { \
    (DEFAULT_PULSE_DELAY), \
    (DEFAULT_PULSE_DELAY + DEFAULT_PULSE_WIDTH), \
    (DEFAULT_PULSE_DELAY + DEFAULT_PULSE_WIDTH + DEFAULT_INTERPULSE_DELAY), \
    (DEFAULT_PULSE_DELAY + DEFAULT_PULSE_WIDTH + DEFAULT_INTERPULSE_DELAY + DEFAULT_PULSE_WIDTH) \
}
#define INITIAL_PULSE_MAX_INDEX 3

#define GATE_PIN_MAPPING(channel) { GATE_ ## channel ## A_PIN, GATE_ ## channel ## B_PIN }
static uint32_t const gate_pins[DEVICE_CHANNEL_COUNT][2 /* Positive & negative enable pins */] = {
    GATE_PIN_MAPPING(0),
    GATE_PIN_MAPPING(1),
    GATE_PIN_MAPPING(2),
    GATE_PIN_MAPPING(3),
};

static nrfx_timer_t const timers[DEVICE_CHANNEL_COUNT] = {
    NRFX_TIMER_INSTANCE(CHANNEL0_TIMER_INSTANCE),
    NRFX_TIMER_INSTANCE(CHANNEL1_TIMER_INSTANCE),
    NRFX_TIMER_INSTANCE(CHANNEL2_TIMER_INSTANCE),
    NRFX_TIMER_INSTANCE(CHANNEL3_TIMER_INSTANCE),
};

static nrfx_spim_t const spim = NRFX_SPIM_INSTANCE(DAC_SPIM_INSTANCE);

#define DAC60504_DEVICE_ID_REG  1
#define DAC60504_SPI_READ       0x80
#define DAC60504_SPI_WRITE      0x00
#define DAC60504_REG_GAIN       0x04
#define DAC60504_REG_DAC0       0x8
#define DAC60504_REG_DAC1       0x9
#define DAC60504_REG_DAC2       0xA
#define DAC60504_REG_DAC3       0xB
#define DAC60504_BIT_DEPTH      12
static uint8_t const dac_addr_map[] = {
    DAC60504_REG_DAC3,
    DAC60504_REG_DAC2,
    DAC60504_REG_DAC1,
    DAC60504_REG_DAC0,
};
static uint8_t volatile spim_write_buffer[3] = {0};
static uint8_t volatile spim_read_buffer[3] = {0};

static nrfx_spim_xfer_desc_t spim_desc = NRFX_SPIM_XFER_TRX((uint8_t *) spim_write_buffer, 3,
                                                            (uint8_t *) spim_read_buffer, 3);

typedef struct {
    zappy_pulse_t pulse;
    uint32_t power_modulator;
    // Keep track of which pulse edge is last, enabling arbitrary pulse combinations
    uint8_t max_pulse_index;
    uint16_t pv_selector;
} pulse_state_t;

// Value set by user input, copied to compare register in state handler
pulse_state_t volatile *pulse_states[DEVICE_CHANNEL_COUNT];

// Because memcpy can be interrupted, use two arrays and alternate between them
static pulse_state_t volatile pulse_states_store[DEVICE_CHANNEL_COUNT][2] = {
    [0 ... _CHANNEL_ARR_MAX] = {
        [0 ... 1] = {
            .pulse = INITIAL_PULSE,
            .power_modulator = 0,
            .max_pulse_index = INITIAL_PULSE_MAX_INDEX,
            .pv_selector = 0
        }
    }
};

// An Software Interrupt
static nrfx_swi_t swi;

// Used to Enable interrupts for update_pulse_edges
static uint32_t volatile egu_task_addrs[DEVICE_CHANNEL_COUNT];

static nrf_ppi_channel_t volatile ppi_channels[DEVICE_CHANNEL_COUNT][PULSE_EDGES] =
    {[0 ... _CHANNEL_ARR_MAX] = {0xFF, 0xFF, 0xFF, 0xFF}};

static nrf_ppi_channel_t volatile active_forks[DEVICE_CHANNEL_COUNT] = {0};

// Value set on user input, copied to pwm_tick_counts while channel is idle
// Array exposed to easily copy values from
zappy_power_levels_t volatile power_levels = {[0 ... _CHANNEL_ARR_MAX] = INITIAL_CHANNEL_POWER};

#ifdef DEBUG
static uint32_t volatile no_flags_count = 0;
#endif

static void update_pulse_edges(uint8_t __unused _swi, uint16_t flags) {
    if (!flags) {
        #ifdef DEBUG
        no_flags_count++;
        #endif
        return;
    }
    // https://stackoverflow.com/a/18050458/161366
    uint8_t channel = 31 - __builtin_clz(flags);

    if (channels_active & (1UL << channel)) {
        // Update timer compare registers for this channel
        for (uint8_t i = 0; i < PULSE_EDGES; i++) {
            if (i == (*pulse_states[channel]).max_pulse_index) {
                if (active_forks[channel] != ppi_channels[channel][i]) {
                    // Old PPI fork in place, clear it
                    APP_ERROR_CHECK(nrfx_ppi_channel_fork_assign(active_forks[channel], 0));
                    // Assign new fork
                    active_forks[channel] = ppi_channels[channel][i];
                    APP_ERROR_CHECK(nrfx_ppi_channel_fork_assign(active_forks[channel], egu_task_addrs[channel]));
                }
                nrfx_timer_extended_compare(
                    &timers[channel],
                    i,
                    (*pulse_states[channel]).pulse[i],
                    // Reset timer counter upon reaching this (max) compare value
                    TIMER_SHORTS_COMPARE0_CLEAR_Enabled << i,
                    false
                );
            } else {
                nrfx_timer_extended_compare(&timers[channel], i, (*pulse_states[channel]).pulse[i], 0, false);
            }
        }
    } else {
        nrfx_timer_disable(&timers[channel]);
    }
}

// For nrfx_timer_init. Must not be NULL.
static void pulse_timer_irq_evt_handler(nrf_timer_event_t __unused event_type, void __unused *p_context) {}

static void timer_init(void) {
    nrfx_timer_config_t timer_config = NRFX_TIMER_DEFAULT_CONFIG;
    // Pulses are defined in milliseconds, so use a timer resolution to match.
    timer_config.frequency = NRF_TIMER_FREQ_1MHz;
    timer_config.mode = NRF_TIMER_MODE_TIMER;
    timer_config.bit_width = NRF_TIMER_BIT_WIDTH_16;
    for (int i = 0; i < DEVICE_CHANNEL_COUNT; i++) {
        APP_ERROR_CHECK(nrfx_timer_init(&timers[i], &timer_config, pulse_timer_irq_evt_handler));
    }
}

static nrfx_gpiote_out_config_t gpiote_config = NRFX_GPIOTE_CONFIG_OUT_TASK_TOGGLE(false);

static void gate_init(void) {
    nrfx_gpiote_init();

    uint32_t pin;
    uint32_t task_addr;
    uint32_t evt_addr;
    nrf_ppi_channel_t ppi_channel;

#define SET_GATE_PPI \
APP_ERROR_CHECK(nrfx_ppi_channel_alloc(&ppi_channel)); \
APP_ERROR_CHECK(nrfx_ppi_channel_assign(ppi_channel, evt_addr, task_addr)); \
APP_ERROR_CHECK(nrfx_ppi_channel_enable(ppi_channel))

    // Configure Software Interrupt to call update_pulse_edges
    APP_ERROR_CHECK(nrfx_swi_alloc(&swi, update_pulse_edges, PULSE_UPDATES_IRQ_PRIORITY));

    for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
        egu_task_addrs[channel] = nrfx_swi_task_trigger_address_get(swi, channel);
        // Gate A pin
        pin = gate_pins[channel][0];
        APP_ERROR_CHECK(nrfx_gpiote_out_init(pin, &gpiote_config));
        task_addr = nrfx_gpiote_out_task_addr_get(pin);
        // Gate A rising edge
        evt_addr = nrfx_timer_compare_event_address_get(&timers[channel], 0);
        SET_GATE_PPI;
        ppi_channels[channel][0] = ppi_channel;
        // Gate A falling edge
        evt_addr = nrfx_timer_compare_event_address_get(&timers[channel], 1);
        SET_GATE_PPI;
        ppi_channels[channel][1] = ppi_channel;
        // Enable gate
        nrfx_gpiote_out_task_enable(pin);

        // Gate B pin
        pin = gate_pins[channel][1];
        APP_ERROR_CHECK(nrfx_gpiote_out_init(pin, &gpiote_config));
        task_addr = nrfx_gpiote_out_task_addr_get(pin);
        // Gate B rising edge
        evt_addr = nrfx_timer_compare_event_address_get(&timers[channel], 2);
        SET_GATE_PPI;
        ppi_channels[channel][2] = ppi_channel;
        // Gate B falling edge
        evt_addr = nrfx_timer_compare_event_address_get(&timers[channel], 3);
        SET_GATE_PPI;
        ppi_channels[channel][3] = ppi_channel;
        // Enable gate
        nrfx_gpiote_out_task_enable(pin);
    }
}

static bool volatile spim_busy = false;
// Unused callback needed to ensure async SPI calls
static void spim_evt_handler(nrfx_spim_evt_t const *p_evt, void *p_context) {
    spim_busy = false;
}

static void spi_init(void) {
    nrfx_spim_config_t spim_config  = NRFX_SPIM_DEFAULT_CONFIG;

    spim_config.mode            = NRF_SPIM_MODE_1;
    spim_config.frequency       = NRF_SPIM_FREQ_8M;
    spim_config.irq_priority    = ON_BOARD_COMMS_IRQ_PRIORITY;
    spim_config.sck_pin         = DAC_SCL_PIN;
    spim_config.mosi_pin        = DAC_SDI_PIN;
    spim_config.miso_pin        = DAC_SDO_PIN;
    spim_config.ss_pin          = DAC_CS_PIN;

    APP_ERROR_CHECK(nrfx_spim_init(&spim, &spim_config, spim_evt_handler, NULL));
}

static void inline enable_channel(uint8_t channel) {
    if (!(channels_active & (1UL << channel))) {
        channels_active |= 1UL << channel;
        update_pulse_edges(0xFF, 1UL << channel);
        nrfx_timer_enable(&timers[channel]);
    }
}

static void inline disable_channel(uint8_t channel) {
    if (channels_active & (1UL << channel)) {
        channels_active &= ~(1UL << channel);
    }
}

static void dac_set_power(uint16_t power_level, uint8_t channel) {
    // Modulate power level
    power_level = ((POWER_MOD_MAX - (*pulse_states[channel]).power_modulator) * power_level / POWER_MOD_MAX);

    while (spim_busy) { prv_wait(); }
    // Bit numbers in TI datasheet are sent high to low.
    // Set buffer address
    spim_write_buffer[0] = dac_addr_map[channel] | DAC60504_SPI_WRITE;
    //Spread the DAC power level over two bytes.
    //The first byte gets the upper 8 bits, the second byte gets the rest, right-padded with zeros.
    spim_write_buffer[1] = power_level >> (16 - DAC60504_BIT_DEPTH);
    spim_write_buffer[2] = power_level << (16 - DAC60504_BIT_DEPTH);

    spim_busy = true;
//    nrfx_err_t err = nrfx_spim_xfer(&spim, &spim_desc, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER);
    nrfx_err_t err = nrfx_spim_xfer(&spim, &spim_desc, 0);
    APP_ERROR_CHECK(err);
}

// Helper function to be queued to main loop
static void update_power_level(uint8_t *p_channel) {
    dac_set_power(power_levels[*p_channel], *p_channel);
}

static void dac_set_register(uint8_t addr, uint16_t value) {
    // Valid addresses are 0-F
    ASSERT((addr & 0xF0) == 0);

    while (spim_busy) { prv_wait(); }
    // Bit numbers in TI datasheet are sent high to low.
    spim_write_buffer[0] = addr | DAC60504_SPI_WRITE;
    spim_write_buffer[1] = value / 256;
    spim_write_buffer[2] = value % 256;

    spim_busy = true;
//    nrfx_err_t err = nrfx_spim_xfer(&spim, &spim_desc, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER);
    nrfx_err_t err = nrfx_spim_xfer(&spim, &spim_desc, 0);
    APP_ERROR_CHECK(err);
}

static uint16_t dac_read(uint8_t reg_addr) {
    spim_write_buffer[0] = reg_addr | DAC60504_SPI_READ;

    while (spim_busy) { prv_wait(); }
    spim_busy = true;
//    nrfx_err_t err = nrfx_spim_xfer(&spim, &spim_desc, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER);
    nrfx_err_t err = nrfx_spim_xfer(&spim, &spim_desc, 0);
    APP_ERROR_CHECK(err);

    while (spim_busy) { prv_wait(); }
    spim_busy = true;
//    err = nrfx_spim_xfer(&spim, &spim_desc, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER);
    err = nrfx_spim_xfer(&spim, &spim_desc, 0);
    APP_ERROR_CHECK(err);

    while (spim_busy) { prv_wait(); }
    return (spim_read_buffer[1] << 8) | spim_read_buffer[2];
}

static void dac_init(void) {
    spi_init();
    uint16_t device_id = dac_read(DAC60504_DEVICE_ID_REG);
    DEBUG_BREAKPOINT_CHECK(device_id != 0b0010010000010111);
    // Disable 2x gain
    dac_set_register(DAC60504_REG_GAIN, 0x0);
    // TODO: Swap SDO to ALARM, enable CRC
}

void pulse_init(void) {
    timer_init();
    gate_init();
    dac_init();
    for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
        pulse_states[channel] = (pulse_state_t *) &pulse_states_store[channel][pulse_states_store[channel]->pv_selector];
        // Add a slight variation in timing between channels to avoid prolonged power spikes w/ no patterns playing.
        for (int i = 0; i < PULSE_EDGES; ++i) {
            (*pulse_states[channel]).pulse[i] += channel * 8 /* Arbitrary, based on watching a scope */;
        }
    }
}

uint8_t max_pulse_index(uint8_t channel) {
    return pulse_states[channel]->max_pulse_index;
}

zappy_pulse_t volatile* pulse_values(uint8_t channel) {
    return &pulse_states[channel]->pulse;
}

uint16_t set_power(uint8_t channel, uint16_t power_level) {
    if (power_level == power_levels[channel]) return power_level;
    power_levels[channel] = power_level = MIN(power_level, MAX_CHANNEL_POWER_LEVEL);
    dac_set_power(power_level, channel);
    if (power_level > 0) {
        enable_channel(channel);
    } else {
        disable_channel(channel);
    }
    return power_levels[channel];
}

void set_pulse(uint8_t channel, zappy_pulse_t const pulse, uint16_t power_mod) {
    static uint8_t const channels[DEVICE_CHANNEL_COUNT] = {0, 1, 2, 3};
    #if 0
    NRF_LOG_DEBUG("%*u|%5u, %5u, %5u, %5u|", 27 * channel + 5, power_levels[channel], pulse[0], pulse[1], pulse[2], pulse[3]);
    #endif
    // Double-buffered writes to avoid race conditions.
    uint16_t pv_sel = (uint16_t) !((bool) ((pulse_state_t *) pulse_states[channel])->pv_selector);
    pulse_state_t volatile *pulse_state = &pulse_states_store[channel][pv_sel];
    uint8_t max_pulse_index = 0;
    for (int i = 0; i < PULSE_EDGES; ++i) {
        if (pulse[i] < MIN_PULSE_VALUE) {
            // Disable pulse values less than minimum
            pulse_state->pulse[i] = 0;
        } else {
            if (pulse[i] >= pulse[max_pulse_index]) {
                max_pulse_index = i;
            }
            pulse_state->pulse[i] = pulse[i];
        }
    }
    pulse_state->power_modulator = power_mod;
    pulse_state->max_pulse_index = max_pulse_index;
    pulse_state->pv_selector = pv_sel;
    if (pulse_states[channel]->power_modulator != pulse_state->power_modulator) {
        app_sched_event_put(&channels[channel], 1, SCHED_FN(update_power_level));
    }
    // Update pointer last in single operation to avoid race conditions.
    pulse_states[channel] = (pulse_state_t *) pulse_state;

    // Update intensity values
    pulse_state_t *ps = (pulse_state_t *) pulse_states[channel];
    uint16_t pulse_max_width = MAX(ps->pulse[1] - ps->pulse[0], ps->pulse[3] - ps->pulse[2]);
    uint16_t pulse_width_intensity = 0xFFFF * pulse_max_width / MAX_PULSE_WIDTH;
    uint16_t power_mod_intensity = 0xFFFF *
                                   (POWER_MOD_MAX - ps->power_modulator) * power_levels[channel] / POWER_MOD_MAX;
    intensities[channel] = curve_intensity(power_mod_intensity) * curve_intensity(pulse_width_intensity);
}

void pulse_pause(uint8_t channel) {
    disable_channel(channel);
}

void pulse_resume(uint8_t channel) {
    if (power_levels[channel] > 0) {
        enable_channel(channel);
    }
}

void pulse_shutdown(uint8_t channel) {
    set_power(channel, 0);
}
