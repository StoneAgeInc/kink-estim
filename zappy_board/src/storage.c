//
// Created by Benjamin Riggs on 3/10/20.
//

#include <stdlib.h>
#include <string.h>

#include "prv_utils.h"
#include "storage.h"
#include "patterns.h"
#include "prv_power_manager.h"

#include "fds.h"
#include "nrfx.h"
#include "nrf_atfifo.h"

#define MIN_RECORD_KEY 0x0001
#define EMPTY_RECORD_KEY FDS_RECORD_KEY_DIRTY
#define PATTERN_FILE 0x1000

uint16_t volatile pattern_storage_count;

typedef volatile struct pattern_index_t {
    struct pattern_index_t volatile *p, *n;
    uint16_t record_key;
    uint32_t record_id;
    zappy_pattern_t *p_pattern;
} pattern_index_t;

typedef nrfx_err_t (*storage_func_t)(pattern_index_t *p_idx);

typedef struct {
    storage_func_t func;
    pattern_index_t *p_idx;
} storage_command_t;

NRF_ATFIFO_DEF(storage_command_queue, storage_command_t, 3);

static bool volatile storage_initialized = false;
static bool volatile storage_busy = false;
static uint16_t volatile shufflin = 0;
static pattern_index_t volatile *p_modified_idx = NULL;

static uint8_t volatile pattern_write_cache[MAX_PATTERN_BYTE_LENGTH];

static pattern_index_t volatile *p_pattern_index_head = NULL;

static pattern_index_t *volatile unused_node = NULL;
static pattern_index_t volatile *new_idx_node(uint16_t record_key, uint32_t record_id, zappy_pattern_t *p_pattern) {
    pattern_index_t volatile *node;
    if (unused_node != NULL) {
        node = unused_node;
        unused_node = NULL;
    } else {
        node = malloc(sizeof(pattern_index_t));
    }
    node->record_key = record_key;
    node->record_id = record_id;
    node->p_pattern = p_pattern;
    node->p = node->n = NULL;
    return node;
}

static void bt_insert(pattern_index_t volatile *volatile *p_node, pattern_index_t *child) {
    if (!*p_node) *p_node = child;  // Node is NULL, add child here
    else bt_insert(child->record_key > (*p_node)->record_key ? &(*p_node)->p : &(*p_node)->n, child);   // Recurse
}

/**@brief Recursively transform binary tree into linked list. */
static void flatten(pattern_index_t volatile *node, pattern_index_t volatile **p_head) {
    if (!node) return;
    pattern_index_t *p = node->p;       // Save first branch for later
    node->p = NULL;
    flatten(p, p_head);                 // Flatten first branch
    pattern_index_t *n = node->n;       // Save second branch for later
    node->n = *p_head;                  // Replace second branch with head of flattened branch
    if (*p_head) (*p_head)->p = node;   // If head of flattened branch isn't NULL, attach this node to it
    *p_head = node;                     // Make this node the head of this flattened branch
    flatten(n, p_head);                 // Flatten second branch
}

#if 0
static void ll_echo(pattern_index_t *node) {
    if (!node) return;
    printf("Key: %2i, %8i %8i %8i\n",
        node->record_key, (int)node->p, (int)node, (int)node->n);
    ll_echo(node->n);
}

static void bt_echo(pattern_index_t *node, uint8_t indent) {
    if (!node) return;
    uint16_t key = node->record_key;
    if (node->p) bt_echo(node->p, indent+2);
    printf("%*skey: %i\n", indent, "", key);
    if (node->n) bt_echo(node->n, indent+2);
}

static void idx_echo(uint16_t nth) {
    pattern_index_t *idx = nth_index(nth);
    printf("%i - Key: %i\n", nth, idx->record_key);
}
#endif

/**@brief   Build index for patterns.
 *
 * Function iterates through the pattern file and attempts to open all records for reading.
 * Records that fail their CRC check are deleted.
 *
 * Records are stored in a sorted linked-list for easy iteration, starting at *p_pattern_index_head.
 */
