//
// Created by Benjamin Riggs on 9/29/20.
//

#include "board2board_host.h"
#include "board2board_protocol.h"
#include "pin_config.h"
#include "prv_utils.h"
#include "prv_serial_parser.h"
#include "crc8.h"

#include "nrfx_spim.h"

crc8_t g_crc8;
static crc8_t const *const p_crc8 = (crc8_t *) &g_crc8;

static nrfx_spim_t const spim = NRFX_SPIM_INSTANCE(BOARD2BOARD_SPIM_INSTANCE);
static bool volatile spim_busy = true;
static bool initialized = false;

// SPIM3 DMA area is defined in linker file and isolated for anomaly 198 workaround.
extern uint8_t spim3_rx_buffer[0x1000], spim3_tx_buffer[0x1000];

static nrfx_spim_xfer_desc_t volatile xfer_desc = NRFX_SPIM_XFER_TRX(spim3_tx_buffer, 0, spim3_rx_buffer, 0);
#define SPIM_XFER (nrfx_spim_xfer(&spim, (nrfx_spim_xfer_desc_t const *) &xfer_desc, 0))

static void crc_init(void) {
    crc8_conf_t crc8_conf = CRC8_CONF;
    crc8_init((crc8_t *) p_crc8, &crc8_conf);
}

static void __inline parse_device_message(void) {
    size_t length = board2board_get_payload_length(spim3_rx_buffer);
    if (length) {
        serial_parse(board2board_get_payload(spim3_rx_buffer), length,
                     (prv_serial_response_t) board2board_host_send, NULL);
    }
}

static bool __inline status_is_real(void) {
    if (board2board_validate_status(spim3_rx_buffer)) {
        return true;
    } else if (spim3_rx_buffer[0] == BOARD2BOARD_DEVICE_BUSY_BYTE) {
        /* Device was busy, re-transfer */
    } else {
        /* Corrupt message Header */
        NRF_LOG_DEBUG("Bad first header byte: %2X", spim3_rx_buffer[0]);
        board2board_set_header_error(spim3_tx_buffer);
        xfer_desc.tx_length = 1;
        xfer_desc.rx_length = 1;
    }
    APP_ERROR_CHECK(SPIM_XFER);
    return false;
}

static void __inline validate_payload(void) {
    size_t len;
    if ((len = board2board_get_msg_length(spim3_rx_buffer)) <= BOARD2BOARD_MSG_HEADER_LENGTH) return;
    if (board2board_validate_payload(spim3_rx_buffer)) {
        /* Data CRC passed */
//        NRF_LOG_DEBUG("Successfully received %d bytes:", len);
//        NRF_LOG_HEXDUMP_DEBUG(spim3_rx_buffer, len);
    } else {
        /* Corrupt message data, set error flag. */
        NRF_LOG_DEBUG("Bad CRC (%2X) for payload:", spim3_rx_buffer[3]);
        NRF_LOG_HEXDUMP_DEBUG(spim3_rx_buffer + BOARD2BOARD_MSG_HEADER_LENGTH, len - BOARD2BOARD_MSG_HEADER_LENGTH);
        board2board_set_payload_error(spim3_tx_buffer);
    }
}

