/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/handset_payload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : main
* Description    : 校验手报注册、事件、历史字节位置及未知字段拒绝
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
int main(void)
{
    kaiwan_handset_identity_t id = {
        "869975034441082", "460113118743732", "89861120224014398762", 0x10, false, 0xff};
    alarm_event_t e = {0};
    uint8_t data[55];
    e.battery_mv = 3700;
    e.battery_percent = 100;
    e.csq = 21;
    e.valid = 7;
    e.event_type = 0x0c;
    assert(kaiwan_handset_event_payload(&id, &e, false, data, sizeof(data)) == 21);
    assert(data[0] == 4 && data[16] == 12 && data[17] == 0x25 && data[18] == 21 &&
           data[19] == 100 && data[20] == 16);
    assert(memcmp(data + 1, id.imei, 15) == 0);
    assert(kaiwan_handset_registration_payload(&id, &e, data, sizeof(data)) == 55);
    assert(data[0] == 4 && data[51] == 0x25 && data[54] == 16);
    assert(kaiwan_handset_event_payload(&id, &e, true, data, sizeof(data)) == ALARM_ERROR_NOT_READY);
    e.valid |= ALARM_TIME_UTC;
    e.utc_seconds = 0x64224ce5;
    assert(kaiwan_handset_event_payload(&id, &e, true, data, sizeof(data)) == 27 && data[0] == 1 &&
           data[1] == 1 && data[2] == 4);
    assert(data[23] == 0x64 && data[24] == 0x22 && data[25] == 0x4c && data[26] == 0xe5);
    e.valid = 0;
    assert(kaiwan_handset_event_payload(&id, &e, false, data, sizeof(data)) == ALARM_ERROR_NOT_READY);
    id.unknown_telemetry_verified = true;
    assert(kaiwan_handset_event_payload(&id, &e, false, data, sizeof(data)) == 21 && data[17] == 0xff &&
           data[19] == 0xff);
    id.imei[0] = 'x';
    assert(kaiwan_handset_event_payload(&id, &e, false, data, sizeof(data)) == ALARM_ERROR_ARGUMENT);
    puts("handset payload: registration/event/history/unknown-field tests passed");
    return 0;
}