static void build_index(void) {
    while (!storage_initialized) {
        prv_wait();
    }
    fds_record_desc_t desc = {0};
    fds_find_token_t token = {0};
    fds_flash_record_t flash_record = {0};
    // Build a binary tree of pattern indices.
    pattern_index_t *bt_root = NULL;
    // Get around const (which we still want for external access)
    // Retrieve first record
    nrfx_err_t err = fds_record_find_in_file(PATTERN_FILE, &desc, &token);
    while (err == NRF_SUCCESS) {
        // Open record for reading
        err = fds_record_open(&desc, &flash_record);
        if (err == NRF_SUCCESS) {
            // Insert successfully opened records into binary tree
            bt_insert(&bt_root, new_idx_node(flash_record.p_header->record_key, flash_record.p_header->record_id,
                                             flash_record.p_data));
            // Update metadata
            pattern_storage_count++;
        } else if (err == FDS_ERR_CRC_CHECK_FAILED) {
            // Record is corrupt, delete it.
            err = fds_record_delete(&desc);
            if (err != NRF_SUCCESS) {
                APP_ERROR_CHECK(fds_record_close(&desc));
            }
        } else {
            APP_ERROR_CHECK(fds_record_close(&desc));
        }
        // Retrieve next record
        err = fds_record_find_in_file(PATTERN_FILE, &desc, &token);
    }
    if (err == FDS_ERR_NOT_FOUND) {
        // Found all records, generated binary tree index.
        // Flatten binary tree into linked list for easy iteration.
        flatten(bt_root, (pattern_index_t **) &p_pattern_index_head);
    } else {
        APP_ERROR_CHECK(err);
    }
}

pattern_index_t volatile *nth_idx_cache = NULL;
uint16_t volatile nth_idx_counter = 0;

/// 1-indexed counter
static pattern_index_t *nth_index(uint16_t n) {
    if (!p_pattern_index_head || !n) return NULL;
    if (!nth_idx_cache || n < nth_idx_counter || nth_idx_cache->record_key == EMPTY_RECORD_KEY) {
        nth_idx_cache = p_pattern_index_head;
        nth_idx_counter = nth_idx_cache->record_key != EMPTY_RECORD_KEY;
    }
    while (nth_idx_counter < n) {
        nth_idx_cache = nth_idx_cache->n;
        if (!nth_idx_cache) break;
        if (nth_idx_cache->record_key != EMPTY_RECORD_KEY) {    // Skip empty indices
            nth_idx_counter++;
        }
    }
    return nth_idx_cache;
}

static void open_pattern_records() {
    fds_record_desc_t desc = {0};
    fds_flash_record_t flash_record = {0};
    pattern_index_t *p_idx = p_pattern_index_head;
    while (p_idx) {
        if (p_idx->record_key != EMPTY_RECORD_KEY) {
            fds_descriptor_from_rec_id(&desc, p_idx->record_id);
            APP_ERROR_CHECK(fds_record_open(&desc, &flash_record));
            p_idx->p_pattern = flash_record.p_data;
        }
        p_idx = p_idx->n;
    }
}

static void wipe_pattern_index() {
    pattern_index_t *p_idx = p_pattern_index_head;
    while (p_idx) {
        p_idx->record_key = EMPTY_RECORD_KEY;
        p_idx = p_idx->n;
    }
}

static fds_stat_t volatile fds_stats = {0};

static void inline update_fds_stats() {
    fds_stat((fds_stat_t *) &fds_stats);
}

static bool inline check_free_space(size_t size) {
    return size / 4 <= fds_stats.freeable_words + fds_stats.largest_contig;
}

static bool inline have_storage_space(size_t size) {
    update_fds_stats();
    return check_free_space(size);
}

static nrfx_err_t gc_command() {
    nrfx_err_t err = fds_gc();
    if (err == NRFX_SUCCESS) storage_busy = true;
    APP_ERROR_CHECK_BOOL(err == NRFX_SUCCESS);
    return err;
}

static nrfx_err_t run_gc() {
    fds_record_desc_t desc = {0};
    pattern_index_t *p_idx = p_pattern_index_head;
    while (p_idx) {
        if (p_idx->record_key != EMPTY_RECORD_KEY) {
            fds_descriptor_from_rec_id(&desc, p_idx->record_id);
            APP_ERROR_CHECK(fds_record_close(&desc));
        }
        p_idx = p_idx->n;
    }
    return gc_command();
}

static nrfx_err_t write_command(fds_record_t *record, fds_reserve_token_t *token, pattern_index_t *node) {
    if (node == NULL) {
        p_modified_idx = new_idx_node(EMPTY_RECORD_KEY, 0, NULL);
    } else {
        p_modified_idx = node;
    }
    // Schedule pattern write (async w/ softdevice, sync w/o)
    storage_busy = true;
    nrfx_err_t err = fds_record_write_reserved(NULL, record, token);
    if (err != NRFX_SUCCESS) {
        fds_reserve_cancel(token);
        if (node == NULL) unused_node = p_modified_idx;
        p_modified_idx = NULL;
        storage_busy = false;
    }
    return err;
}

