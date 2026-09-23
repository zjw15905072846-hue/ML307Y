#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    uint32_t initial_ms;
    uint32_t blink_ms;
    uint32_t wait_ms;
    uint32_t half_period_ms;
} al_pattern_t;

typedef struct
{
    bool led;
    bool buzzer;
    bool active;
} al_output_t;

typedef struct
{
    al_pattern_t pattern;
    uint32_t event_id;
    uint32_t started;
    bool active;
    bool acked;
} al_indicator_t;

/*-------------------------------------------function---------------------------------------------*/
void al_indicator_start(al_indicator_t *ctx, const al_pattern_t *pattern, uint32_t event_id,
                        uint32_t now);
void al_indicator_ack(al_indicator_t *ctx, uint32_t event_id);
void al_indicator_stop(al_indicator_t *ctx);
al_output_t al_indicator_tick(al_indicator_t *ctx, uint32_t now);
