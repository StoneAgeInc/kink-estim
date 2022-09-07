//
// Created by Benjamin Riggs on 10/19/21.
//

#include "display.h"
#include "serial_protocol.h"
#include "pattern_control.h"
#include "pulse_control.h"
#include "board2board_host.h"
#include "prv_utils.h"

#define QUEUED_MESSAGES \
X(Q_ADJUSTS, OP_SET_PATTERN_ADJUST, pattern_adjusts) \
X(Q_PROGRESS, OP_SET_PATTERN_PROGRESS, pattern_progress) \
X(Q_INTENSITIES, OP_DISPLAY_INTENSITIES, intensities)

typedef enum {
    #define X(q, op, d) q,
    QUEUED_MESSAGES
    #undef X
    Q_COUNT
} queued_t;

typedef struct {
    queued_t type;
    bool queued;
    bool init;
    uint16_t *p_data;
    union {
        struct {
            uint8_t header[sizeof(zappy_msg_t)];
            uint16_t payload[DEVICE_CHANNEL_COUNT];
        };
        zappy_msg_t msg;
    };
} queue_state_t;

static queue_state_t queue_state[Q_COUNT] = {0};

zappy_intensities_t intensities = {0};

static void update(queue_state_t **p_state) {
    queue_state_t *state = *p_state;
    ASSERT(state->init);
    state->msg.channels = channels_active;
    for (int i = 0; i < DEVICE_CHANNEL_COUNT; i++) {
        state->payload[i] = state->p_data[i];
    }

    nrfx_err_t err = board2board_host_send(state->header, sizeof(state->header) + sizeof(state->payload));

    if (err == NRFX_SUCCESS || err == NRFX_ERROR_INVALID_STATE) {
        state->queued = false;
        return;
    }
    if (err == NRFX_ERROR_BUSY) {
        state->queued = true;
        APP_ERROR_CHECK(app_sched_event_put(p_state, sizeof(void *), SCHED_FN(update)));
        return;
    }
    DEBUG_BREAKPOINT;
}

static void do_update(queue_state_t *qs) {
    if (qs->queued) return;
    update(&qs);
}

#if 0
static void do_update_pattern_progress(void) {
    static zappy_msg_t msg = {
        .opcode = OP_SET_PATTERN_PROGRESS,
        .channels = (1 << (DEVICE_CHANNEL_COUNT - 1)) - 1,
        .payload = {[0 ... _CHANNEL_ARR_MAX] = 0}
    };
    msg.channels = channels_active;
    for (int i = 0; i < DEVICE_CHANNEL_COUNT; i++) {
        msg.payload[i] = pattern_progress[i];
    }
    nrfx_err_t err = board2board_host_send((uint8_t *) &msg,
                                           sizeof(zappy_msg_t) + sizeof(zappy_pattern_progress_t));
    if (err == NRFX_SUCCESS) {
        queued_progress = false;
        return;
    }
    if (err == NRFX_ERROR_BUSY) {
        queued_progress = true;
        app_sched_event_put(NULL, 0, SCHED_FN(do_update_pattern_progress));
        return;
    }
    DEBUG_BREAKPOINT;
}

static void do_update_intensities(void) {
    static zappy_msg_t msg = {
        .opcode = OP_DISPLAY_INTENSITIES,
        .channels = (1 << (DEVICE_CHANNEL_COUNT - 1)) - 1,
        .payload = {[0 ... _CHANNEL_ARR_MAX] = 0}
    };
    msg.channels = channels_active;
    for (int i = 0; i < DEVICE_CHANNEL_COUNT; i++) {
        msg.payload[i] = intensities[i];
    }
    nrfx_err_t err = board2board_host_send((uint8_t *) &msg,
                                           sizeof(zappy_msg_t) + sizeof(zappy_intensities_t));
    if (err == NRFX_SUCCESS) {
        queued_intensities = false;
        return;
    }
    if (err == NRFX_ERROR_BUSY) {
        queued_intensities = true;
        app_sched_event_put(NULL, 0, SCHED_FN(do_update_intensities));
        return;
    }
    DEBUG_BREAKPOINT;
}
#endif

void update_pattern_progress(void) {
    do_update(&queue_state[Q_PROGRESS]);
}

void update_intensities(void) {
    do_update(&queue_state[Q_INTENSITIES]);
}

void update_adjusts(void) {
    do_update(&queue_state[Q_ADJUSTS]);
}

void display_init(void) {
    #define X(q, op, d) \
    queue_state[q].init = true; \
    queue_state[q].type = q; \
    queue_state[q].msg.opcode = op; \
    queue_state[q].p_data = d; \
    queue_state[q].msg.channels = (1 << (DEVICE_CHANNEL_COUNT - 1)) - 1;
    QUEUED_MESSAGES
    #undef X
}

uint16_t curve_intensity(uint16_t input) {
    // 4x / (1 + x)^2 * 0xFF
    uint32_t n = input * 0xFF;
    /*
     * 0xFFFF + input / 2 <= UINT16_MAX.
     * UINT16_MAX ** 2 < UINT32_MAX.
     * (1 / (1 / 2) ** 2) = 4.
     */
    uint32_t d = ((uint32_t) (((0xFFFF + input) / 2) * ((0xFFFF + input) / 2))) / 0xFFFF;
    return n / d;
}
