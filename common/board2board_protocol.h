//
// Created by Benjamin Riggs on 10/30/20.
//

#ifndef BOARD2BOARD_PROTOCOL_H
#define BOARD2BOARD_PROTOCOL_H

#include "crc8.h"

/** @brief SPI Comms Scheme
 *
 * SPI Mode 1, 8 MHz, LSb first.
 *
 * When the SPI device (slave) doesn't have new data ready to transmit,
 *  it shall send busy bytes of 0xFF.
 * When the SPI host (host) clocks more bytes than the device has available to transmit,
 *  or when the host has fewer bytes to send than to receive from the device,
 *  it shall send pad bytes of 0x00.
 *
 * Message Header, 4 bytes:
 *      Bytes 0-1:
 *          Bit 0: Parity bit to give byte 0 even parity
 *          Bit 1: Header CRC error flag
 *          Bit 2: Payload CRC error flag
 *          Bit 3: 0
 *          Bits 4-15: Size of entire message, as Uint12LE
 *      Byte 2: 8-bit CRC of previous 2 bytes
 *      Byte 3: 8-bit CRC of Data
 * Message Payload: (CRC byte in header so Data is word-aligned)
 *      Bytes 4-N: Data
 *
 * Algorithm:
 *
 * TRANSFER 1:
 *  Host (master) sends complete message.
 *  Device (slave) sends any applicable bytes.
 *
 * RESPONSE 1:
 *  Host verifies Size header send by Device.
 *  Device compares its message Size to number of bytes sent, verifies Host message Size.
 *
 * TRANSFER 2:
 *  If Host received corrupt header from Device:
 *      Host sets appropriate bit flag and send 1 byte, ignoring byte from Device.
 *  Else:
 *  If Device message Size is <= bytes sent (Host message size):
 *      Host has received complete message from Device.
 *      Proceed to MESSAGE VERIFICATION.
 *  If Device message Size is > bytes sent:
 *      Host adjusts buffer pointer to receive remaining message bytes.
 *      Host clocks remaining number of bytes + 1 in one transfer, re-sending partial Host message,
 *          or complete Host message plus pad bytes, as applicable.
 *      Device sends remaining message data + pad byte.
 *      If Device received corrupted Host message:
 *          Device overwrites previous message with newly received data.
 *      Else:
 *          Device discards newly received data.
 *
 * RESPONSE 2:
 *  If Host only sent 1 byte:
 *      Host resets to TRANSFER 1.
 *      Device verifies Host message.
 *      If Device received corrupted Host message:
 *          Device resets to TRANSFER 1.
 *      Else:
 *          Device resets to TRANSFER 1, but ignores received data.
 *  Else, previous Device message was incomplete:
 *      Device verifies Host message.
 *      Host has received complete message from Device, verifies message.
 *      Proceed to MESSAGE VERIFICATION
 *
 * MESSAGE VERIFICATION:
 *  Host sends 1 byte with appropriate status bits set.
 *  Device sends 1 byte with appropriate status bits set.
 *  If no errors:
 *      Host & Device act upon received messages.
 *  If both Host & Device report errors:
 *      Reset to TRANSFER 1, but Device message Size is known, so Host can clock appropriate number of bytes.
 *  If only Device reports an error:
 *      Device resets to TRANSFER 1.
 *      Host resets to TRANSFER 1, but ignore received data.
 *  If only Host reports an error:
 *      Device resets to TRANSFER 1, but ignores received data.
 *      Host resets to TRANSFER 1, but receives bytes equal to Device message Size.
 */

#define BOARD2BOARD_MSG_HEADER_LENGTH       4U
#define BOARD2BOARD_MSG_LENGTH_SHIFT        4U
// 12-bit number, 1 less than buffer max
#define BOARD2BOARD_MSG_MAX_LENGTH          0xFFF
#define BOARD2BOARD_PAYLOAD_MAX_LENGTH  (BOARD2BOARD_MSG_MAX_LENGTH - BOARD2BOARD_MSG_HEADER_LENGTH)

