//
// Created by Benjamin Riggs on 6/28/21.
//

#include "audio_adc.h"
#include "nrfx_i2s.h"
#include "usb_serial.h"
#include "pin_config.h"

#include "prv_utils.h"
#include "nrfx_log.h"

#define DATA_BUFFER_WORD_COUNT (0x1000)
static uint8_t volatile m_data_buffers[2][DATA_BUFFER_WORD_COUNT * 4] = {0};
static nrfx_i2s_buffers_t m_i2s_buffers = { .p_rx_buffer = (uint32_t *) (m_data_buffers[0]), .p_tx_buffer = NULL};

static enum {
    AUDIO_ADC_UNINITIALIZED,
    AUDIO_ADC_STOPPED,
    AUDIO_ADC_STARTING,
    AUDIO_ADC_RUNNING,
    AUDIO_ADC_STOPPING,
} m_state = AUDIO_ADC_UNINITIALIZED;

static void i2s_event_handler(nrfx_i2s_buffers_t const *p_released, uint32_t status) {
    static uint8_t next_buffer = 1;

    if (status & NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED) {
        switch (m_state) {
            case AUDIO_ADC_UNINITIALIZED:
            case AUDIO_ADC_STOPPED:
                DEBUG_BREAKPOINT;
                break;
            case AUDIO_ADC_STARTING:
                m_state = AUDIO_ADC_RUNNING;
                break;
            case AUDIO_ADC_RUNNING:
                if (p_released->p_rx_buffer != NULL) {
                    usb_serial_send((uint8_t *) p_released->p_rx_buffer, DATA_BUFFER_WORD_COUNT * 4, NULL);
                    next_buffer = (uint8_t) ((uint8_t *) p_released->p_rx_buffer == m_data_buffers[0]);
                    m_i2s_buffers.p_rx_buffer = (uint32_t *) m_data_buffers[next_buffer];
                    int ret = nrfx_i2s_next_buffers_set(&m_i2s_buffers);
                    if (ret != NRFX_SUCCESS) {
                        NRF_LOG_ERROR("I2S error setting next buffer: %u", ret);
                    }
                } else {
                    NRF_LOG_ERROR("I2S data corruption occurred");
                }
                break;
            case AUDIO_ADC_STOPPING:
                m_state = AUDIO_ADC_STOPPED;
                break;
        }
    } else {
        NRF_LOG_DEBUG("I2S status: %u", status);
    }
}

void audio_adc_init(void) {

    nrfx_i2s_config_t i2c_cfg = {
        .sck_pin = AUDIO_BCK_PIN,
        .lrck_pin = AUDIO_LRCK_PIN,
        .mck_pin = NRFX_I2S_PIN_NOT_USED,
        .sdout_pin = NRFX_I2S_PIN_NOT_USED,
        .sdin_pin = AUDIO_DOUT_PIN,
        .mode = NRF_I2S_MODE_MASTER,
        .format = NRF_I2S_FORMAT_ALIGNED,
        .alignment = NRF_I2S_ALIGN_LEFT,
        .sample_width = NRF_I2S_SWIDTH_24BIT,
        .channels = NRF_I2S_CHANNELS_STEREO,
        .mck_setup = NRF_I2S_MCK_32MDIV16,
        .ratio = NRF_I2S_RATIO_48X,
    };

    APP_ERROR_CHECK(nrfx_i2s_init(&i2c_cfg, i2s_event_handler));

    m_state = AUDIO_ADC_STOPPED;
}

void audio_adc_start(void) {
    switch (m_state) {
        case AUDIO_ADC_UNINITIALIZED:
            NRFX_LOG_ERROR("Can't start I2S before initialization.")
            break;
        case AUDIO_ADC_STOPPING:
        case AUDIO_ADC_STOPPED:
            m_state = AUDIO_ADC_STARTING;
            APP_ERROR_CHECK(nrfx_i2s_start(&m_i2s_buffers, DATA_BUFFER_WORD_COUNT, 0));
            break;
        case AUDIO_ADC_STARTING:
        case AUDIO_ADC_RUNNING:
            // Nothing to do
            break;
    }
}

void audio_adc_stop(void) {
    switch (m_state) {
        case AUDIO_ADC_UNINITIALIZED:
        case AUDIO_ADC_STOPPING:
        case AUDIO_ADC_STOPPED:
            // Nothing to do
            break;
        case AUDIO_ADC_STARTING:
        case AUDIO_ADC_RUNNING:
            m_state = AUDIO_ADC_STOPPING;
            nrfx_i2s_stop();
            break;
    }
}
