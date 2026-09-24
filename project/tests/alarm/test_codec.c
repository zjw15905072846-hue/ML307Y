/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/handset_payload.h"
#include "kaiwan/kaiwan_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/*-------------------------------------------define---------------------------------------------*/
#undef assert
#define assert(x)                                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static kaiwan_protocol_workspace_t workspace;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : main
* Description    : 验证真实SDK编解码器的AES、CRC与手报数据体闭环
* Input          : 无
* Output         : 断言与测试日志
* Return         : 0 - 测试通过
* Attention      : 使用虚拟身份和密钥，不代表平台联调通过
*******************************************************************************/
int main(void)
{
    kaiwan_handset_identity_t id = {
        "123456789012345", "123456789012345", "12345678901234567890", 0x10, false, 0};
    alarm_event_t event = {0};
    kaiwan_protocol_config_t config;
    kaiwan_frame_view_t view;
    uint8_t body[KAIWAN_HANDSET_REGISTER_BYTES];
    uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    uint8_t decoded[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    uint8_t iv[16] = {0};
    char json[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
    size_t length;
    size_t json_length;
    size_t decoded_length;
    unsigned i;
    uint8_t response = 0;
    assert(kaiwan_protocol_run_self_tests() == 0);
    kaiwan_protocol_config_init(&config);
    config.protocol_version = 0x36;
    config.manufacturer_id = 0x1234;
    memset(config.factory_code, 'T', 32);
    for (i = 0; i < 16; i++)
    {
        config.aes_key[i] = (uint8_t)(i + 1);
    }
    event.valid = 7;
    event.battery_mv = 3900;
    event.battery_percent = 75;
    event.csq = 20;
    event.event_type = 0x0c;
    assert(kaiwan_handset_event_payload(&id, &event, false, body, sizeof(body)) == KAIWAN_HANDSET_EVENT_BYTES);
    assert(kaiwan_protocol_build_frame(&config, 7, KAIWAN_COMMAND_EVENT, body, KAIWAN_HANDSET_EVENT_BYTES, frame,
                                   sizeof(frame), &length) == KAIWAN_OK);
    assert(kaiwan_protocol_wrap_json(&config, &workspace, frame, length, iv, json, sizeof(json),
                                 &json_length) == KAIWAN_OK);
    assert(kaiwan_protocol_unwrap_json(&config, &workspace, json, json_length,
                                   iv /* deliberately small */, sizeof(iv),
                                   &decoded_length) != KAIWAN_OK);
    {
        int status = kaiwan_protocol_unwrap_json(&config, &workspace, json, json_length, decoded,
                                             sizeof(decoded), &decoded_length);
        if (status)
        {
            fprintf(stderr, "unwrap status=%d\n", status);
        }
        assert(status == KAIWAN_OK);
    }
    assert(decoded_length == length && memcmp(frame, decoded, length) == 0);
    assert(kaiwan_protocol_parse_frame(&config, decoded, decoded_length, &view) == KAIWAN_OK);
    assert(view.sequence == 7 && view.command == KAIWAN_COMMAND_EVENT && view.data[0] == 4 &&
           view.data[16] == 0x0c);
    decoded[12] ^= 1;
    assert(kaiwan_protocol_parse_frame(&config, decoded, decoded_length, &view) != KAIWAN_OK);
    assert(kaiwan_protocol_build_frame(&config, 7, KAIWAN_COMMAND_SERVER_RESPONSE, &response, 1, frame,
                                   sizeof(frame), &length) == KAIWAN_OK);
    assert(kaiwan_protocol_parse_frame(&config, frame, length, &view) == KAIWAN_OK);
    assert(kaiwan_protocol_parse_server_response(&view, &response) == KAIWAN_OK && response == 0);
    puts("codec: SDK AES/CRC roundtrip, handset identity and malformed-frame rejection passed");
    return 0;
}
