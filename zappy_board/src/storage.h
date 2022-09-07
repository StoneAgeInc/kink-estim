//
// Created by Benjamin Riggs on 3/10/20.
//

#ifndef STORAGE_H
#define STORAGE_H

#include "nrfx.h"

#include "patterns.h"

/**@brief Metadata about patterns stored in flash */
extern uint16_t volatile pattern_storage_count;

void storage_init(void);

bool is_busy(void);

/**@brief   Function to retrieve the next pattern header from storage.
 *
 * This function iterates through the headers of patterns found or added to flash storage.
 *
 * @param[in, out]  *p_nth              A pointer to the index position returned by the previous call,
 *                                      or a pointer to 0 to iterate from the beginning.
 * @param[in]       reverse             A flag to iterate in reverse when true.
 *
 * @retval  true    A pattern was found, @p *p_nth was updated accordingly.
 * @retval  false   No pattern was found, @p *p_nth was set to 0 (enabling easy wrap-around functionality).
 *
 * @note    Wait until main loop to query for first pattern to ensure module is initialized.
 */
bool next_pattern_index(uint16_t *p_nth, bool reverse);

/**@brief   Function to retrieve the nth pattern (1-indexed) on the device.
 *
 * @param[out]      **p_pattern         A pointer that will be set to the flash memory containing the pattern.
 *                                      Because this is a pointer to flash, the data is read-only.
 * @param[in]       nth                 The count of pattern to retrieve, starting at 1.
 *
 * @retval  NRFX_SUCCESS                A pattern was found.
 * @retval  NRFX_ERROR_INVALID_ADDR     nth is out of range.
 */
nrfx_err_t get_nth_pattern(zappy_pattern_t const **p_pattern, uint16_t nth);

/// nth of 0 means append to end of pattern list
/**@brief   Function to insert a pattern into the list of patterns in the nth slot.
 *
 * @param[in]       *pattern            A pointer to the pattern to be stored. Pattern will be cached in library memory.
 * @param[in]       nth                 The desired location in the pattern list, 1-indexed.
 *                                      A value of 0 appends to the end of the pattern list.
 *
 * @retval  NRFX_SUCCESS
 * @retval  NRFX_ERROR_INVALID_STATE
 * @retval  NRFX_ERROR_INVALID_ADDR
 * @retval  NRFX_ERROR_BUSY
 * @retval  NRFX_ERROR_NO_MEM
 */
nrfx_err_t insert_pattern(zappy_pattern_t const *pattern, uint16_t nth);

nrfx_err_t delete_pattern(uint16_t nth);

nrfx_err_t delete_all_patterns(void);

#define STORAGE_EVENT_SIZE 0

#endif //STORAGE_H