static void spim_evt_handler(nrfx_spim_evt_t const *p_event, void __unused *p_context) {
    static size_t remaining_bytes = 0;

    switch (p_event->type) {
        default:
            DEBUG_BREAKPOINT;
            break;
        case NRFX_SPIM_EVENT_DONE: {
            #define PAD(N) (p_event->xfer_desc.p_rx_buffer[N] == BOARD2BOARD_PAD_BYTE)
            #define BUSY(N) (p_event->xfer_desc.p_rx_buffer[N] == BOARD2BOARD_DEVICE_BUSY_BYTE)
            if (!((p_event->xfer_desc.tx_length == 1 && p_event->xfer_desc.p_tx_buffer[0] == 0x41 &&
                   (BUSY(0) || PAD(0))) ||
                  (p_event->xfer_desc.tx_length == 4 && p_event->xfer_desc.p_tx_buffer[0] == 0x41 &&
                   p_event->xfer_desc.p_tx_buffer[1] == 0 && p_event->xfer_desc.p_tx_buffer[2] == 0x4E &&
                   ((BUSY(0) && BUSY(1) && BUSY(2) && BUSY(3)) ||
                    (PAD(0) && PAD(1) && PAD(2) && PAD(3)))))) {
                #undef BUSY
                #undef PAD
//                NRF_LOG_DEBUG("Received %d bytes; sent %d bytes", p_event->xfer_desc.rx_length,
//                              p_event->xfer_desc.tx_length);
//                if (p_event->xfer_desc.rx_length)
//                    NRF_LOG_HEXDUMP_DEBUG((void *) &spim3_rx_buffer[p_event->xfer_desc.p_rx_buffer - spim3_rx_buffer],
//                                          p_event->xfer_desc.rx_length);
//                if (p_event->xfer_desc.tx_length)
//                    NRF_LOG_HEXDUMP_DEBUG((void *) &spim3_tx_buffer[p_event->xfer_desc.p_tx_buffer - spim3_tx_buffer],
//                                          p_event->xfer_desc.tx_length);
            }
            if (p_event->xfer_desc.tx_length == 1) {
                if (spim3_rx_buffer[0] == BOARD2BOARD_DEVICE_BUSY_BYTE) {
                    /* Device was busy, re-transfer */
                    APP_ERROR_CHECK(SPIM_XFER);
                    break;
                }
                // Statuses transmitted.
                if (spim3_tx_buffer[0] & BOARD2BOARD_MSG_HEADER_CRC_ERROR) {
                    // Previous message indicated Header error.
                    // Impossible to know state of Device, so clear error flags & repeat transfer
                    NRF_LOG_DEBUG("Reported header CRC error, reset transfer.");
                    board2board_clear_error_flags(spim3_tx_buffer);
                    // Reset size
                    xfer_desc.tx_length = xfer_desc.rx_length = board2board_get_msg_length(spim3_tx_buffer);
                } else {
                    // Received status response to completed message.
                    xfer_desc.rx_length = xfer_desc.tx_length = 0;
                    if (spim3_tx_buffer[0] & BOARD2BOARD_MSG_PAYLOAD_CRC_ERROR) {
                        // Received corrupted payload, but valid header: re-receive entire message in single transfer.
                        NRF_LOG_DEBUG("Payload CRC error, valid header; re-receive in single transfer.");
                        xfer_desc.rx_length = board2board_get_msg_length(spim3_rx_buffer);
                        board2board_clear_error_flags(spim3_tx_buffer);
                    }
                    if (spim3_rx_buffer[0] & BOARD2BOARD_ERROR_FLAGS) {
                        // Device reported corrupted message, resend.
                        NRF_LOG_DEBUG("Device reported payload CRC error, resend whole message.");
                        xfer_desc.tx_length = board2board_get_msg_length(spim3_tx_buffer);
                    }
                    if (xfer_desc.tx_length || xfer_desc.rx_length) {
                        // Retransmission needed
                        if (xfer_desc.tx_length) {
                            NRF_LOG_DEBUG("Retransmit %d bytes:", xfer_desc.tx_length);
                            NRF_LOG_HEXDUMP_DEBUG(xfer_desc.p_tx_buffer, xfer_desc.tx_length);
                        }
                        if (xfer_desc.rx_length) NRF_LOG_DEBUG("Re-receive %d bytes.", xfer_desc.rx_length);
                    } else {
                        // Message received
                        spim_busy = false;
                        parse_device_message();
                        break;
                    }
                }
            } else if (remaining_bytes) {
                if (spim3_rx_buffer[board2board_get_msg_length(spim3_rx_buffer)] == BOARD2BOARD_DEVICE_BUSY_BYTE) {
                    /* Device was busy, re-transfer */
                    APP_ERROR_CHECK(SPIM_XFER);
                    break;
                }
                // Header + complete data was received; Header was previously verified.
//                NRF_LOG_DEBUG("Received %d remaining bytes.", xfer_desc.rx_length);
                remaining_bytes = 0;
                xfer_desc.p_rx_buffer = spim3_rx_buffer;  // Reset buffer pointer
                validate_payload();
                /* Report status */
                xfer_desc.tx_length = xfer_desc.rx_length = 1;
            } else {
                // Message start
                if (!status_is_real()) break;
                if (board2board_validate_header(spim3_rx_buffer)) {
                    // Verified message Header
                    size_t msg_len = board2board_get_msg_length(spim3_rx_buffer);
                    if (msg_len > p_event->xfer_desc.rx_length) {
                        // Message incomplete, receive remaining bytes
//                        NRF_LOG_DEBUG("Partial message received:")
//                        NRF_LOG_HEXDUMP_DEBUG(p_event->xfer_desc.p_rx_buffer, p_event->xfer_desc.rx_length);
                        xfer_desc.p_rx_buffer += p_event->xfer_desc.rx_length;
                        // Extra byte should be padding; easy to check for busy device.
                        remaining_bytes = msg_len - p_event->xfer_desc.rx_length + 1;
                        xfer_desc.rx_length = xfer_desc.tx_length = remaining_bytes;
//                        NRF_LOG_DEBUG("Awaiting %d more bytes.", remaining_bytes);
                    } else {
                        // Complete Device message received.
                        validate_payload();
                        /* Report status */
                        xfer_desc.tx_length = xfer_desc.rx_length = 1;
                    }
                } else {
                    // Corrupt message Header
                    NRF_LOG_DEBUG("Bad CRC (%d) for header:", spim3_rx_buffer[2]);
                    NRF_LOG_HEXDUMP_DEBUG(spim3_rx_buffer, 2);
                    board2board_set_header_error(spim3_tx_buffer);
                    xfer_desc.tx_length = 1;
                    xfer_desc.rx_length = 0;
                }
            }
            APP_ERROR_CHECK(SPIM_XFER);
        }
            break;
    }
}

