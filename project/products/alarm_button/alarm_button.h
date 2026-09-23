#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "alarm_core.h"
/*-------------------------------------------define---------------------------------------------*/
#define AB_PRODUCT_ID 0x41420101U

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    uint32_t debounce_ms;
    uint32_t heartbeat_ms;
    al_pattern_t pattern;
} ab_config_t;

typedef struct
{
    void *user;
    bool (*output)(void *user, bool led, bool buzzer);
    void (*fault)(void *user, int error);
    int (*submit_event)(void *user, uint32_t request, const al_event_t *event);
} ab_io_t;

typedef struct
{
    ab_config_t config;
    ab_io_t io;
    al_store_t *store;
    al_reporter_t *reporter;
    al_key_t key;
    al_indicator_t indicator;
    al_output_t output;
    uint32_t latest_event_id;
    uint32_t latest_request;
    uint32_t completed_request;
    unsigned pending_saves;
    bool background_idle;
    int last_error;
} ab_app_t;

/*-------------------------------------------function---------------------------------------------*/
/* 本产品独立配置，不影响其他产品。 */
ab_config_t ab_default_config(void);
int ab_init(ab_app_t *app, const ab_config_t *config, const ab_io_t *io, al_store_t *store,
            al_reporter_t *reporter);
void ab_poll(ab_app_t *app, bool pressed, uint32_t now, const al_event_t *telemetry);
int ab_ack(ab_app_t *app, uint16_t sequence, uint8_t response, uint32_t now);
bool ab_can_sleep(const ab_app_t *app);

/* FIFO worker completion; only the current request can bind the current indication. */
void ab_saved(ab_app_t *app, uint32_t request, uint32_t event_id, int result);
void ab_confirmed(ab_app_t *app, uint32_t event_id);
