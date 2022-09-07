//
// Created by Benjamin Riggs on 1/8/20.
//

#ifndef PULSE_CONTROL_H
#define PULSE_CONTROL_H

#include <stdint.h>

#include "app_config.h"
#include "patterns.h"

// 2^12 - 1 = 0xFFF
#define POWER_MOD_MAX (0xFFF)

typedef uint16_t zappy_power_levels_t[DEVICE_CHANNEL_COUNT];

extern zappy_power_levels_t volatile power_levels;

extern uint8_t volatile channels_active;

void pulse_init(void);

uint8_t max_pulse_index(uint8_t channel);

zappy_pulse_t volatile* pulse_values(uint8_t channel);

uint16_t set_power(uint8_t channel, uint16_t power_level);

void set_pulse(uint8_t channel, zappy_pulse_t const p_pulse, uint16_t power_mod);

void pulse_pause(uint8_t channel);

void pulse_resume(uint8_t channel);

void pulse_shutdown(uint8_t channel);

#endif //PULSE_CONTROL_H
