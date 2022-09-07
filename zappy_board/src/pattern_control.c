//
// Created by Benjamin Riggs on 3/10/20.
//

#include "pattern_control.h"
#include "pulse_control.h"
#include "prv_utils.h"
#include "timers.h"
#include "storage.h"
#include "display.h"

zappy_pattern_adjusts_t pattern_adjusts = {[0 ... _CHANNEL_ARR_MAX] = 0};
zappy_pattern_progress_t pattern_progress = {0};

pattern_playback_t pattern_playback[DEVICE_CHANNEL_COUNT] = {[0 ... _CHANNEL_ARR_MAX] = {0}};

static zappy_pulse_t pulse_cache = {0};
static uint16_t _power_mod_cache_store = 0;
static uint16_t *power_mod_cache = &_power_mod_cache_store;

static void iter_element(uint8_t channel) {
    pattern_playback_t *pb = &pattern_playback[channel];
    pb->element_index++;
    // Restart if end is reached
    pb->element_index %= pb->p_pattern->element_count;
    // Copy pulse to RAM
    memcpy(pulse_cache, pb->p_pattern->elements[pb->element_index].pulse, sizeof(zappy_pulse_t));
    *power_mod_cache = pb->p_pattern->elements[pb->element_index].power_modulator;
    pb->element_start = ms_timestamp();
}

// Completion is a percentage value * UINT16_MAX.
static void interpolate_pulse(uint16_t completion,
                              zappy_pattern_element_t const *p_element,
                              zappy_pattern_element_t const *p_next_element) {
    switch (p_element->easing) {
        case EASING_LINEAR: {
            // Linear interpolation between subsequent power_modulator values
            // int32 can accommodate +/- UINT12_MAX * UINT16_MAX
            int32_t delta_pm = p_next_element->power_modulator - p_element->power_modulator;
            *power_mod_cache = p_element->power_modulator + ((delta_pm * completion) >> 16);

            // Linear interpolation between each edge of subsequent pulses
            for (uint8_t i = 0; i < PULSE_EDGES; i++) {
                uint32_t t0 = p_element->pulse[i];
                uint32_t t1 = p_next_element->pulse[i];
                // Cannot use int32 because UINT16_MAX * UINT16_MAX would overflow
                if (t1 > t0) {
                    pulse_cache[i] = t0 + (((t1 - t0) * completion) >> 16);
                } else {
                    pulse_cache[i] = t0 - (((t0 - t1) * completion) >> 16);
                }
            }
        }
            break;
        default:
        case EASING_NONE:
            // Update caches with unmodified pattern element values.
            *power_mod_cache = p_element->power_modulator;
            for (uint8_t i = 0; i < PULSE_EDGES; i++) {
                pulse_cache[i] = p_element->pulse[i];
            }
            break;
    }
}