static void spim_init(void) {
    nrfx_spim_config_t spim_config = NRFX_SPIM_DEFAULT_CONFIG;

    spim_config.mode = NRF_SPIM_MODE_1;
    spim_config.frequency = NRF_SPIM_FREQ_8M;
    spim_config.orc = BOARD2BOARD_PAD_BYTE;
    spim_config.irq_priority = EXTERNAL_COMMS_IRQ_PRIORITY;
    spim_config.sck_pin = B2B_SCL_PIN;
    spim_config.miso_pin = B2B_SDO_PIN;
    spim_config.mosi_pin = B2B_SDI_PIN;
    spim_config.ss_pin = B2B_CSN_PIN;
    spim_config.bit_order = NRF_SPIM_BIT_ORDER_LSB_FIRST;
    spim_config.use_hw_ss = true;   // The reason this is on SPI3
    spim_config.ss_duration = 64;   // May need to be doubled

    nrfx_err_t err = nrfx_spim_init(&spim, &spim_config, spim_evt_handler, NULL);
    APP_ERROR_CHECK(err);
    spim_busy = false;
    board2board_host_send(NULL, 0);
}

void board2board_host_init(void) {
    crc_init();
    spim_init();
    memset(spim3_rx_buffer, 0, sizeof(spim3_rx_buffer));
    memset(spim3_tx_buffer, 0, sizeof(spim3_tx_buffer));
    initialized = true;
}

nrfx_err_t board2board_host_send(uint8_t const *data, size_t length) {
    if (!initialized) return NRFX_ERROR_INVALID_STATE;
    if (length > BOARD2BOARD_PAYLOAD_MAX_LENGTH) return NRFX_ERROR_INVALID_LENGTH;
    if (spim_busy) return NRFX_ERROR_BUSY;
    if (length && data) memcpy(board2board_get_payload(spim3_tx_buffer), data, length);
    board2board_set_header(spim3_tx_buffer, length);
    // Zero byte immediately following data because it may get transmitted.
    spim3_tx_buffer[length + BOARD2BOARD_MSG_HEADER_LENGTH] = 0;
    xfer_desc.tx_length = xfer_desc.rx_length = length + BOARD2BOARD_MSG_HEADER_LENGTH;
    spim_busy = true;
    nrfx_err_t err = SPIM_XFER;
    if (err != NRFX_SUCCESS) {
        spim_busy = false;
    } else if(length) {
//        NRF_LOG_DEBUG("Transferring %d bytes.", xfer_desc.tx_length);
    }
    return err;
}

bool board2board_host_ready(void) {
    return spim_busy;
}
