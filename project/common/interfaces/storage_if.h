#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*-------------------------------------------define---------------------------------------------*/
#define STORAGE_OK 0
#define STORAGE_EMPTY 1
#define STORAGE_IO_ERROR (-2)
#define STORAGE_FOREIGN (-4)
#define STORAGE_CORRUPT (-5)
#define STORAGE_NOT_READY (-9)

/*-------------------------------------------typedef---------------------------------------------*/
/* Read must distinguish verified blank media from read failure. */
typedef struct
{
    void *user;
    int (*read)(void *user, void *data, size_t size);
    bool (*write)(void *user, const void *data, size_t size);
    int (*warning)(void *user);
} storage_if_t;

/*-------------------------------------------function---------------------------------------------*/
