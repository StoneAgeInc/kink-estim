//
// Created by Benjamin Riggs on 9/3/20.
//

#include <memory.h>

#include "prv_serial_parser.h"
#include "serial_protocol.h"
#include "storage.h"
#include "pattern_control.h"
#include "pulse_control.h"
#include "board2board_host.h"
#include "usb_serial.h"
#include "audio_adc.h"

size_t serial_parse(uint8_t *p_data, size_t length, prv_serial_response_t serial_response, void *p_ctx) {
    ASSERT(p_data);
    ASSERT(length);
    static uint8_t output_buffer[MSG_MAX_SIZE] = {0};
    // Address is const, content is volatile
    static zappy_msg_t *const response = (zappy_msg_t *) output_buffer;
    static size_t response_length = 0;

    #define FORWARD_TO_DISPLAY  (1U << 0)
    #define FORWARD_TO_USB_ACM  (1U << 1)
    size_t forward_targets = FORWARD_TO_DISPLAY * (serial_response != (prv_serial_response_t) board2board_host_send) +
                             FORWARD_TO_USB_ACM * (serial_response != usb_serial_send);
    #define FORWARD(data, length)                                                   \
    if (forward_targets & FORWARD_TO_DISPLAY) board2board_host_send(data, length);  \
    if (forward_targets & FORWARD_TO_USB_ACM) usb_serial_send(data, length, NULL)

    size_t remaining_length = length;
    #define REQUIRE_LENGTH(_len)            \
    if (remaining_length < (_len)) {        \
        return (_len) - remaining_length;   \
    } else {                                \
        remaining_length -= _len;           \
    }
    REQUIRE_LENGTH(MSG_HEADER_SIZE);
    response_length = MSG_HEADER_SIZE;
    zappy_msg_t *command = (zappy_msg_t *) p_data;
    response->opcode = command->opcode;
    response->retcode = OP_NOOP;
    switch (command->opcode) {
        case OP_DEVICE_INFO:
            response->retcode = OP_SUCCESS;
            // TODO: Serial number, firmware version, pattern version
            // TODO: Update length
            break;
        case OP_DEVICE_STATUS: {
            zappy_status_msg_t *status = (zappy_status_msg_t *) response->payload;
            status->pattern_count = pattern_storage_count;
            for (int channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (pattern_playback[channel].p_pattern) {
                    status->patterns_playing[channel] = pattern_playback[channel].pattern_index;
                } else {
                    status->patterns_playing[channel] = 0;
                }
            }
            response->retcode = OP_SUCCESS;
            response_length += sizeof(zappy_status_msg_t);
        }
            break;
        case OP_PAUSE: {
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (command->channels & (1UL << channel)) {
                    pulse_pause(channel);
                    response->retcode = OP_SUCCESS;
                }
            }
            if (response->retcode == OP_SUCCESS) {
                FORWARD(p_data, length);
            }
        }
            break;
        case OP_RESUME: {
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (command->channels & (1UL << channel)) {
                    pulse_resume(channel);
                    response->retcode = OP_SUCCESS;
                }
            }
            if (response->retcode == OP_SUCCESS) {
                FORWARD(p_data, length);
            }
        }
            break;
        case OP_GET_PULSE: {
            zappy_pulse_t *output_pulse = (zappy_pulse_t *) response->payload;
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                // Only returns the first channel found
                if (command->channels & (1UL << channel)) {
                    memcpy(output_pulse, (void *) pulse_values(channel), sizeof(zappy_pulse_t));
                    response->channels = 1UL << channel;
                    response_length += sizeof(zappy_pulse_t);
                    break;
                }
            }
        }
            break;
        case OP_SET_PULSE: {
            REQUIRE_LENGTH(sizeof(zappy_pulse_t));
            zappy_pulse_t *input_pulse = (zappy_pulse_t *) command->payload;
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (command->channels & (1UL << channel)) {
                    set_pulse(channel, *input_pulse, 0);
                    response->retcode = OP_SUCCESS;
                }
            }
        }
            break;
        case OP_GET_POWERS: {
            memcpy((void *) response->payload, (void *) power_levels, sizeof(zappy_power_levels_t));
            response->retcode = OP_SUCCESS;
            response_length += sizeof(zappy_power_levels_t);
        }
            break;
        case OP_SET_POWERS: {
            REQUIRE_LENGTH(sizeof(zappy_power_levels_t));
            zappy_power_levels_t *input_levels = (zappy_power_levels_t *) command->payload;
            zappy_power_levels_t *output_levels = (zappy_power_levels_t *) response->payload;
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (command->channels & (1UL << channel)) {
                    (*output_levels)[channel] = set_power(channel, (*input_levels)[channel]);
                    response->retcode = OP_SUCCESS;
                }
            }
            if (response->retcode == OP_SUCCESS) {
                response_length += sizeof(zappy_power_levels_t);
                FORWARD(p_data, length);
            }
        }
            break;
        case OP_GET_PATTERN_ADJUST: {
            memcpy((void *) response->payload, pattern_adjusts,
                   sizeof(zappy_pattern_adjusts_t));
            response->retcode = OP_SUCCESS;
            response_length += sizeof(zappy_pattern_adjusts_t);
        }
            break;
        case OP_SET_PATTERN_ADJUST: {
            REQUIRE_LENGTH(sizeof(zappy_pattern_adjusts_t));
            zappy_pattern_adjusts_t *input_adjusts = (zappy_pattern_adjusts_t *) command->payload;
            zappy_pattern_adjusts_t *output_adjusts = (zappy_pattern_adjusts_t *) response->payload;
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (command->channels & (1UL << channel)) {
                    pattern_adjusts[channel] = MIN((*input_adjusts)[channel], MAX_PATTERN_ADJUST);
                    (*output_adjusts)[channel] = pattern_adjusts[channel];
                    response->retcode = OP_SUCCESS;
                }
            }
            if (response->retcode == OP_SUCCESS) {
                response_length += sizeof(zappy_pattern_adjusts_t);
                FORWARD(p_data, length);
            }
        }
            break;
        case OP_PLAY_PATTERN: {
            REQUIRE_LENGTH(sizeof(pattern_index_t));
            pattern_index_t index = ((pattern_index_t *) command->payload)[0];
            for (uint8_t channel = 0; channel < DEVICE_CHANNEL_COUNT; channel++) {
                if (command->channels & (1UL << channel)) {
                    nrfx_err_t err = pattern_play(channel, index);
                    switch (err) {
                        case NRFX_ERROR_INVALID_ADDR:
                            response->retcode = OP_ERROR_INVALID_INDEX;
                            break;
                        case NRFX_SUCCESS:
                            response->retcode = OP_SUCCESS;
                            break;
                        default:
                            APP_ERROR_CHECK(err);
                            break;
                    }
                    pulse_resume(channel);
                }
            }
            if (response->retcode == OP_SUCCESS) {
                FORWARD(p_data, length);
            }
        }
            break;
        case OP_GET_PATTERN: {
            pattern_index_t nth = (pattern_index_t) command->index;
            zappy_pattern_t *p_pattern = NULL;
            nrfx_err_t err = get_nth_pattern(&p_pattern, nth);
            switch (err) {
                case NRFX_ERROR_INVALID_ADDR:
                    response->retcode = OP_ERROR_INVALID_INDEX;
                    break;
                case NRFX_SUCCESS: {
                    size_t pattern_len = ZAPPY_PATTERN_SIZE(p_pattern);
                    memcpy((void *) response->payload, p_pattern, pattern_len);
                    response->retcode = OP_SUCCESS;
                    response_length += pattern_len;
                }
                    break;
                default:
                    APP_ERROR_CHECK(err);
                    break;
            }
        }
            break;
        case OP_GET_PATTERN_TITLE: {
            pattern_index_t nth = (pattern_index_t) command->index;
            zappy_pattern_t *p_pattern = NULL;
            nrfx_err_t err = get_nth_pattern(&p_pattern, nth);
            switch (err) {
                case NRFX_ERROR_INVALID_ADDR:
                    response->retcode = OP_ERROR_INVALID_INDEX;
                    break;
                case NRFX_SUCCESS:
                    memcpy((void *) response->payload, p_pattern->title, MAX_PATTERN_TITLE_CHARACTER_COUNT);
                    response_length += MAX_PATTERN_TITLE_CHARACTER_COUNT;
                    response->retcode = OP_SUCCESS;
                    break;
                default:
                    APP_ERROR_CHECK(err);
                    break;
            }
        }
            break;
        case OP_INSERT_PATTERN: {
            pattern_index_t idx = (pattern_index_t) command->index;
            zappy_pattern_t *p_pattern = (zappy_pattern_t *) command->payload;
            // Ensure pattern header is present before attempting to calculate pattern size
            REQUIRE_LENGTH(ZAPPY_PATTERN_HEADER_SIZE);
            // TODO: Validate pattern version
            REQUIRE_LENGTH(ZAPPY_PATTERN_ELEMENTS_SIZE(p_pattern));
            nrfx_err_t err = insert_pattern(p_pattern, idx);
            switch (err) {
                case NRFX_ERROR_BUSY:
                    response->retcode = OP_ERROR_BUSY;
                    break;
                case NRFX_ERROR_NO_MEM:
                    response->retcode = OP_ERROR_NO_MEMORY;
                    break;
                case NRFX_SUCCESS:
                    response->retcode = OP_SUCCESS;
                    break;
                default:
                    APP_ERROR_CHECK(err);
                    break;
            }
        }
            break;
        case OP_DELETE_PATTERN: {
            pattern_index_t nth = (pattern_index_t) command->index;
            nrfx_err_t err = delete_pattern(nth);
            switch (err) {
                case NRFX_ERROR_BUSY:
                    response->retcode = OP_ERROR_BUSY;
                    break;
                case NRFX_ERROR_INVALID_ADDR:
                    response->retcode = OP_ERROR_INVALID_INDEX;
                    break;
                case NRFX_SUCCESS:
                    response->retcode = OP_SUCCESS;
                    break;
                default:
                    APP_ERROR_CHECK(err);
                    break;
            }
        }
            break;
        case OP_DELETE_ALL_PATTERNS: {
            REQUIRE_LENGTH(MSG_HEADER_SIZE);
            if (command->index != OP_DELETE_ALL_PATTERNS ||
                ((serial_opcode_t *) command->payload)[0] != OP_DELETE_ALL_PATTERNS ||
                ((serial_opcode_t *) command->payload)[1] != OP_DELETE_ALL_PATTERNS) {
                break;
            }
            nrfx_err_t err = delete_all_patterns();
            switch (err) {
                case NRFX_ERROR_BUSY:
                    response->retcode = OP_ERROR_BUSY;
                    break;
                case NRFX_SUCCESS:
                    response->retcode = OP_SUCCESS;
                    break;
                default:
                    APP_ERROR_CHECK(err);
                    break;
            }
        }
            break;
#if 0
        /// Enable audio ADC to USB serial streaming.
        case 0x6969: {
            audio_adc_start();
            serial_response = NULL;
            break;
        }
        /// Disable audio ADC to USB serial streaming.
        case 0x696A: {
            audio_adc_stop();
            response->retcode = OP_SUCCESS;
            break;
        }
#endif
        default:
            break;
    }
    if (serial_response) serial_response((uint8_t *) output_buffer, response_length, p_ctx);
    return 0;
}

void serial_parser_init(void) {
}
