/* 铠湾协议纯软件回归测试，不自动访问网络或Flash。 */
/*------------------------------------------includes--------------------------------------------*/
#include "kaiwan/kaiwan_protocol.h"

#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kaiwan_test_crc
* Description    : 验证标准字符串“123456789”的 CRC16-XMODEM 固定结果
* Input          : 无
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 校验标准向量 0x31C3
*******************************************************************************/
static int kaiwan_test_crc(void)
{
    static const uint8_t vector[] = "123456789";
    return kaiwan_protocol_crc16_xmodem(vector, sizeof(vector) - 1U) == 0x31C3U;
}

/*******************************************************************************
* Function Name  : kaiwan_test_frame_and_payloads
* Description    : 验证注册、门磁、报警、红外、命令结果以及完整帧的构建和解析
* Input          : 无
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 同时覆盖CRC错误路径
*******************************************************************************/
static int kaiwan_test_frame_and_payloads(void)
{
    kaiwan_protocol_config_t config;
    kaiwan_door_event_t door;
    kaiwan_alarm_event_t alarm;
    kaiwan_ir_event_t ir;
    kaiwan_frame_view_t view;
    uint8_t data[64];
    uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    uint8_t relay_data[2] = {KAIWAN_INSTRUCTION_RELAY_STATE, 1U};
    uint16_t data_length;
    size_t frame_length;
    uint8_t relay_state;

    kaiwan_protocol_config_init(&config);
    config.manufacturer_id = 0x1234U;

    memcpy(door.imei, "869975034441082", sizeof(door.imei));
    door.event_type = KAIWAN_EVENT_ALARM;
    door.door_state = 1U;
    door.battery_voltage_0_1v = 0x25U;
    door.csq = 21U;
    door.battery_percent = 100U;
    door.firmware_version = 0x10U;
    if ((kaiwan_protocol_build_door_event_data(&door, data, sizeof(data), &data_length) != KAIWAN_OK) ||
        (data_length != 22U) || (data[0] != KAIWAN_DEVICE_DOOR_CONTACT) || (data[16] != KAIWAN_EVENT_ALARM) ||
        (data[17] != 1U))
    {
        return 0;
    }

    if ((kaiwan_protocol_build_frame(&config, 1U, KAIWAN_COMMAND_EVENT, data, data_length, frame, sizeof(frame),
                                 &frame_length) != KAIWAN_OK) ||
        (kaiwan_protocol_parse_frame(&config, frame, frame_length, &view) != KAIWAN_OK) ||
        (view.sequence != 1U) || (view.command != KAIWAN_COMMAND_EVENT) || (view.data_length != 22U))
    {
        return 0;
    }

    frame[12] ^= 0x01U;
    if (kaiwan_protocol_parse_frame(&config, frame, frame_length, &view) != KAIWAN_ERROR_CRC)
    {
        return 0;
    }
    frame[12] ^= 0x01U;

    memcpy(alarm.imei, door.imei, sizeof(alarm.imei));
    alarm.event_type = KAIWAN_EVENT_EMERGENCY;
    alarm.battery_voltage_0_1v = 0xFFU;
    alarm.csq = 20U;
    alarm.battery_percent = 100U;
    alarm.firmware_version = 0x10U;
    if ((kaiwan_protocol_build_alarm_event_data(&alarm, data, sizeof(data), &data_length) != KAIWAN_OK) ||
        (data_length != 21U) || (data[0] != KAIWAN_DEVICE_ALARM) || (data[16] != KAIWAN_EVENT_EMERGENCY))
    {
        return 0;
    }

    memcpy(ir.imei, door.imei, sizeof(ir.imei));
    ir.event_type = KAIWAN_EVENT_HEARTBEAT;
    ir.battery_voltage_0_1v = 0x25U;
    ir.csq = 20U;
    ir.battery_percent = 100U;
    ir.firmware_version = 0x10U;
    ir.armed_state = 0U;
    if ((kaiwan_protocol_build_ir_event_data(&ir, data, sizeof(data), &data_length) != KAIWAN_OK) ||
        (data_length != 22U) || (data[0] != KAIWAN_DEVICE_IR_ALARM) || (data[21] != 0U))
    {
        return 0;
    }

    if ((kaiwan_protocol_build_frame(&config, 9U, KAIWAN_COMMAND_DOWNLINK, relay_data, sizeof(relay_data),
                                 frame, sizeof(frame), &frame_length) != KAIWAN_OK) ||
        (kaiwan_protocol_parse_frame(&config, frame, frame_length, &view) != KAIWAN_OK) ||
        (kaiwan_protocol_parse_relay_command(&view, &relay_state) != KAIWAN_OK) || (relay_state != 1U))
    {
        return 0;
    }

    return 1;
}

