/*------------------------------------------includes--------------------------------------------*/
#include "mqtt/mqtt_receive.h"
#include "test_support.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : main
* Description    : 验证分段组包身份和容量保护
* Input          : 无
* Output         : 断言
* Return         : 0成功
* Attention      : 不调用真实网络
*******************************************************************************/
int main(void)
{
    mqtt_receive_state_t receive;
    mqtt_receive_reset(&receive, 8);
    assert(mqtt_receive_feed(&receive, 8, 1, "down", 6, "abc", 3) == MQTT_RECEIVE_MORE);
    assert(mqtt_receive_feed(&receive, 7, 1, NULL, 6, "bad", 3) == MQTT_RECEIVE_STALE);
    assert(mqtt_receive_feed(&receive, 8, 1, NULL, 6, "def", 3) == MQTT_RECEIVE_COMPLETE);
    assert(memcmp(receive.payload, "abcdef", 6) == 0);
    assert(mqtt_receive_feed(&receive, 8, 2, "down", 6, "abc", 3) == MQTT_RECEIVE_MORE);
    assert(mqtt_receive_feed(&receive, 8, 3, "down", 6, "def", 3) == MQTT_RECEIVE_INVALID);
    assert(mqtt_receive_feed(&receive, 8, 2, NULL, 6, "def", 3) == MQTT_RECEIVE_INVALID);
    assert(mqtt_receive_feed(&receive, 8, 4, "down", sizeof(receive.payload) + 1, "x", 1) == MQTT_RECEIVE_INVALID);
    assert(mqtt_receive_feed(&receive, 8, 4, "down", 4, "abc", 3) == MQTT_RECEIVE_MORE);
    assert(mqtt_receive_feed(&receive, 8, 4, NULL, 4, "de", 2) == MQTT_RECEIVE_INVALID);
    mqtt_receive_reset(&receive, 9);
    assert(mqtt_receive_feed(&receive, 8, 4, "down", 3, "old", 3) == MQTT_RECEIVE_STALE);
    assert(mqtt_receive_feed(&receive, 9, 5, "down", 3, "new", 3) == MQTT_RECEIVE_COMPLETE);
    puts("mqtt_receive: fragmented, stale, interleaved and oversized messages OK");
    return 0;
}
