//
// Created by Benjamin Riggs on 9/3/20.
//

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include "app_config.h"
#include "patterns.h"

/** @brief Message protocol description.
 *
 *  Command messages consist of an opcode, channel selector bitfield or pattern index (as applicable), and the payload.
 *  Response messages consist of the echoed command opcode, a response opcode, and the payload.
 *
 *  The command channel selector bitfield is a indicates to which channels to apply the chose operation.
 *
 * @details Opcode and message detailed descriptions:
 *
 *  DEVICE_INFO          Retrieves device version and identification information.
 *      Command:
 *          Header: { DEVICE_INFO, ignored }
 *          Payload: None
 *      Response:
 *          Header: { DEVICE_INFO, SUCCESS }
 *          Payload: TODO
 *
 *
 *  DEVICE_STATUS        Retrieves number of patterns on device and pattern indices currently being played.
 *      Command:
 *          Header: { DEVICE_STATUS, ignored }
 *          Payload: None
 *      Response
 *          Header: { DEVICE_STATUS, SUCCESS }
 *          Payload: zappy_status_msg_t
 *
 *
 *  PAUSE                Stops all output on selected channels and freezes channel state.
 *      Command:
 *          Header: { PAUSE, channel selector bitfield }
 *          Payload: None
 *      Response:
 *          Header: { PAUSE, SUCCESS if valid channel selected or else NOOP }
 *          Payload: None
 *
 *
 *  RESUME               Resumes playback from frozen state on selected channel.
 *      Command:
 *          Header: { RESUME, channel selector bitfield }
 *          Payload: None
 *      Response:
 *          Header: { PAUSE, SUCCESS if valid channel selected or else NOOP }
 *          Payload: None
 *
 *
 *  GET_POWERS           Retrieves power levels of all channels.
 *      Command:
 *          Header: { GET_POWERS, ignored }
 *          Payload: None
 *      Response:
 *          Header: { GET_POWERS, SUCCESS }
 *          Payload: Uint16LE per channel
 *
 *
 *  SET_POWERS          Sets the power levels of selected channels. The command payload is an array of values each
 *                      corresponding to a single channel. The channels selected by the bitfield will be updated to
 *                      the values specified in the payload. The order of values and bitfields are the same.
 *
 *                      If any channels are updated, the response payload will contain the actual values set on the
 *                      device. If no valid channels are selected, the response retcode will be NOOP and there
 *                      will be no response payload.
 *      Command:
 *          Header: { SET_POWERS, channel selector bitfield }
 *          Payload: Uint16LE per channel
 *      Response:
 *          Header: { SET_POWERS, SUCCESS or NOOP }
 *          Payload: Uint16LE per channel or none
 *
 *
 *  GET_PATTERN_ADJUST   Retrieves pattern adjust values of all channels.
 *      Command:
 *          Header: { GET_PATTERN_ADJUST, ignored }
 *          Payload: None
 *      Response:
 *          Header: { GET_PATTERN_ADJUST, SUCCESS }
 *          Payload: Uint16LE per channel
 *
 *
 *  SET_PATTERN_ADJUST  Sets the pattern adjust value of selected channels. The command payload is an array of
 *                      values each corresponding to a single channel. The channels selected by the bitfield will
 *                      be updated to the values specified in the payload. The order of values and bitfields are
 *                      the same.
 *
 *                      If any channels are updated, the response payload will contain the actual values set on the
 *                      device. If no valid channels are selected, the response retcode will be NOOP and there
 *                      will be no response payload.
 *      Command:
 *          Header: { SET_PATTERN_ADJUST, channel selector bitfield }
 *          Payload: Uint16LE per channel
 *      Response:
 *          Header: { SET_PATTERN_ADJUST, SUCCESS or NOOP if no valid channels selected }
 *          Payload: Uint16LE per channel or none
 *
 *
 *  GET_PULSE            Retrieves current pulse for the lowest selected channel.
 *      Command:
 *          Header: { GET_PULSE, channel selector bitfield }
 *          Payload: None
 *      Response:
 *          Header: { GET_PULSE, channel bitflag for payload or NOOP if invalid channels selected }
 *          Payload: 4x Uint16LE pulse timer values, in microseconds
 *
 *
 *  SET_PULSE            Set pulse timer values for selected channels
 *      Command:
 *          Header: { SET_PULSE, channel selector bitfield }
 *          Payload: 4x Uint16LE pulse timer values, in microseconds
 *      Response:
 *          Header: { SET_PULSE, SUCCESS if any selected channels are valid or else NOOP }
 *          Payload: None
 *
 *  @note:
 *      Pulse timer values specify when pulse edge is triggered, counting from the end of prior pulse.
 *      A pulse ends after the last edge of the prior pulse is triggered, which resets timer to zero.
 *      Pulse timer value order is { positive_toggle, positive_toggle, negative_toggle, negative_toggle }.
 *      Pulse timer values of less than MIN_PULSE_VALUE are ignored and will not trigger their respective edge.
 *      Use with care.
 *
 *
 *  GET_PATTERN          Retrieves the pattern stored on the device at a given index, if any. An invalid pattern
 *                       index will result in ERROR_INVALID_INDEX retcode. Patterns are 1-indexed.
 *      Command:
 *          Header: { GET_PATTERN, pattern_index_t index }
 *          Payload: None
 *      Response:
 *          Header: { GET_PATTERN, SUCCESS or ERROR_INVALID_INDEX }
 *          Payload: If SUCCESS, pattern is returned, otherwise no payload is returned
 *
 *
 *  GET_PATTERN_TITLE    Retrieves the pattern title on the device at a given index, if any. An invalid pattern
 *                       index will result in ERROR_INVALID_INDEX retcode. Patterns are 1-indexed.
 *      Command:
 *          Header: { GET_PATTERN_TITLE, pattern_index_t index }
 *          Payload: None
 *      Response:
 *          Header: { GET_PATTERN_TITLE, SUCCESS or ERROR_INVALID_INDEX }
 *          Payload: If SUCCESS, pattern title (23 bytes) is returned, otherwise no payload is returned
 *
 *
 *  PLAY_PATTERN         Plays the indicated pattern on the selected channels. If a selected channel is paused,
 *                       it will be resumed with the new pattern. An invalid pattern index will result in
 *                       ERROR_INVALID_INDEX retcode. An invalid channel selector will result in NOOP retcode.
 *                       Patterns are 1-indexed.
 *      Command:
 *          Header: { PLAY_PATTERN, channel selector bitfield }
 *          Payload: pattern_index_t index
 *      Response:
 *          Header: { GET_PATTERN, SUCCESS or ERROR_INVALID_INDEX or NOOP }
 *          Payload: None
 *
 *
 *  GET_PATTERN_PROGRESS    Retrieves the progress of a pattern playback on a channel as a percentage
 *                          of 0x10000. Because 0x10000 is functionally the same as 0, only values
 *                          0x0000 - 0xFFFF are returned. Non-playing channels will return 0.
 *      Command:
 *          Header: { GET_PATTERN_PROGRESS, ignored }
 *          Payload: None
 *      Response:
 *          Header: { GET_PATTERN_PROGRESS, SUCCESS }
 *          Payload: Uint16LE per channel
 *
 *
 *  SET_PATTERN_PROGRESS    Sets the pattern progress of selected channels. The command payload is an array of
 *                          values each corresponding to a single channel. The channels selected by the bitfield
 *                          will be updated to the values specified in the payload. The order of values and
 *                          bitfields are the same.
 *
 *                          Values are percentage of 0x10000 of the time progress of pattern playback.
 *                          Because 0x10000 is functionally the same as 0, only values 0x0000 - 0xFFFF
 *                          are returned. Non-playing channels will return 0.
 *      Command:
 *          Header: { SET_PATTERN_PROGRESS, channel selector bitfield }
 *          Payload: Uint16LE per channel
 *      Response:
 *          Header: { SET_PATTERN_PROGRESS, SUCCESS or NOOP }
 *          Payload: Uint16LE per channel or none
 *
 *
 *  INSERT_PATTERN       Inserts the provided pattern at the selected index. Patterns are 1-indexed. An index value
 *                       of zero, or an index value greater than the total pattern count will add the pattern
 *                       immediately after the highest index.
 *
 *                       A retcode of ERROR_BUSY indicates that flash storage is busy and the pattern cannot be
 *                       stored at this time. A retcode of ERROR_NO_MEMORY indicates there is insufficient free
 *                       storage available to store this pattern.
 *      Command:
 *          Header: { INSERT_PATTERN, pattern_index_t index }
 *          Payload: zappy_pattern_t pattern
 *      Response:
 *          Header: { INSERT_PATTERN, SUCCESS or ERROR_BUSY or ERROR_NO_MEMORY }
 *          Payload: None
 *
 *
 *  DELETE_PATTERN       Deletes the pattern stored at the selected index. Patterns are 1-indexed. A retcode of
 *                       ERROR_BUSY indicates that flash storage is busy and the pattern cannot be deleted at
 *                       this time. A retcode of ERROR_INVALID_INDEX indicates there is no pattern at the
 *                       provided index.
 *      Command:
 *          Header: { DELETE_PATTERN, pattern_index_t index }
 *          Payload: None
 *      Response:
 *          Header: { DELETE_PATTERN, SUCCESS or ERROR_BUSY or ERROR_INVALID_INDEX }
 *          Payload: None
 *
 *
 *  DELETE_ALL_PATTERNS  Deletes every pattern stored on the device. For opcode to have effect, the command header
 *                       index value must match the opcode and the command payload must match the command header.
 *      Command:
 *          Header: { DELETE_ALL_PATTERNS, DELETE_ALL_PATTERNS }
 *          Payload: { DELETE_ALL_PATTERNS, DELETE_ALL_PATTERNS }
 *      Response:
 *          Header: { DELETE_ALL_PATTERNS, SUCCESS or ERROR_BUSY or NOOP }
 *          Payload: None
 *
 *
 *   DISPLAY_INTENSITIES    Updates the GUI with the current channel intensities. Values are a percentage multiplied
 *                          by 0xFFFF.
 *      Command:
 *          Header: { DISPLAY_INTENSITIES, channel selector bitfield }
 *          Payload: Uint16LE per channel
 *      Response:
 *          Header: { DISPLAY_INTENSITIES, SUCCESS or ERROR_INVALID_INDEX }
 *          Payload: None
 */

