#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_core.h"
/*-------------------------------------------define---------------------------------------------*/
#define KAIWAN_HANDSET_DEVICE_TYPE 0x04U /* 铠湾手报设备类型。 */
#define KAIWAN_HANDSET_EVENT_BYTES 21U /* 单条实时报警数据体长度。 */
#define KAIWAN_HANDSET_REGISTER_BYTES 55U /* 手报注册数据体长度。 */
#define KAIWAN_HANDSET_HISTORY_BYTES 27U /* 含发生时间的单条历史数据体长度。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 手报身份与遥测约定由产品提供，不由共用协议层推断。 */
typedef struct
{
    char imei[16]; /* 15 位十进制 IMEI 加终止符。 */
    char imsi[16]; /* 15 位十进制 IMSI 加终止符。 */
    char iccid[21]; /* 20 位十进制 ICCID 加终止符。 */
    uint8_t firmware; /* 注册及事件遥测携带的固件版本。 */
    bool unknown_telemetry_verified; /* 平台确认未知遥测占位值后才置位。 */
    uint8_t unknown_telemetry; /* 经平台确认的未知值编码。 */
} kaiwan_handset_identity_t;

/*-------------------------------------------function---------------------------------------------*/
/* 未知值只有经平台确认后才允许编码；history 还要求有效发生 UTC。 */
int kaiwan_handset_event_payload(const kaiwan_handset_identity_t *identity, const alarm_event_t *event, bool history,
                     uint8_t *output, size_t capacity);
/* 对 IMEI、IMSI、ICCID 做定长数字校验后输出手报注册数据体。 */
int kaiwan_handset_registration_payload(const kaiwan_handset_identity_t *identity, const alarm_event_t *telemetry,
                            uint8_t *output, size_t capacity);
