/*------------------------------------------includes--------------------------------------------*/
#include "mqtt_rx.h"
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
    mqtt_rx_t rx;
    mqtt_rx_reset(&rx, 8);
    assert(mqtt_rx_feed(&rx, 8, 1, "down", 6, "abc", 3) == MQTT_RX_MORE);
    assert(mqtt_rx_feed(&rx, 7, 1, NULL, 6, "bad", 3) == MQTT_RX_STALE);
    assert(mqtt_rx_feed(&rx, 8, 1, NULL, 6, "def", 3) == MQTT_RX_COMPLETE);
    assert(memcmp(rx.payload, "abcdef", 6) == 0);
    assert(mqtt_rx_feed(&rx, 8, 2, "down", 6, "abc", 3) == MQTT_RX_MORE);
    assert(mqtt_rx_feed(&rx, 8, 3, "down", 6, "def", 3) == MQTT_RX_INVALID);
    assert(mqtt_rx_feed(&rx, 8, 2, NULL, 6, "def", 3) == MQTT_RX_INVALID);
    assert(mqtt_rx_feed(&rx, 8, 4, "down", sizeof(rx.payload) + 1, "x", 1) == MQTT_RX_INVALID);
    assert(mqtt_rx_feed(&rx, 8, 4, "down", 4, "abc", 3) == MQTT_RX_MORE);
    assert(mqtt_rx_feed(&rx, 8, 4, NULL, 4, "de", 2) == MQTT_RX_INVALID);
    mqtt_rx_reset(&rx, 9);
    assert(mqtt_rx_feed(&rx, 8, 4, "down", 3, "old", 3) == MQTT_RX_STALE);
    assert(mqtt_rx_feed(&rx, 9, 5, "down", 3, "new", 3) == MQTT_RX_COMPLETE);
    puts("mqtt_rx: fragmented, stale, interleaved and oversized messages OK");
    return 0;
}