#define OPCODE_TABLE \
/* Global device state retrieval and control */ \
    X(OP_DEVICE_INFO, 0x01) \
    X(OP_DEVICE_STATUS, 0x02) \
    X(OP_PAUSE, 0x08) \
    X(OP_RESUME, 0x09) \
    X(OP_GET_POWERS, 0x0A) \
    X(OP_SET_POWERS, 0x0B) \
    X(OP_GET_PATTERN_ADJUST, 0x0C) \
    X(OP_SET_PATTERN_ADJUST, 0x0D) \
/* Low-level pulse control */ \
    X(OP_GET_PULSE, 0x11) \
    X(OP_SET_PULSE, 0x12) \
/* Pattern info & playback */ \
    X(OP_GET_PATTERN, 0x20) \
    X(OP_PLAY_PATTERN, 0x21) \
    X(OP_GET_PATTERN_TITLE, 0x22)              \
    X(OP_GET_PATTERN_PROGRESS, 0x28)           \
    X(OP_SET_PATTERN_PROGRESS, 0x29)           \
/* Pattern storage controls */                  \
    X(OP_INSERT_PATTERN, 0x40)                 \
    X(OP_DELETE_PATTERN, 0x60)                 \
    X(OP_DELETE_ALL_PATTERNS, 0x6AAC)          \
