//
// Created by Benjamin Riggs on 3/6/20.
//

#include "usb_serial.h"
#include "app_config.h"
#include "patterns.h"
#include "serial_protocol.h"

#include "prv_serial_parser.h"

#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"

#include "nrf_drv_usbd.h"

static uint8_t volatile input_buffer[MSG_MAX_SIZE] = {0};
static uint8_t volatile error_buffer[MSG_HEADER_SIZE] = {0};
static zappy_msg_t volatile *const response = (zappy_msg_t *) error_buffer;
static bool volatile m_connected = false;

static void cdc_acm_user_evt_handler(app_usbd_class_inst_t const *p_i, app_usbd_cdc_acm_user_event_t event);

APP_USBD_CDC_ACM_GLOBAL_DEF(app_cdc_acm,
                            cdc_acm_user_evt_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_AT_V250
);

static size_t volatile received_length = MSG_HEADER_SIZE;

static bool receive(uint16_t length, uint16_t offset) {
    received_length = offset + length;
    nrfx_err_t err = app_usbd_cdc_acm_read(&app_cdc_acm, (void *) &input_buffer + offset, length);
    if (err == NRF_SUCCESS) {
        // Data is available in buffer now
        return true;
    } else if (err == NRF_ERROR_IO_PENDING) {
        // Waiting on more data.
        return false;
    } else {
        APP_ERROR_CHECK(err);
        return false;
    }
}

void usb_serial_send(uint8_t *p_data, size_t length, void __unused *p_ctx) {
    if (!m_connected) return;
    APP_ERROR_CHECK(app_usbd_cdc_acm_write(&app_cdc_acm, p_data, length));
}

static void cdc_acm_user_evt_handler(app_usbd_class_inst_t __unused const *p_i, app_usbd_cdc_acm_user_event_t event) {
    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
            m_connected = true;
            receive(MSG_HEADER_SIZE, 0);
            break;
        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            m_connected = false;
            break;
        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
//            receive(MSG_HEADER_SIZE, 0);
            break;
        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            bool data_to_parse = true;
            while (data_to_parse) {
                size_t missing_length = serial_parse((uint8_t *) input_buffer, received_length, usb_serial_send, NULL);
                if (missing_length == SIZE_MAX) {
                    // Parse error, reset connection
                    response->opcode = ((zappy_msg_t *) input_buffer)->opcode;
                    response->retcode = OP_ERROR_PARSE_ERROR;
                    app_usbd_cdc_acm_write(&app_cdc_acm, (void *) error_buffer, MSG_HEADER_SIZE);
                    // Flush any data in the buffer, throwing it away
                    while (receive(MSG_HEADER_SIZE, 0)) {
                        // TODO: yield occasionally to not hang on usb spam
                        // Loop will exit when peripheral is waiting for new message header
                    }
                    data_to_parse = false;
                } else if (missing_length) {
                    // Incomplete message
                    data_to_parse = receive(missing_length, received_length);
                } else {
                    // Previous message completely parsed
                    data_to_parse = receive(MSG_HEADER_SIZE, 0);
                }
            }
        }
            break;
        default:
            break;
    }
}

static void usbd_user_ev_handler(app_usbd_event_type_t event) {
    switch (event) {
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            if (!nrf_drv_usbd_is_enabled()) {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            app_usbd_start();
            break;
        default:
            break;
    }
}

/// Public functions

void usb_init(void) {
    /**
     * IMPORTANT HARDWARE NOTE:
     *
     * When the signal board is not attached to either the display board or a battery, some sort of (likely timing)
     * anomaly occurs between the USB MUX, battery controller, and nRF52840 which causes the device to fail to be
     * detected by OSes via USB.
     */
    static app_usbd_config_t const usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    app_usbd_serial_num_generate();

    // app_usbd_init requires nrf_drv_clock_init
    APP_ERROR_CHECK(app_usbd_init(&usbd_config));

    APP_ERROR_CHECK(app_usbd_class_append(app_usbd_cdc_acm_class_inst_get(&app_cdc_acm)));

    #if APP_USBD_CONFIG_POWER_EVENTS_PROCESS
    APP_ERROR_CHECK(app_usbd_power_events_enable());
    #else
    app_usbd_enable();
    app_usbd_start();
    #endif
}

void inline process_usb_events(void) {
    while (app_usbd_event_queue_process()) { /* Nothing to do */ }
}
