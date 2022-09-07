//
// Created by Benjamin Riggs on 3/2/20.
//

#include <string.h>

#include "bq25895.h"

#include "battery_charger.h"
#include "pin_config.h"
#include "prv_power_manager.h"
#include "timers.h"

#include "nrfx_twim.h"
#include "nrf_atfifo.h"

#include "app_scheduler.h"

#define I2C_QUEUE_SIZE 8

static nrfx_twim_t twim = NRFX_TWIM_INSTANCE(I2C_TWIM_INSTANCE);

// Must to be in RAM for EasyDMA
// Must be contiguous for writes, but 'separate' buffers for reads.
static uint8_t volatile twim_reg_addr[1 + BQ25895_STATUS_REG_COUNT] = {0xFF};
static uint8_t volatile *const twim_data = &twim_reg_addr[1];

typedef struct {
    uint32_t            updated;
    bq25895_REG0C_t     prior_fault;
    bq25895_REG0C_t     current_fault;
} battery_charger_faults_t;

static battery_charger_faults_t battery_charger_faults = {0};

typedef struct {
    uint32_t updated;
    uint8_t value;
    bq25895_register_t address;
} charger_reg_t;

typedef struct {
    bool write;
    uint8_t reg_addr;
    uint8_t *p_data;        // For write xfer, where to read data from, for read xfer, where to write data to
    uint8_t length;
    uint32_t *p_updated;    // Pointer to a location that contains the ms timestamp of last update
} i2c_command_t;

static i2c_command_t volatile i2c_command;

NRF_ATFIFO_DEF(i2c_command_queue, i2c_command_t, I2C_QUEUE_SIZE);

static nrfx_err_t i2c_xfer(bool write, bq25895_register_t reg_addr, uint8_t *p_data, uint8_t length, uint32_t *p_completed) {
    if (length > BQ25895_STATUS_REG_COUNT) return NRFX_ERROR_INVALID_LENGTH;
    if (nrfx_twim_is_busy(&twim)) {
        nrf_atfifo_item_put_t context;
        i2c_command_t *p_command = nrf_atfifo_item_alloc(i2c_command_queue, &context);
        if (p_command == NULL) return NRFX_ERROR_NO_MEM;
        p_command->write = write;
        p_command->reg_addr = reg_addr;
        p_command->p_data = p_data;
        p_command->length = length;
        p_command->p_updated = p_completed;
        nrf_atfifo_item_put(i2c_command_queue, &context);
        return NRFX_SUCCESS;
    } else {
        twim_reg_addr[0] = reg_addr;
        nrfx_twim_xfer_desc_t xfer_desc = NRFX_TWIM_XFER_DESC_TXRX(
            BQ25895_I2C_ADDRESS, (uint8_t *) twim_reg_addr, 1, (uint8_t *) twim_data, length);
        if (write) {
            xfer_desc.type = NRFX_TWIM_XFER_TX;
            xfer_desc.primary_length = length + 1;
            xfer_desc.secondary_length = 0;
            xfer_desc.p_secondary_buf = NULL;
            memcpy((void *) twim_data, p_data, length);
        }
        nrfx_err_t err = nrfx_twim_xfer(&twim, &xfer_desc, 0);
        if (err == NRFX_SUCCESS) {
            i2c_command.write = write;
            i2c_command.reg_addr = reg_addr;
            i2c_command.p_data = p_data;
            i2c_command.length = length;
            i2c_command.p_updated = write ? NULL : p_completed;
        }
        return err;
    }
}

void battery_charger_evt_handler(nrfx_twim_evt_t const *p_event, void __unused *p_context) {
    switch (p_event->type) {
        case NRFX_TWIM_EVT_ADDRESS_NACK:
            break;
        case NRFX_TWIM_EVT_DATA_NACK:
            break;
        case NRFX_TWIM_EVT_OVERRUN:
            break;
        case NRFX_TWIM_EVT_BUS_ERROR:
            break;
        case NRFX_TWIM_EVT_DONE:
            // If it was a read command, copy the data to the provided location
            if (!i2c_command.write && i2c_command.p_data != NULL) {
                memcpy(i2c_command.p_data, p_event->xfer_desc.p_secondary_buf, p_event->xfer_desc.secondary_length);
            }
            if (i2c_command.p_updated) *i2c_command.p_updated = ms_timestamp();
            break;
        default:
            break;
    }
    // Call next event in queue, if any.
    nrf_atfifo_item_get_t context;
    i2c_command_t *c = nrf_atfifo_item_get(i2c_command_queue, &context);
    if (c == NULL) return; // Queue is empty
    APP_ERROR_CHECK(i2c_xfer(c->write, c->reg_addr, c->p_data, c->length, c->p_updated));
    nrf_atfifo_item_free(i2c_command_queue, &context);
}

nrfx_err_t update_charger_faults(void) {
    nrfx_err_t err;
    err = i2c_xfer(false, BQ25895_REG0C, (uint8_t *) &battery_charger_faults.prior_fault, 1,
                   &battery_charger_faults.updated);
    if (err != NRFX_SUCCESS) return err;
    err = i2c_xfer(false, BQ25895_REG0C, (uint8_t *) &battery_charger_faults.current_fault, 1,
                   &battery_charger_faults.updated);
    return err;
}

static charger_reg_t reg03 = {.address = BQ25895_REG03};
static charger_reg_t reg09 = {.address = BQ25895_REG09};

static nrfx_err_t update_reg(charger_reg_t *reg) {
    return i2c_xfer(false, reg->address, &reg->value, 1, &reg->updated);
}

bool battery_charger_shutdown(void) {
    if (!reg09.updated) {
        NRF_LOG_DEBUG("Battery charger updating shutdown register.")
        update_reg(&reg09);
        return false;
    }
    reg09.value |= BQ25895_BATFET_DIS_MASK;
    NRF_LOG_DEBUG("Battery charger sending shutdown command.")
    if (i2c_xfer(true, reg09.address, &reg09.value, 1, NULL)) return false;
    return true;
}

static nrfx_err_t set_charger_watchdog(void) {
    if (!reg03.updated) return update_reg(&reg03);
    reg03.value |= BQ25895_WD_RST_MASK;
    return i2c_xfer(true, reg03.address, &reg03.value, 1, NULL);
}

void battery_charger_init(void) {
    NRF_ATFIFO_INIT(i2c_command_queue);
//    battery_charger_update();
}

void battery_charger_update(void) {
    if (battery_charger_faults.current_fault.WATCHDOG_FAULT) {
        APP_ERROR_CHECK(set_charger_watchdog());
    }
    APP_ERROR_CHECK(update_charger_faults());
    APP_ERROR_CHECK(update_reg(&reg09));
    APP_ERROR_CHECK(update_reg(&reg03));
}