/* Ensure at least 1 value is 16-bit for __packed enum */ \
/* GUI shows rough intensity */                 \
    X(OP_DISPLAY_INTENSITIES, 0x8000)

#define RESPONSE_CODE_TABLE \
    X(OP_SUCCESS, 0x00) \
    X(OP_ERROR_PARSE_ERROR, 0xFFFB) \
    X(OP_ERROR_INVALID_INDEX, 0xFFFC) \
    X(OP_ERROR_NO_MEMORY, 0xFFFD) \
    X(OP_ERROR_BUSY, 0xFFFE) \
    X(OP_NOOP, UINT16_MAX) /* Max opcode value, keep last. */

typedef enum __packed {
    #define X(name, num) name = num,
    OPCODE_TABLE
    #undef X
} serial_opcode_t;

typedef enum __packed {
    #define X(name, num) name = num,
    RESPONSE_CODE_TABLE
    #undef X
} serial_response_code_t;

typedef uint16_t pattern_index_t;

typedef struct __packed {
    serial_opcode_t opcode;
    union {
        uint16_t channels;
        pattern_index_t index;
        serial_response_code_t retcode;
    };
    // Must be word-aligned
    uint16_t payload[];
} zappy_msg_t;

typedef struct __packed {
    uint32_t pattern_count;
    pattern_index_t patterns_playing[DEVICE_CHANNEL_COUNT];
} zappy_status_msg_t;

#define MSG_HEADER_SIZE (sizeof(zappy_msg_t))
#define MSG_PAYLOAD_MAX_SIZE (sizeof(pattern_index_t) + (MAX_PATTERN_BYTE_LENGTH))
#define MSG_MAX_SIZE ((MSG_HEADER_SIZE) + (MSG_PAYLOAD_MAX_SIZE))

#endif //SERIAL_PROTOCOL_H
