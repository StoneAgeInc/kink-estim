//
// Created by Benjamin Riggs on 4/13/20.
//

#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>

#define UPDATE_TIMER_FREQ_Hz 250
#define BUTTON_SCAN_UPDATE_FREQ_Hz 25
#define DISPLAY_STATE_UPDATE_FREQ_Hz 30
#define BATTERY_CHARGER_UPDATE_FREQ_Hz 1
#define BOARD_2_BOARD_POLL_FREQ_Hz 50

uint32_t ms_timestamp(void);

void timers_init(void);

#endif //TIMERS_H
