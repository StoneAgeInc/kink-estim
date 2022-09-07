//
// Created by Benjamin Riggs on 6/1/21.
//

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

extern struct bat_mon_stat_t {
    bool self_test_complete : 1;
    bool self_test_passed : 1;
    uint8_t self_test_xfer_step : 2;
} volatile battery_monitor_status;

void battery_monitor_init(void);

#endif //BATTERY_MONITOR_H
