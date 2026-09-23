#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "storage_if.h"
/*-------------------------------------------define---------------------------------------------*/
#define SNAPSHOT_MAX_PAYLOAD 2048U
#define SNAPSHOT_HEADER_BYTES 32U
#define SNAPSHOT_BYTES (SNAPSHOT_HEADER_BYTES + SNAPSHOT_MAX_PAYLOAD)

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    void *user;
    int (*probe)(void *user, unsigned slot);
    int (*read)(void *user, unsigned slot, void *data, size_t capacity, size_t *actual);
    bool (*write_sync)(void *user, unsigned slot, const void *data, size_t size);
    bool (*preserve)(void *user, unsigned slot);
} snapshot_file_if_t;

typedef struct
{
    snapshot_file_if_t files;
    uint32_t product_id;
    uint64_t generation;
    size_t payload_size;
    int active;
    int warning;
    unsigned corrupt_mask;
    bool ready;
    uint8_t record[SNAPSHOT_BYTES];
    uint8_t verify[SNAPSHOT_BYTES];
} snapshot_store_t;

/*-------------------------------------------function---------------------------------------------*/
/* Read checks both slots before permitting any write; unknown formats are preserved. */
storage_if_t snapshot_storage(snapshot_store_t *store, uint32_t product_id,
                              const snapshot_file_if_t *files);