static nrfx_err_t delete_command(pattern_index_t *p_index) {
    if (p_index->record_key == EMPTY_RECORD_KEY) return NRFX_SUCCESS;
    fds_record_desc_t desc;
    fds_descriptor_from_rec_id(&desc, p_index->record_id);
    p_modified_idx = p_index;
    nrfx_err_t err = fds_record_delete(&desc);
    if (err == NRFX_SUCCESS) storage_busy = true;
    APP_ERROR_CHECK_BOOL(err == NRFX_SUCCESS);
    return err;
}

static nrfx_err_t shuffle_command(pattern_index_t *p_index) {
    nrfx_err_t err;
    if (!p_index || p_index->n == nth_index(shufflin)) {  // We can stop shufflin'
        uint16_t tmp = shufflin;
        // Set as done to avoid busy check
        shufflin = 0;
        err = insert_pattern((zappy_pattern_t *) pattern_write_cache, tmp);
        if (err != NRFX_SUCCESS) {
            // Not actually done shufflin'
            shufflin = tmp;
            APP_ERROR_CHECK(err);
        }
    } else {
        nrf_atfifo_item_put_t ctx;
        storage_command_t *c = nrf_atfifo_item_alloc(storage_command_queue, &ctx);
        if (!c) return NRFX_ERROR_NO_MEM;
        fds_record_t record = {
            .file_id = PATTERN_FILE,
            .key = p_index->record_key + 1,
            .data.p_data = p_index->p_pattern,
            .data.length_words = ZAPPY_PATTERN_SIZE(p_index->p_pattern) / 4,
        };
        fds_record_desc_t desc = {0};
        fds_descriptor_from_rec_id(&desc, p_index->record_id);
        err = fds_record_update(&desc, &record);
        if (err == NRFX_SUCCESS) {
            storage_busy = true;
            p_modified_idx = p_index->n;
            // Add the next update command to the queue
            c->func = (storage_func_t) &shuffle_command;
            c->p_idx = p_index->p;
            nrf_atfifo_item_put(storage_command_queue, &ctx);
        } else if (err == FDS_ERR_NO_SPACE_IN_FLASH) {
            run_gc();
            return NRFX_ERROR_BUSY;
        } else {
            APP_ERROR_CHECK(err);
        }
    }
    return err;
}

static void next_storage_command() {
    nrf_atfifo_item_get_t context;
    storage_command_t *c = nrf_atfifo_item_get(storage_command_queue, &context);
    if (!c) return; // Nothing to do
    nrfx_err_t err = c->func(c->p_idx);
    if (err == NRFX_SUCCESS) {
        // Only remove item from queue if func returns successfully
        nrf_atfifo_item_free(storage_command_queue, &context);
    } else {
        APP_ERROR_CHECK(err);
    }
}

static void fds_evt_handler(fds_evt_t const *p_evt) {
    APP_ERROR_CHECK_BOOL(p_evt->result == NRF_SUCCESS);
    switch (p_evt->id) {
        case FDS_EVT_INIT:
            storage_initialized = true;
            break;

        case FDS_EVT_WRITE: {
            if (p_evt->result == NRF_SUCCESS) {
                p_modified_idx->record_key = p_evt->write.record_key;
                p_modified_idx->record_id = p_evt->write.record_id;
                fds_record_desc_t desc = {0};
                fds_flash_record_t flash_record = {0};
                fds_descriptor_from_rec_id(&desc, p_modified_idx->record_id);
                nrfx_err_t err = fds_record_open(&desc, &flash_record);
                APP_ERROR_CHECK(err);
                p_modified_idx->p_pattern = flash_record.p_data;
                pattern_storage_count++;
                storage_busy = false;
            }
        }
            break;

        case FDS_EVT_UPDATE: {
            if (p_evt->result == NRF_SUCCESS) {
                p_modified_idx->record_key = p_evt->write.record_key;
                p_modified_idx->record_id = p_evt->write.record_id;
                fds_record_desc_t desc = {0};
                fds_flash_record_t flash_record = {0};
                fds_descriptor_from_rec_id(&desc, p_modified_idx->record_id);
                fds_record_open(&desc, &flash_record);
                p_modified_idx->p_pattern = flash_record.p_data;
                p_modified_idx->p->record_key = EMPTY_RECORD_KEY;
                storage_busy = false;
            }
        }
            break;

        case FDS_EVT_DEL_RECORD: {
            if (p_evt->result == NRF_SUCCESS) {
                p_modified_idx->record_key = EMPTY_RECORD_KEY;
                pattern_storage_count--;
                storage_busy = false;
            }
        }
            break;

        case FDS_EVT_DEL_FILE: {
            if (p_evt->result == NRF_SUCCESS) {
                pattern_storage_count = 0;
                storage_busy = false;
                wipe_pattern_index();
            }
        }
            break;

        case FDS_EVT_GC: {
            if (p_evt->result == NRF_SUCCESS) {
                storage_busy = false;
                open_pattern_records();
                #if DEBUG
                update_fds_stats();
                #endif
            } else if (p_evt->result == FDS_ERR_OPERATION_TIMEOUT) {
                nrfx_err_t err = fds_gc();
                APP_ERROR_CHECK(err);
                // Skip normal queue because we'll end up right back here in next event.
                return;
            }
        }
            break;

        default:
            break;
    }
    next_storage_command();
}

