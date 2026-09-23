#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "alarm_core.h"
/*-------------------------------------------define---------------------------------------------*/
#define KH_DEVICE_TYPE 0x04U
#define KH_EVENT_BYTES 21U
#define KH_REGISTER_BYTES 55U
#define KH_HISTORY_BYTES 27U

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    char imei[16];
    char imsi[16];
    char iccid[21];
    uint8_t firmware;
    bool unknown_telemetry_verified;
    uint8_t unknown_telemetry;
} kh_identity_t;

/*-------------------------------------------function---------------------------------------------*/
/* 未知值只有经平台确认后才允许编码；长度单位均为字节。 */
int kh_event_payload(const kh_identity_t *identity, const al_event_t *event, bool history,
                     uint8_t *out, size_t capacity);
int kh_registration_payload(const kh_identity_t *identity, const al_event_t *telemetry,
                            uint8_t *out, size_t capacity);
