//
// Created by Benjamin Riggs on 9/10/21.
//

#ifndef TRIACS_H
#define TRIACS_H

#include <stdint.h>
#include <nrfx.h>

void triacs_init(void);

nrfx_err_t set_triac(uint8_t low_channel, uint8_t high_channel, bool enable);

#endif //TRIACS_H