void storage_init() {
    pattern_storage_count = 0;
    NRF_ATFIFO_INIT(storage_command_queue);
    APP_ERROR_CHECK(fds_register(fds_evt_handler));
    APP_ERROR_CHECK(fds_init());
    // Init is async, so make sure it's actually started before app starts.
    APP_ERROR_CHECK(app_sched_event_put(NULL, 0, SCHED_FN(build_index)));
    // Update fds_stats, mostly for debug purposes.
    APP_ERROR_CHECK(app_sched_event_put(NULL, 0, SCHED_FN(update_fds_stats)));
}

bool is_busy(void) {
    return storage_busy;
}

bool next_pattern_index(uint16_t *p_nth, bool reverse) {
    if (!storage_initialized || !pattern_storage_count) {
        *p_nth = 0;
        return false;
    }
    uint16_t idx;
    if (*p_nth == 0 && reverse) {
        idx = pattern_storage_count;
    } else {
        idx = *p_nth + (reverse ? -1 : 1);
    }
    pattern_index_t *p_idx = nth_index(idx);
    if (!p_idx) {
        *p_nth = 0;
        return false;
    }
    *p_nth = p_idx->record_key;
    return true;
}

nrfx_err_t get_nth_pattern(zappy_pattern_t const **p_pattern, uint16_t nth) {
    if (!storage_initialized) return NRFX_ERROR_INVALID_STATE;
    if (nth > pattern_storage_count) return NRFX_ERROR_INVALID_ADDR;
    pattern_index_t *idx = nth_index(nth);
    if (!nth) return NRFX_ERROR_INVALID_ADDR;
    *p_pattern = idx->p_pattern;
    return NRFX_SUCCESS;
}

