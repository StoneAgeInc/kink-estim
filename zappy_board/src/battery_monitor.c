//
// Created by Benjamin Riggs on 6/1/21.
//

#include "battery_monitor.h"
#include "battery_charger.h"
#include "app_config.h"
#include "pin_config.h"
#include "prv_utils.h"
#include "bq27621.h"
#include "bq25895.h"
#include "timers.h"

#include "nrfx_twim.h"

struct bat_mon_stat_t volatile battery_monitor_status = {0};
static uint32_t volatile last_i2c_xfer_timestamp = 0;

static nrfx_twim_t twim = NRFX_TWIM_INSTANCE(I2C_TWIM_INSTANCE);

static uint8_t volatile twim_buffers[2][3] = {0};
static nrfx_twim_xfer_desc_t xfer_desc = {
    .type = NRFX_TWIM_XFER_TX,
    .address = BQ27621_I2C_ADDRESS,
    .primary_length = 0,
    .secondary_length = 0,
    .p_primary_buf = (uint8_t *) &(twim_buffers[0]),
    .p_secondary_buf = (uint8_t *) &(twim_buffers[1])
};

static void scheduled_xfer(void) {
    uint32_t now = ms_timestamp();
    if (now - BQ27621_I2C_WAIT_TIME_ms > last_i2c_xfer_timestamp &&
        !nrfx_twim_is_busy(&twim)) {
        APP_ERROR_CHECK(nrfx_twim_xfer(&twim, &xfer_desc, 0));
    } else {
        app_sched_event_put(NULL, 0, SCHED_FN(scheduled_xfer));
    }
}

extern void battery_charger_evt_handler(nrfx_twim_evt_t const *p_event, void __unused *p_context);

static void battery_monitor_evt_handler(nrfx_twim_evt_t const *p_event, void __unused *p_context) {
    switch (p_event->type) {
        default:
        case NRFX_TWIM_EVT_ADDRESS_NACK:
          break;
        case NRFX_TWIM_EVT_DATA_NACK:
        case NRFX_TWIM_EVT_OVERRUN:
        case NRFX_TWIM_EVT_BUS_ERROR:
            DEBUG_BREAKPOINT;
            break;
        case NRFX_TWIM_EVT_DONE:
            switch (battery_monitor_status.self_test_xfer_step++) {
                case 0:
                    // Subcommand written
                    xfer_desc.type = NRFX_TWIM_XFER_TXRX;
                    xfer_desc.primary_length = 1;
                    xfer_desc.secondary_length = 2;
                    twim_buffers[0][0] = 0x00;  // Device type bytes will be at addresses 0x00 & 0x01 (big-endian)
                    last_i2c_xfer_timestamp = ms_timestamp();
                    app_sched_event_put(NULL, 0, SCHED_FN(scheduled_xfer));
                    break;
                case 1:
                    // Device type bytes read (maybe)
                    battery_monitor_status.self_test_complete = true;
                    battery_monitor_status.self_test_passed =
                        ((twim_buffers[1][0] << 8) + (twim_buffers[1][1]) == BQ27621_DEVICE_TYPE_VALUE);
                    break;
            }
            break;
    }
}

static void i2c_irq_evt_handler(nrfx_twim_evt_t const *p_event, void *p_context) {
    switch (p_event->xfer_desc.address) {
        case BQ27621_I2C_ADDRESS:
            battery_monitor_evt_handler(p_event, p_context);
            break;
        case BQ25895_I2C_ADDRESS:
            battery_charger_evt_handler(p_event, p_context);
            break;
        default:
            DEBUG_BREAKPOINT;
            break;
    }
}

static void twim_init(void) {
    nrfx_twim_config_t twim_config = {
        .scl = I2C_SCL_PIN,
        .sda = I2C_SDA_PIN,
        .frequency = NRF_TWIM_FREQ_100K,
        .interrupt_priority = ON_BOARD_COMMS_IRQ_PRIORITY,
        .hold_bus_uninit = true
    };
    APP_ERROR_CHECK(nrfx_twim_init(&twim, &twim_config, i2c_irq_evt_handler, NULL));
    nrfx_twim_enable(&twim);
}

void battery_monitor_init(void) {
    twim_init();
    xfer_desc.type = NRFX_TWIM_XFER_TX;
    xfer_desc.primary_length = 3;
    twim_buffers[0][0] = BQ27621_COMMAND_CODE0_CNTL;
    twim_buffers[0][1] = BQ27621_CNTL_FUNCTION_DEVICE_TYPE >> 8;
    twim_buffers[0][2] = BQ27621_CNTL_FUNCTION_DEVICE_TYPE & 0xFF;
    APP_ERROR_CHECK(nrfx_twim_xfer(&twim, &xfer_desc, 0));
    battery_charger_init();
}
