#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------typedef---------------------------------------------*/
typedef enum
{
    AL_KEY_NONE = 0,
    AL_KEY_PRESS,
    AL_KEY_RELEASE
} al_key_event_t;

typedef struct
{
    bool candidate;
    bool stable;
    uint32_t since;
} al_key_t;

/*-------------------------------------------function---------------------------------------------*/
al_key_event_t al_key_sample(al_key_t *ctx, bool pressed, uint32_t now, uint32_t debounce_ms);