nrfx_err_t insert_pattern(zappy_pattern_t const *pattern, uint16_t nth) {
    if (!storage_initialized) return NRFX_ERROR_INVALID_STATE;
    if (shufflin || storage_busy) return NRFX_ERROR_BUSY;
    if (nth > pattern_storage_count + 1) return NRFX_ERROR_INVALID_ADDR;
    if (nth == pattern_storage_count + 1) nth = 0;   // Append
    nrfx_err_t err;

    size_t pattern_len = ZAPPY_PATTERN_SIZE(pattern);
    // Copy pattern to cache
    memcpy((void *) pattern_write_cache, pattern, pattern_len);

    // Reserve space for new pattern
    fds_reserve_token_t token = {0};
    // Pattern length is always a multiple of 4
    err = fds_reserve(&token, pattern_len / 4);
    if (err == FDS_ERR_NO_SPACE_IN_FLASH) {
        if (!have_storage_space(pattern_len)) return NRFX_ERROR_NO_MEM;
        DEBUG_BREAKPOINT;
        run_gc();
        return NRFX_ERROR_BUSY;
    } else if (err != NRFX_SUCCESS) {
        DEBUG_BREAKPOINT;
        return err;
    }

    // Create record needed to save pattern
    fds_record_t record = {
        .file_id = PATTERN_FILE,
        .key = EMPTY_RECORD_KEY,
        .data.p_data = (void *) pattern_write_cache,
        .data.length_words = pattern_len / 4,
    };

    // Figure out record key; this gets complicated.
    // index pointers used to track state
    pattern_index_t *p_idx = NULL;
    pattern_index_t *p_prev_idx = NULL;
    if (!pattern_storage_count) {
        // No patterns yet stored
        record.key = MIN_RECORD_KEY;
        // Store pattern
        err = write_command(&record, &token, NULL);
        if (err == NRFX_SUCCESS) {
            // First pattern
            p_pattern_index_head = p_modified_idx;
        }
    } else if (!nth) {
        // Append pattern to end
        p_idx = nth_index(pattern_storage_count);
        ASSERT(p_idx != NULL); // Shouldn't be NULL, but test to make sure.
        record.key = p_idx->record_key + 1;
        p_prev_idx = p_idx;
        // Store pattern
        err = write_command(&record, &token, NULL);
        if (err == NRFX_SUCCESS) {
            // Appending
            p_prev_idx->n = p_modified_idx;
            p_modified_idx->p = p_prev_idx;
        }
    } else {
        // Insert pattern into nth slot
        p_idx = nth_index(nth);
        ASSERT(p_idx != NULL); // Shouldn't be NULL, but test to make sure.
        // Pattern index is in order of record key, so previous key will be less than this record key or EMPTY,
        // or previous index will be NULL.
        p_prev_idx = p_idx->p;
        if (p_prev_idx && p_prev_idx->record_key == EMPTY_RECORD_KEY) {
            // If previous key is EMPTY, this key - 1 is safe to use.
            record.key = p_idx->record_key - 1;
            // Re-use previous node after storing pattern.
            err = write_command(&record, &token, p_prev_idx);
        } else if (p_prev_idx && p_prev_idx->record_key < p_idx->record_key - 1) {
            // If previous key is smaller than this key - 1, add a new index node and link it up appropriately.
            // Give the new node as low a key as possible.
            record.key = p_prev_idx->record_key + 1;
            // Store pattern
            err = write_command(&record, &token, NULL);
            if (err == NRFX_SUCCESS) {
                p_prev_idx->n = p_modified_idx;
                p_modified_idx->p = p_prev_idx;
                p_idx->p = p_modified_idx;
                p_modified_idx->n = p_idx;
            }
        } else {
            // If previous node is NULL, or previous key is only 1 less, stuff must be moved.
            // Shuffle before writing this pattern, so cancel memory reservation.
            fds_reserve_cancel(&token);
            // Find first unused record key later in the list.
            pattern_index_t volatile *p_next_idx = p_idx->n;
            update_fds_stats();
            while (true) {
                // Ensure sufficient space in memory to shuffle current index.
                if (!check_free_space(ZAPPY_PATTERN_SIZE(p_idx->p_pattern) + pattern_len)) {
                    return NRFX_ERROR_NO_MEM;
                }
                if (!p_next_idx) {
                    // End of the list, append an index for updated record.
                    p_modified_idx = new_idx_node(EMPTY_RECORD_KEY, 0, NULL);
                    p_idx->n = p_modified_idx;
                    p_modified_idx->p = p_idx;
                    break;
                }
                if (p_next_idx->record_key == EMPTY_RECORD_KEY) {
                    // Reuse existing index for updated record.
                    p_modified_idx = p_next_idx;
                    break;
                }
                if (p_next_idx->record_key > p_idx->record_key + 1) {
                    // A gap of at least 1 between record_keys, insert new index for updated record
                    p_modified_idx = new_idx_node(EMPTY_RECORD_KEY, 0, NULL);
                    p_idx->n = p_next_idx->p = p_modified_idx;
                    p_modified_idx->p = p_idx;
                    p_modified_idx->n = p_next_idx;
                    break;
                }
                // Iterate to next index
                p_idx = p_next_idx;
                p_next_idx = p_next_idx->n;
            }
            // Start shufflin
            shufflin = nth;
            // Function has been called synchronously, so just return nested result
            return shuffle_command(p_idx);
        }
    }
    return err;
}

nrfx_err_t delete_pattern(uint16_t nth) {
    if (!storage_initialized) return NRFX_ERROR_INVALID_STATE;
    if (shufflin || storage_busy) return NRFX_ERROR_BUSY;
    if (!nth || nth > pattern_storage_count) return NRFX_ERROR_INVALID_ADDR;
    pattern_index_t *p_idx = nth_index(nth);
    nrfx_err_t err = delete_command(p_idx);
    APP_ERROR_CHECK(err);
    if (err == FDS_ERR_NO_SPACE_IN_QUEUES) return NRFX_ERROR_BUSY;
    return err;
}

nrfx_err_t delete_all_patterns(void) {
    if (!storage_initialized) return NRFX_ERROR_INVALID_STATE;
    if (shufflin || storage_busy) return NRFX_ERROR_BUSY;
    nrfx_err_t err = fds_file_delete(PATTERN_FILE);
    APP_ERROR_CHECK(err);
    if (err == FDS_ERR_NO_SPACE_IN_QUEUES) return NRFX_ERROR_BUSY;
    storage_busy = true;
    nrf_atfifo_item_put_t ctx = {0};
    storage_command_t *c = nrf_atfifo_item_alloc(storage_command_queue, &ctx);
    c->func = (storage_func_t) &gc_command;
    c->p_idx = NULL;
    nrf_atfifo_item_put(storage_command_queue, &ctx);
    return err;
}