void update_pulses(void) {
    for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
        if (!(channels_active & (1UL << channel))) continue;    // Channel isn't active
        pattern_playback_t pb = pattern_playback[channel];
        if (!pb.p_pattern) continue;    // No pattern selected
        zappy_pattern_element_t const *p_element = &pb.p_pattern->elements[pb.element_index];
        zappy_pattern_element_t const *p_next_element = &pb.p_pattern->elements[(pb.element_index + 1)
                                                                                % pb.p_pattern->element_count];
        uint32_t elapsed = ms_timestamp() - pb.element_start;
        uint16_t adj = pattern_adjusts[channel];
        uint16_t completion = 0;
        switch (pb.p_pattern->pattern_adjust.algorithm) {
            case ADJUST_PLAYBACK_SPEED: {
                // TODO: Change parameter interpretation algorithm.
                //      Pattern adjust should only increase playback speed, because intensity scales with speed.
                //      Keep general curve shape, but have the adjust parameter control the max speed multiplier.
                //      Allowing pattern adjust to slow patterns down isn't useful.

                // Scale up pattern adjust parameter to uint16 range
                uint16_t adj_offset = (pb.p_pattern->pattern_adjust.parameters << 4);
                int32_t offset = adj - adj_offset;
                uint32_t offset_squared = offset * offset;
                uint32_t scaler;
                if (adj_offset > HALF_PATTERN_ADJUST) {
                    scaler = (4 * (uint32_t) adj_offset) / 7 - (offset_squared / 2) / adj_offset;
                } else {
                    scaler = (4 * (uint32_t) (MAX_PATTERN_ADJUST - adj_offset)) / 7 -
                                  (offset_squared / 2) / (MAX_PATTERN_ADJUST - adj_offset);
                }
                uint32_t duration;
                if (offset < 0) {
                    // Slow pattern down. Want a positive duration, so subtract offset
                    duration = (scaler - offset) * p_element->duration / scaler;
                } else {
                    // Speed pattern up.
                    duration = scaler * p_element->duration / (scaler + offset);
                }
                if (!duration || elapsed >= duration) {
                    iter_element(channel);
                } else {
                    completion = 0x10000 * elapsed / duration;
                    interpolate_pulse(completion, p_element, p_next_element);
                }
            }
                break;
            case ADJUST_PULSE_PERIOD: {
                if (!p_element->duration || elapsed >= p_element->duration) {
                    iter_element(channel);
                } else {
                    completion = 0x10000 * elapsed / p_element->duration;
                    interpolate_pulse(completion, p_element, p_next_element);
                }
                uint16_t offset =
                    (pulse_cache[max_pulse_index(channel)] - MIN_PULSE_VALUE) * (uint32_t) adj /
                    (MAX_PATTERN_ADJUST);
                // Subtract pattern adjust value from all pulses ensuring the min pulse value will be > MIN_PULSE_VALUE.
                for (uint8_t i = 0; i < PULSE_EDGES; i++) {
                    if (pulse_cache[i] != 0) {
                        pulse_cache[i] -= offset;
                    }
                }
            }
                break;
            default:
            case ADJUST_IGNORED:
                // Ignore pattern adjust, use unmodified pattern pulses.
                if (elapsed > p_element->duration) {
                    iter_element(channel);
                } else {
                    completion = 0x10000 * elapsed / p_element->duration;
                    interpolate_pulse(completion, p_element, p_next_element);
                }
                break;
        }
        set_pulse(channel, pulse_cache, *power_mod_cache);
        pattern_progress[channel] = (0x10000 * pb.element_index + completion) / pb.p_pattern->element_count;
    }
}

nrfx_err_t pattern_play(uint8_t channel, uint16_t index) {
    if (index) {
        zappy_pattern_t *p_pattern = NULL;
        nrfx_err_t err = get_nth_pattern(&p_pattern, index);
        if (err != NRFX_SUCCESS) return err;
        // Set pattern adjust to 'neutral' when starting new pattern
        pattern_adjusts[channel] = 0;
        pattern_playback[channel].p_pattern = p_pattern;
        pattern_playback[channel].pattern_index = index;
        pattern_playback[channel].element_index = 0;
        pattern_playback[channel].element_start = ms_timestamp();
        APP_ERROR_CHECK(app_sched_event_put(NULL, 0, SCHED_FN(update_adjusts)));
    } else {
        pattern_playback[channel].p_pattern = NULL;
        pattern_playback[channel].pattern_index = 0;
    }
    return NRFX_SUCCESS;
}

static void queued_pattern_init(void) {
    for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
        next_pattern_index(&pattern_playback[channel].pattern_index, false);
        pattern_play(channel, pattern_playback[channel].pattern_index);
    }
}

void patterns_init(void) {
    // next_pattern_index relies on storage being initialized, which is async.
    // Ensure pattern_init is called after storage_init
    APP_ERROR_CHECK(app_sched_event_put(NULL, 0, SCHED_FN(queued_pattern_init)));
}
