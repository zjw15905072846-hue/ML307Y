/*------------------------------------------includes--------------------------------------------*/
#include "handset_payload.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kh_digits
* Description    : 校验固定长十进制设备身份
* Input          : s - 固定长度身份字符串；n - 字符数
* Output         : 无
* Return         : true - n位十进制且随后为终止符
* Attention      : s必须有至少n+1字节可读空间
*******************************************************************************/
static bool kh_digits(const char *s, size_t n)
{
    size_t i;
    if (!s)
    {
        return false;
    }
    for (i = 0; i < n; i++)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            return false;
        }
    }
    return s[n] == 0;
}

/*******************************************************************************
* Function Name  : kh_telemetry
* Description    : 编码真实遥测，拒绝未经平台确认的未知值
* Input          : identity - 固件与未知值约定；event - 遥测快照；out - 4字节输出
* Output         : 电压、CSQ、百分比、固件版本
* Return         : AL_OK或数据/未知值约定错误
* Attention      : 不能把未知电量、信号编码成虚构有效值
*******************************************************************************/
static int kh_telemetry(const kh_identity_t *identity, const al_event_t *event, uint8_t *out)
{
    uint8_t flags = event->valid;
    if ((flags & 7U) != 7U && !identity->unknown_telemetry_verified)
    {
        return AL_ERR_NOT_READY;
    }
    if (((flags & AL_TELEMETRY_PERCENT) && event->battery_percent > 100) ||
        ((flags & AL_TELEMETRY_CSQ) && event->csq > 31) ||
        ((flags & AL_TELEMETRY_VOLTAGE) && event->battery_mv > 25400))
    {
        return AL_ERR_ARGUMENT;
    }
    out[0] = (flags & AL_TELEMETRY_VOLTAGE) ? (uint8_t)((event->battery_mv + 50U) / 100U)
                                            : identity->unknown_telemetry;
    out[1] = (flags & AL_TELEMETRY_CSQ) ? event->csq : identity->unknown_telemetry;
    out[2] = (flags & AL_TELEMETRY_PERCENT) ? event->battery_percent : identity->unknown_telemetry;
    out[3] = identity->firmware;
    return AL_OK;
}

/*******************************************************************************
* Function Name  : kh_event_payload
* Description    : 编码手报04实时事件或单条历史事件
* Input          : identity - 设备身份；event - 事件；history - 历史标志；out/capacity - 缓冲区
* Output         : 手报实时或历史数据体
* Return         : 正数为数据体长度；负数为AL错误码
* Attention      : 历史编码要求真实UTC；设备类型固定为手报0x04
*******************************************************************************/
int kh_event_payload(const kh_identity_t *identity, const al_event_t *event, bool history,
                     uint8_t *out, size_t capacity)
{
    uint8_t *body;
    int r;
    if (!identity || !event || !out || !kh_digits(identity->imei, 15))
    {
        return AL_ERR_ARGUMENT;
    }
    if (capacity < (history ? KH_HISTORY_BYTES : KH_EVENT_BYTES))
    {
        return AL_ERR_ARGUMENT;
    }
    if (history && (!(event->valid & AL_TIME_UTC) || !event->utc_seconds))
    {
        return AL_ERR_NOT_READY;
    }
    body = out + (history ? 2 : 0);
    body[0] = KH_DEVICE_TYPE;
    memcpy(body + 1, identity->imei, 15);
    body[16] = event->event_type;
    r = kh_telemetry(identity, event, body + 17);
    if (r)
    {
        return r;
    }
    if (history)
    {
        out[0] = 1;
        out[1] = 1;
        body[21] = (uint8_t)(event->utc_seconds >> 24);
        body[22] = (uint8_t)(event->utc_seconds >> 16);
        body[23] = (uint8_t)(event->utc_seconds >> 8);
        body[24] = (uint8_t)event->utc_seconds;
    }
    return history ? KH_HISTORY_BYTES : KH_EVENT_BYTES;
}

/*******************************************************************************
* Function Name  : kh_registration_payload
* Description    : 注册始终使用04手报，防止与报警类型不一致
* Input          : identity - IMEI/IMSI/ICCID；telemetry - 遥测；out/capacity - 输出缓冲区
* Output         : 手报注册数据体
* Return         : 55字节或AL错误码
* Attention      : 注册、心跳、报警须保持同一设备类型
*******************************************************************************/
int kh_registration_payload(const kh_identity_t *identity, const al_event_t *telemetry,
                            uint8_t *out, size_t capacity)
{
    int r;
    if (!identity || !telemetry || !out || capacity < KH_REGISTER_BYTES ||
        !kh_digits(identity->imei, 15) || !kh_digits(identity->imsi, 15) ||
        !kh_digits(identity->iccid, 20))
    {
        return AL_ERR_ARGUMENT;
    }
    out[0] = KH_DEVICE_TYPE;
    memcpy(out + 1, identity->imei, 15);
    memcpy(out + 16, identity->imsi, 15);
    memcpy(out + 31, identity->iccid, 20);
    r = kh_telemetry(identity, telemetry, out + 51);
    return r ? r : KH_REGISTER_BYTES;
}
