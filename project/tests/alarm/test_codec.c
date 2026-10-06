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
* Function Name  : test_document_field_vectors
* Description    : 核对 V3.6 手报注册、心跳、报警、历史和单字节回执的固定向量
* Input          : 无
* Output         : 帧字节与独立 AES-CBC 参考密文断言
* Return         : 无
* Attention      : 文档 XX 使用虚构厂商值补齐；独立参考向量不代表平台实测
*******************************************************************************/
static void test_document_field_vectors(void)
{
    static const uint8_t commands[5] = {1, 2, 2, 12, 255};
    static const char *expected_frames[5] = {
        "57544B361234000101003704383639393735303334343431303832343630313133313138373433373332383938363131323032323430313433393837363225156410FA5C454E44",
        "57544B36123400020200150438363939373530333434343130383201251564102241454E44",
        "57544B3612340003020015043836393937353033343434313038320C25156410468F454E44",
        "57544B36123400040C001B0101043836393937353033343434313038320C2515641064224CE526E6454E44",
        "57544B3612340005FF000100F47C454E44"
    };
    static const char *expected_cipher[5] = {
        "67FFBC320A0934C6A3B4F62F86D9ECB43ED45AE2BAA706D308E0FED10ABEC1326EAAF09423AD1F1F6E809F71E46D37072986456568EF01C792551B82F9B3EAF5A894DB38D1843FD09B9D83382A8DB42C99ECF12A38A5DC1A93DE30994CA4C5B19A3FB42B80507AD39240839D9E239059FF0A6DBDE55AC9FC14B4A63A03D476DAD782C59EF1A9B507A662942396352149",
        "2D2CC3F9653644FB088572AEBB84FFF2F685B0C7DB46CFF0FC2756EC76BC69B4A54F6538026B00D82F301B4BE2E2507ED936193423A047EE21362A7773048CFA07025143077B53CB391DC9BCECF50DF6",
        "FEEAD7B4A29C3967EE81E1501326052DBEF1CF6F9DC1AB06C393D8AD7B97ADB8F92B096F4AE3D37F57F599AA78695323C14593E6415F81B9CF4A0724B330C9B2A1AAEAFF5925EAFA91DFD0CE9E4A7DD2",
        "F85691F64D9D2323EA055D434ED56CFAD8DE981DC9F7AAD83E06C0201BF057C4621519AF136C5617EF6001513A5C4B0749396622A59AB08179EFC72187B227A0C92C57F726C7118110DCAA19E8DDEBD6A77E62E4E1FFBA9278747174D3762213",
        "5E46EC82723D490732D8D215F831F30B0E593AE88B37A2BF1036994F40F66C90944A58661437EFB15FB06748490E81D4"
    };
    kaiwan_handset_identity_t identity = {
        "869975034441082", "460113118743732", "89861120224014398762", 0x10, false, 0};
    alarm_event_t event = {0};
    kaiwan_protocol_config_t config;
    uint8_t body[KAIWAN_HANDSET_REGISTER_BYTES];
    uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    uint8_t iv[16] = {0};
    char frame_hex[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE * 2 + 1];
    char json[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
    size_t frame_length;
    size_t text_length;
    unsigned index;
    int body_length;
    kaiwan_protocol_config_init(&config);
    config.manufacturer_id = 0x1234;
    memset(config.factory_code, 'T', 32);
    for (index = 0; index < 16; ++index)
    {
        config.aes_key[index] = (uint8_t)(index + 1);
    }
    event.valid = ALARM_TELEMETRY_VOLTAGE | ALARM_TELEMETRY_CSQ | ALARM_TELEMETRY_PERCENT | ALARM_TIME_UTC;
    event.battery_mv = 3700;
    event.csq = 21;
    event.battery_percent = 100;
    event.utc_seconds = 0x64224CE5;
    for (index = 0; index < 5; ++index)
    {
        event.event_type = index == 1 ? 1 : 12;
        if (index == 0)
        {
            body_length = kaiwan_handset_registration_payload(&identity, &event, body, sizeof(body));
        }
        else if (index == 4)
        {
            body[0] = 0;
            body_length = 1;
        }
        else
        {
            body_length = kaiwan_handset_event_payload(&identity, &event, index == 3, body, sizeof(body));
        }
        assert(body_length > 0);
        assert(kaiwan_protocol_build_frame(&config, (uint16_t)(index + 1), commands[index], body,
            (uint16_t)body_length, frame, sizeof(frame), &frame_length) == KAIWAN_OK);
        assert(kaiwan_protocol_hex_encode(frame, frame_length, frame_hex, sizeof(frame_hex), &text_length) == KAIWAN_OK);
        assert(strcmp(frame_hex, expected_frames[index]) == 0);
        assert(kaiwan_protocol_wrap_json(&config, &workspace, frame, frame_length, iv, json,
            sizeof(json), &text_length) == KAIWAN_OK);
        assert(strstr(json, expected_cipher[index]) != NULL);
    }
    /* 文档规定 factoryCode 为平台分配的字符串，未规定必须正好 32 字符。 */
    strcpy(config.factory_code, "TEST-VENDOR");
    assert(kaiwan_protocol_wrap_json(&config, &workspace, frame, frame_length, iv, json,
        sizeof(json), &text_length) == KAIWAN_OK);
    assert(strstr(json, "TEST-VENDOR") != NULL);
    config.factory_code[0] = 0;
    assert(kaiwan_protocol_wrap_json(&config, &workspace, frame, frame_length, iv, json,
        sizeof(json), &text_length) == KAIWAN_ERROR_ARGUMENT);
    memset(config.factory_code, 'T', sizeof(config.factory_code));
    assert(kaiwan_protocol_wrap_json(&config, &workspace, frame, frame_length, iv, json,
        sizeof(json), &text_length) == KAIWAN_ERROR_ARGUMENT);
}

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
    test_document_field_vectors();
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
