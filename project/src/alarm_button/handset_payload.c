/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/handset_payload.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kaiwan_handset_digits
* Description    : 校验固定长十进制设备身份
* Input          : text - 固定长度身份字符串；length - 字符数
* Output         : 无
* Return         : true - length位十进制且随后为终止符
* Attention      : text必须有至少length+1字节可读空间
*******************************************************************************/
static bool kaiwan_handset_digits(const char *text, size_t length)
{
    size_t index;
    if (!text)
    {
        return false;
    }
    for (index = 0; index < length; index++)
    {
        if (text[index] < '0' || text[index] > '9')
        {
            return false;
        }
    }
    return text[length] == 0;
}

/*******************************************************************************
* Function Name  : kaiwan_handset_telemetry
* Description    : 编码真实遥测，拒绝未经平台确认的未知值
* Input          : identity - 固件与未知值约定；event - 遥测快照；output - 4字节输出
* Output         : 电压、CSQ、百分比、固件版本
* Return         : ALARM_OK或数据/未知值约定错误
* Attention      : 不能把未知电量、信号编码成虚构有效值
*******************************************************************************/
static int kaiwan_handset_telemetry(const kaiwan_handset_identity_t *identity, const alarm_event_t *event, uint8_t *output)
{
    uint8_t flags = event->valid;
    /* 缺任一遥测字段时，仅允许使用已与平台核验的未知值编码。 */
    if ((flags & 7U) != 7U && !identity->unknown_telemetry_verified)
    {
        return ALARM_ERROR_NOT_READY;
    }
    if (((flags & ALARM_TELEMETRY_PERCENT) && event->battery_percent > 100) ||
        ((flags & ALARM_TELEMETRY_CSQ) && event->csq > 31) ||
        ((flags & ALARM_TELEMETRY_VOLTAGE) && event->battery_mv > 25400))
    {
        return ALARM_ERROR_ARGUMENT;
    }
    /* 电压由毫伏四舍五入到 0.1 V，其他字段保持原协议编码。 */
    output[0] = (flags & ALARM_TELEMETRY_VOLTAGE) ? (uint8_t)((event->battery_mv + 50U) / 100U)
                                            : identity->unknown_telemetry;
    output[1] = (flags & ALARM_TELEMETRY_CSQ) ? event->csq : identity->unknown_telemetry;
    output[2] = (flags & ALARM_TELEMETRY_PERCENT) ? event->battery_percent : identity->unknown_telemetry;
    output[3] = identity->firmware;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_handset_event_payload
* Description    : 编码手报04实时事件或单条历史事件
* Input          : identity - 设备身份；event - 事件；history - 历史标志；output/capacity - 缓冲区
* Output         : 手报实时或历史数据体
* Return         : 正数为数据体长度；负数为AL错误码
* Attention      : 历史编码要求真实UTC；设备类型固定为手报0x04
*******************************************************************************/
int kaiwan_handset_event_payload(const kaiwan_handset_identity_t *identity, const alarm_event_t *event, bool history,
                     uint8_t *output, size_t capacity)
{
    uint8_t *body;
    int result;
    if (!identity || !event || !output || !kaiwan_handset_digits(identity->imei, 15))
    {
        return ALARM_ERROR_ARGUMENT;
    }
    if (capacity < (history ? KAIWAN_HANDSET_HISTORY_BYTES : KAIWAN_HANDSET_EVENT_BYTES))
    {
        return ALARM_ERROR_ARGUMENT;
    }
    if (history && (!(event->valid & ALARM_TIME_UTC) || !event->utc_seconds))
    {
        return ALARM_ERROR_NOT_READY;
    }
    /* 历史帧前两字节留给数量与记录数，后面复用实时事件布局。 */
    body = output + (history ? 2 : 0);
    body[0] = KAIWAN_HANDSET_DEVICE_TYPE;
    memcpy(body + 1, identity->imei, 15);
    body[16] = event->event_type;
    result = kaiwan_handset_telemetry(identity, event, body + 17);
    if (result)
    {
        return result;
    }
    if (history)
    {
        /* 历史记录必须携带真实 UTC，按协议大端写入四字节时间。 */
        output[0] = 1;
        output[1] = 1;
        body[21] = (uint8_t)(event->utc_seconds >> 24);
        body[22] = (uint8_t)(event->utc_seconds >> 16);
        body[23] = (uint8_t)(event->utc_seconds >> 8);
        body[24] = (uint8_t)event->utc_seconds;
    }
    return history ? KAIWAN_HANDSET_HISTORY_BYTES : KAIWAN_HANDSET_EVENT_BYTES;
}

/*******************************************************************************
* Function Name  : kaiwan_handset_registration_payload
* Description    : 注册始终使用04手报，防止与报警类型不一致
* Input          : identity - IMEI/IMSI/ICCID；telemetry - 遥测；output/capacity - 输出缓冲区
* Output         : 手报注册数据体
* Return         : 55字节或AL错误码
* Attention      : 注册、心跳、报警须保持同一设备类型
*******************************************************************************/
int kaiwan_handset_registration_payload(const kaiwan_handset_identity_t *identity, const alarm_event_t *telemetry,
                            uint8_t *output, size_t capacity)
{
    int result;
    if (!identity || !telemetry || !output || capacity < KAIWAN_HANDSET_REGISTER_BYTES ||
        !kaiwan_handset_digits(identity->imei, 15) || !kaiwan_handset_digits(identity->imsi, 15) ||
        !kaiwan_handset_digits(identity->iccid, 20))
    {
        return ALARM_ERROR_ARGUMENT;
    }
    output[0] = KAIWAN_HANDSET_DEVICE_TYPE;
    memcpy(output + 1, identity->imei, 15);
    memcpy(output + 16, identity->imsi, 15);
    memcpy(output + 31, identity->iccid, 20);
    result = kaiwan_handset_telemetry(identity, telemetry, output + 51);
    return result ? result : KAIWAN_HANDSET_REGISTER_BYTES;
}
