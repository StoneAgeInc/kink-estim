//
// Created by Benjamin Riggs on 3/10/20.
//

#ifndef PATTERNS_H
#define PATTERNS_H

#include <stdlib.h>
#include <stdint.h>

/// Max byte length is 4076 due to flash page size & storage library metadata.
#define MAX_PATTERN_BYTE_LENGTH 4076
#define MAX_PATTERN_ELEMENT_COUNT 337
#define MAX_PATTERN_TITLE_CHARACTER_COUNT (sizeof(zappy_pattern_title_t) - 1)

#define PULSE_EDGES 4
typedef uint16_t zappy_pulse_t[PULSE_EDGES];

/// Don't define more than 16 functions
typedef enum __packed {
    EASING_LINEAR = 0x0,           /**< Linear interpolation between each of the 4 pulse values independently */
    // TODO: Implement some subset of https://easings.net/en
    // Max value
    EASING_NONE = 0xF              /**< No interpolation between pattern elements. */
} easing_function_t;

/// Don't define more than 16 functions
typedef enum __packed {
    ADJUST_PLAYBACK_SPEED = 0x0,   /**< Pattern adjust value acts as a multiplier to duration. */
    ADJUST_PULSE_PERIOD = 0x1,     /**< Pattern adjust value is directly subtracted from all pulse values. */
    // Max value
    ADJUST_IGNORED = 0xF           /**< Pattern element ignores adjust value. */
} pattern_adjust_algorithm_t;

typedef struct __packed {
    pattern_adjust_algorithm_t  algorithm  :  4;
    uint16_t                    parameters : 12;
} pattern_adjust_t;

/**@brief   Pattern element definition for zappy box
 *
 * A pattern is defined by up to 253 sequential pattern elements. Each element defines a pulse state and
 * parameters for how to transition to the next pulse state.
 *
 * @note Due to Pattern Adjust algorithms, durations greater than 61166 ms may overflow.
 */
typedef struct __packed {
    zappy_pulse_t       pulse;         /**< Pulse definition of pattern element. */
    uint16_t            duration;      /**< Duration of transition between this pulse and next, in milliseconds. */
    easing_function_t   easing : 4;    /**< Easing function defining the nature of the transition between elements. */
    uint16_t            power_modulator : 12;  /**< Amount by which power output is modulated. Algorithm:
                                                *                                (0xFFF - element.power_modulator)
                                                * output_power = channel_power * ---------------------------------
                                                *                                              0xFFF              */
} zappy_pattern_element_t;         // 12 bytes total

/**@brief   Version half-word for zappy patterns */
typedef struct __packed {
    uint16_t patch : 6;
    uint16_t minor : 6;
    uint16_t major : 4;
} zappy_pattern_version_t;

/**@brief The highest level pattern understood by this firmware. */
#define ZAPPY_PATTERN_VERSION   \
{                               \
    .major = 1;                 \
    .minor = 0;                 \
    .patch = 0;                 \
};

/* 4076
 *  - sizeof(pattern_adjust) = 2
 *  - sizeof(element_count) = 2
 *  - sizeof(length) = 2
 *  - 253 * sizeof(zappy_pattern_element_t) = 337 * 12
 *  - sizeof(zappy_pattern_version_t) = 2
 * = 24
 */
/// 23 characters + \00
typedef char zappy_pattern_title_t[24];

/** Max allowable pattern size is 4076 bytes because of flash page size on nRF52 */
typedef const struct __packed {
    zappy_pattern_version_t     version;
    pattern_adjust_t            pattern_adjust;     /**< Defines the behavior of pattern adjust values. */
    uint16_t                    element_count;      /**< Max of 337 */
    uint16_t                    length;             /**< Default pattern length in milliseconds */
    zappy_pattern_title_t       title;              /**< Pattern tittle */
    zappy_pattern_element_t     elements[];
} zappy_pattern_t;

#define ZAPPY_PATTERN_HEADER_SIZE sizeof(zappy_pattern_t)
#define ZAPPY_PATTERN_ELEMENTS_SIZE(p_pattern) (sizeof(zappy_pattern_element_t) * ((zappy_pattern_t *) p_pattern)->element_count)
#define ZAPPY_PATTERN_SIZE(p_pattern) (ZAPPY_PATTERN_HEADER_SIZE + ZAPPY_PATTERN_ELEMENTS_SIZE(p_pattern))

#endif //PATTERNS_H
