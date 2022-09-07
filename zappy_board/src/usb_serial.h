//
// Created by Benjamin Riggs on 3/6/20.
//

#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <stdint.h>
#include <stddef.h>

void usb_serial_send(uint8_t *p_data, size_t length, void *p_ctx);

void usb_init(void);

void process_usb_events(void);

#endif //USB_SERIAL_H