#define BOARD2BOARD_MSG_HEADER_CRC_ERROR    (1U << 1)
#define BOARD2BOARD_MSG_PAYLOAD_CRC_ERROR   (1U << 2)
#define BOARD2BOARD_ERROR_FLAGS             (BOARD2BOARD_MSG_HEADER_CRC_ERROR | BOARD2BOARD_MSG_PAYLOAD_CRC_ERROR)

#define BOARD2BOARD_RESERVED_BIT            (1U << 3)
#define BOARD2BOARD_DEVICE_BUSY_BYTE        0xFF
#define BOARD2BOARD_PAD_BYTE                0x00

extern crc8_t g_crc8;

static inline bool board2board_validate_status(uint8_t volatile *buffer) {
    ASSERT(buffer);
    return !__builtin_parity(buffer[0]) && !(buffer[0] & BOARD2BOARD_RESERVED_BIT);
}

static inline bool board2board_validate_header(uint8_t volatile *buffer) {
    return board2board_validate_status(buffer) && (buffer[2] == crc8_calc(&g_crc8, (uint8_t *) buffer, 2));
}

static inline uint16_t board2board_get_msg_length(uint8_t volatile *buffer) {
    return board2board_validate_header(buffer) * (((uint16_t *) buffer)[0] >> BOARD2BOARD_MSG_LENGTH_SHIFT);
}

static inline uint8_t *board2board_get_payload(uint8_t volatile *buffer) {
    ASSERT(buffer);
    return (uint8_t *) (buffer + BOARD2BOARD_MSG_HEADER_LENGTH);
}

static inline uint16_t board2board_get_payload_length(uint8_t volatile *buffer) {
    uint16_t len = board2board_get_msg_length(buffer);
    return (len > BOARD2BOARD_MSG_HEADER_LENGTH) * (len - BOARD2BOARD_MSG_HEADER_LENGTH);
}

static inline bool board2board_validate_payload(uint8_t volatile *buffer) {
    uint16_t len = board2board_get_payload_length(buffer);
    return len == 0 || buffer[3] == crc8_calc(&g_crc8, (uint8_t *) (buffer + BOARD2BOARD_MSG_HEADER_LENGTH), len);
}

static inline void board2board_set_header(uint8_t volatile *buffer, uint16_t payload_length) {
    ASSERT(buffer);
    ASSERT(payload_length <= BOARD2BOARD_PAYLOAD_MAX_LENGTH);
    // Preserve any error flags to avoid losing SPI transaction state.
    ((uint16_t *) buffer)[0] = ((payload_length + BOARD2BOARD_MSG_HEADER_LENGTH) << BOARD2BOARD_MSG_LENGTH_SHIFT) |
                               (buffer[0] & BOARD2BOARD_ERROR_FLAGS);
    buffer[0] |= __builtin_parity(buffer[0]);
    buffer[2] = crc8_calc(&g_crc8, (uint8_t const *) buffer, 2);
    buffer[3] = crc8_calc(&g_crc8, (uint8_t const *) (buffer + BOARD2BOARD_MSG_HEADER_LENGTH), payload_length);
}

static inline void board2board_set_header_flag(uint8_t volatile *buffer, uint32_t flag) {
    ASSERT(buffer);
    ASSERT(flag & BOARD2BOARD_ERROR_FLAGS);
    buffer[0] |= flag;
    buffer[0] &= ~1U;
    buffer[0] |= __builtin_parity(buffer[0]);
}

static inline void board2board_set_header_error(uint8_t volatile *buffer) {
    board2board_set_header_flag(buffer, BOARD2BOARD_MSG_HEADER_CRC_ERROR);
}

static inline void board2board_set_payload_error(uint8_t volatile *buffer) {
    board2board_set_header_flag(buffer, BOARD2BOARD_MSG_PAYLOAD_CRC_ERROR);
}

static inline void board2board_clear_error_flags(uint8_t volatile *buffer) {
    ASSERT(buffer);
    buffer[0] &= ~BOARD2BOARD_ERROR_FLAGS;
    buffer[0] &= ~1U;
    buffer[0] |= __builtin_parity(buffer[0]);
}

#endif //BOARD2BOARD_PROTOCOL_H
