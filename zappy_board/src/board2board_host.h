//
// Created by Benjamin Riggs on 9/29/20.
//

#ifndef BOARD2BOARD_HOST_H
#define BOARD2BOARD_HOST_H

#include "nrfx.h"

void board2board_host_init(void);

/**
 * @brief Send a message to companion board.
 *
 * @param[in] data
 * @param[in] length
 *
 * @retval NRFX_SUCCESS                 Message successfully sent.
 * @retval NRFX_ERROR_INVALID_LENGTH    Message too long.
 * @retval NRFX_ERROR_BUSY              Device is busy.
 */
nrfx_err_t board2board_host_send(uint8_t const *data, size_t length);

bool board2board_host_ready(void);

#endif //BOARD2BOARD_HOST_H