/*******************************************************************************
* Function Name  : kaiwan_test_invalid_inputs
* Description    : 覆盖短帧、空指针、奇数HEX和输出容量不足等错误入口
* Input          : 无
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 验证各类错误码返回正确
*******************************************************************************/
static int kaiwan_test_invalid_inputs(void)
{
    static kaiwan_protocol_workspace_t workspace;
    kaiwan_protocol_config_t config;
    kaiwan_frame_view_t view;
    uint8_t decoded[4];
    uint8_t tiny_frame[1] = {0U};
    uint8_t iv[KAIWAN_PROTOCOL_AES_IV_LENGTH] = {0U};
    uint8_t relay_state;
    uint8_t response_code;
    size_t decoded_length;
    size_t json_length;
    char json[128];

    if (kaiwan_protocol_hex_decode("ABC", 3U, decoded, sizeof(decoded), &decoded_length) != KAIWAN_ERROR_FORMAT)
    {
        return 0;
    }

    kaiwan_protocol_config_init(&config);
    config.manufacturer_id = 0x1234U;
    memcpy(config.factory_code, "0123456789ABCDEF0123456789ABCDEF", 33U);
    if (kaiwan_protocol_wrap_json(&config, &workspace, tiny_frame, sizeof(tiny_frame), iv, json,
                              sizeof(json), &json_length) != KAIWAN_ERROR_FORMAT)
    {
        return 0;
    }

    memset(&view, 0, sizeof(view));
    view.command = KAIWAN_COMMAND_DOWNLINK;
    view.data_length = 2U;
    if (kaiwan_protocol_parse_relay_command(&view, &relay_state) != KAIWAN_ERROR_FORMAT)
    {
        return 0;
    }
    view.command = KAIWAN_COMMAND_SERVER_RESPONSE;
    view.data_length = 1U;
    if (kaiwan_protocol_parse_server_response(&view, &response_code) != KAIWAN_ERROR_FORMAT)
    {
        return 0;
    }
    return 1;
}

/*******************************************************************************
* Function Name  : kaiwan_test_json_roundtrip
* Description    : 分别验证二进制明文和HEX明文两种 AES-CBC JSON 往返
* Input          : mode - AES明文模式
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 同时覆盖发送端拒绝CRC损坏帧
*******************************************************************************/
static int kaiwan_test_json_roundtrip(kaiwan_aes_plain_mode_t mode)
{
    kaiwan_protocol_config_t config;
    static kaiwan_protocol_workspace_t workspace;
    kaiwan_frame_view_t view;
    static uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    static uint8_t decoded[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    static uint8_t exact_frame[KAIWAN_PROTOCOL_FRAME_OVERHEAD + 2U];
    uint8_t data[2] = {KAIWAN_INSTRUCTION_RELAY_STATE, 1U};
    uint8_t iv[KAIWAN_PROTOCOL_AES_IV_LENGTH];
    static char json[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
    size_t frame_length;
    size_t decoded_length;
    size_t json_length;
    size_t index;

    kaiwan_protocol_config_init(&config);
    config.manufacturer_id = 0x4567U;
    config.aes_plain_mode = mode;
    memcpy(config.factory_code, "0123456789ABCDEF0123456789ABCDEF", 33U);
    for (index = 0U; index < KAIWAN_PROTOCOL_AES_KEY_LENGTH; ++index)
    {
        config.aes_key[index] = (uint8_t)index;
        iv[index] = (uint8_t)(0xF0U + index);
    }

    if ((kaiwan_protocol_build_frame(&config, 0x0102U, KAIWAN_COMMAND_DOWNLINK, data, sizeof(data), frame,
                                 sizeof(frame), &frame_length) != KAIWAN_OK) ||
        (kaiwan_protocol_wrap_json(&config, &workspace, frame, frame_length, iv, json, sizeof(json),
                               &json_length) != KAIWAN_OK) ||
        /* 回归：调用者只提供真实帧长度时也必须能解密，不能要求容纳填充。 */
        (kaiwan_protocol_unwrap_json(&config, &workspace, json, json_length, exact_frame, frame_length,
                                 &decoded_length) != KAIWAN_OK) ||
        (decoded_length != frame_length) || (memcmp(exact_frame, frame, frame_length) != 0) ||
        (kaiwan_protocol_unwrap_json(&config, &workspace, json, json_length, decoded, sizeof(decoded),
                                 &decoded_length) != KAIWAN_OK) ||
        (decoded_length != frame_length) || (memcmp(decoded, frame, frame_length) != 0) ||
        (kaiwan_protocol_parse_frame(&config, decoded, decoded_length, &view) != KAIWAN_OK))
    {
        return 0;
    }

    /* 发送端也拒绝 CRC 已损坏的帧，避免加密无效数据。 */
    frame[11] ^= 0x01U;
    if (kaiwan_protocol_wrap_json(&config, &workspace, frame, frame_length, iv, json, sizeof(json),
                              &json_length) != KAIWAN_ERROR_CRC)
    {
        return 0;
    }
    frame[11] ^= 0x01U;
    return 1;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_run_self_tests
* Description    : 汇总全部协议自测
* Input          : 无
* Output         : 无
* Return         : 0 - 全部通过；正数 - 失败用例数
* Attention      : 由产测或调试入口显式调用，不会自动运行
*******************************************************************************/
int kaiwan_protocol_run_self_tests(void)
{
    int failures = 0;

    if (!kaiwan_test_crc())
    {
        ++failures;
    }
    if (!kaiwan_test_frame_and_payloads())
    {
        ++failures;
    }
    if (!kaiwan_test_invalid_inputs())
    {
        ++failures;
    }
    if (!kaiwan_test_json_roundtrip(KAIWAN_AES_PLAIN_HEX_FRAME))
    {
        ++failures;
    }
    if (!kaiwan_test_json_roundtrip(KAIWAN_AES_PLAIN_BINARY_FRAME))
    {
        ++failures;
    }
    return failures;
}
