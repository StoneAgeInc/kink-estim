//
// Created by Benjamin Riggs on 4/13/20.
//

#include "timers.h"
#include "buttons.h"
#include "pattern_control.h"
#include "battery_charger.h"
#include "board2board_host.h"
#include "display.h"
#include "prv_utils.h"
#include "prv_timers.h"

static void update_timer_handler(void __unused *p_context) {
    static uint32_t volatile update_counter = 0;
    update_pulses();
    if (update_counter % (UPDATE_TIMER_FREQ_Hz / BUTTON_SCAN_UPDATE_FREQ_Hz) == 0) {
        button_scan();
    }
    if (update_counter % (UPDATE_TIMER_FREQ_Hz / BATTERY_CHARGER_UPDATE_FREQ_Hz) == 0) {
        app_sched_event_put(NULL, 0, SCHED_FN(battery_charger_update));
    }
    if (update_counter % (UPDATE_TIMER_FREQ_Hz / DISPLAY_STATE_UPDATE_FREQ_Hz) == 0) {
        update_pattern_progress();
        update_intensities();
    }
    // Queue up 'empty' message to pull anything from sibling board.
    // If another transaction is occurring, it will silently fail.
    if (update_counter % (UPDATE_TIMER_FREQ_Hz / BOARD_2_BOARD_POLL_FREQ_Hz) == 0) {
        app_sched_event_put(NULL, 0, SCHED_FN(board2board_host_send));
    }

    update_counter++;
}

uint32_t inline ms_timestamp(void) { return prv_timestamp(); }

void timers_init(void) {
    prv_timers_init((1000/UPDATE_TIMER_FREQ_Hz), update_timer_handler);
}
