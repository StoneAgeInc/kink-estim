//
// Created by Benjamin Riggs on 10/19/21.
//

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#include "app_config.h"

typedef uint16_t zappy_intensities_t[DEVICE_CHANNEL_COUNT];

extern zappy_intensities_t intensities;

void update_pattern_progress(void);

void update_intensities(void);

void update_adjusts(void);

void display_init(void);

/**
 * @brief Curve the intensity value from strictly linear to something more interesting
 *
 * @param[in] input     Linear input as percentage of 0xFFFF.
 * @return              Adjusted intensity value, also as percentage of 0xFFFF.
 */
uint16_t curve_intensity(uint16_t input);

#endif //DISPLAY_H
