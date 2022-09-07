//
// Created by Benjamin Riggs on 3/28/20.
//

#ifndef PATTERN_CONTROL_H
#define PATTERN_CONTROL_H

#include "patterns.h"
#include "app_config.h"

#include "nrfx.h"

#define HALF_PATTERN_ADJUST ROUNDED_DIV(MAX_PATTERN_ADJUST, 2)

typedef uint16_t zappy_pattern_adjusts_t[DEVICE_CHANNEL_COUNT];
typedef uint16_t zappy_pattern_progress_t[DEVICE_CHANNEL_COUNT];

typedef struct {
    zappy_pattern_t *p_pattern;
    uint16_t pattern_index;
    uint16_t element_index;
    uint32_t element_start;
} pattern_playback_t;

extern zappy_pattern_adjusts_t pattern_adjusts;
extern zappy_pattern_progress_t pattern_progress;
extern pattern_playback_t pattern_playback[DEVICE_CHANNEL_COUNT];

void update_pulses(void);

nrfx_err_t pattern_play(uint8_t channel, uint16_t index);

/** @note Ensure pattern_init is called after storage_init.
 */
void patterns_init(void);

#endif //PATTERN_CONTROL_H
