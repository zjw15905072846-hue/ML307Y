#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
/*-------------------------------------------typedef---------------------------------------------*/
/* False means unavailable. Never synthesize charge state or battery percentage. */
typedef bool (*battery_read_mv_fn)(void *user, uint16_t *millivolts);

typedef struct
{
    void *user;
    battery_read_mv_fn read_mv;
} battery_if_t;

/*-------------------------------------------function---------------------------------------------*/
