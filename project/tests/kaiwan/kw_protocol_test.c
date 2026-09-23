/* 铠湾协议纯软件回归测试，不自动访问网络或Flash。 */
/*------------------------------------------includes--------------------------------------------*/
#include "kw_protocol.h"

#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kw_test_crc
* Description    : 验证标准字符串“123456789”的 CRC16-XMODEM 固定结果
* Input          : 无
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 校验标准向量 0x31C3
*******************************************************************************/
static int kw_test_crc(void)
{
    static const uint8_t vector[] = "123456789";
    return kw_protocol_crc16_xmodem(vector, sizeof(vector) - 1U) == 0x31C3U;
}

/*******************************************************************************
* Function Name  : kw_test_frame_and_payloads
* Description    : 验证注册、门磁、报警、红外、命令结果以及完整帧的构建和解析
* Input          : 无
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 同时覆盖CRC错误路径
*******************************************************************************/
static int kw_test_frame_and_payloads(void)
{
    kw_protocol_config_t config;
    kw_door_event_t door;
    kw_alarm_event_t alarm;
    kw_ir_event_t ir;
    kw_frame_view_t view;
    uint8_t data[64];
    uint8_t frame[KW_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t relay_data[2] = {KW_INSTRUCTION_RELAY_STATE, 1U};
    uint16_t data_len;
    size_t frame_len;
    uint8_t relay_state;

    kw_protocol_config_init(&config);
    config.manufacturer_id = 0x1234U;

    memcpy(door.imei, "869975034441082", sizeof(door.imei));
    door.event_type = KW_EVENT_ALARM;
    door.door_state = 1U;
    door.battery_voltage_0_1v = 0x25U;
    door.csq = 21U;
    door.battery_percent = 100U;
    door.firmware_version = 0x10U;
    if ((kw_protocol_build_door_event_data(&door, data, sizeof(data), &data_len) != KW_OK) ||
        (data_len != 22U) || (data[0] != KW_DEVICE_DOOR_CONTACT) || (data[16] != KW_EVENT_ALARM) ||
        (data[17] != 1U))
    {
        return 0;
    }

    if ((kw_protocol_build_frame(&config, 1U, KW_CMD_EVENT, data, data_len, frame, sizeof(frame),
                                 &frame_len) != KW_OK) ||
        (kw_protocol_parse_frame(&config, frame, frame_len, &view) != KW_OK) ||
        (view.sequence != 1U) || (view.command != KW_CMD_EVENT) || (view.data_len != 22U))
    {
        return 0;
    }

    frame[12] ^= 0x01U;
    if (kw_protocol_parse_frame(&config, frame, frame_len, &view) != KW_ERR_CRC)
    {
        return 0;
    }
    frame[12] ^= 0x01U;

    memcpy(alarm.imei, door.imei, sizeof(alarm.imei));
    alarm.event_type = KW_EVENT_EMERGENCY;
    alarm.battery_voltage_0_1v = 0xFFU;
    alarm.csq = 20U;
    alarm.battery_percent = 100U;
    alarm.firmware_version = 0x10U;
    if ((kw_protocol_build_alarm_event_data(&alarm, data, sizeof(data), &data_len) != KW_OK) ||
        (data_len != 21U) || (data[0] != KW_DEVICE_ALARM) || (data[16] != KW_EVENT_EMERGENCY))
    {
        return 0;
    }

    memcpy(ir.imei, door.imei, sizeof(ir.imei));
    ir.event_type = KW_EVENT_HEARTBEAT;
    ir.battery_voltage_0_1v = 0x25U;
    ir.csq = 20U;
    ir.battery_percent = 100U;
    ir.firmware_version = 0x10U;
    ir.armed_state = 0U;
    if ((kw_protocol_build_ir_event_data(&ir, data, sizeof(data), &data_len) != KW_OK) ||
        (data_len != 22U) || (data[0] != KW_DEVICE_IR_ALARM) || (data[21] != 0U))
    {
        return 0;
    }

    if ((kw_protocol_build_frame(&config, 9U, KW_CMD_DOWNLINK, relay_data, sizeof(relay_data),
                                 frame, sizeof(frame), &frame_len) != KW_OK) ||
        (kw_protocol_parse_frame(&config, frame, frame_len, &view) != KW_OK) ||
        (kw_protocol_parse_relay_command(&view, &relay_state) != KW_OK) || (relay_state != 1U))
    {
        return 0;
    }

    return 1;
}

/*******************************************************************************
* Function Name  : kw_test_invalid_inputs
* Description    : 覆盖短帧、空指针、奇数HEX和输出容量不足等错误入口
* Input          : 无
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 验证各类错误码返回正确
*******************************************************************************/
static int kw_test_invalid_inputs(void)
{
    static kw_protocol_workspace_t workspace;
    kw_protocol_config_t config;
    kw_frame_view_t view;
    uint8_t decoded[4];
    uint8_t tiny_frame[1] = {0U};
    uint8_t iv[KW_PROTOCOL_AES_IV_LEN] = {0U};
    uint8_t relay_state;
    uint8_t response_code;
    size_t decoded_len;
    size_t json_len;
    char json[128];

    if (kw_protocol_hex_decode("ABC", 3U, decoded, sizeof(decoded), &decoded_len) != KW_ERR_FORMAT)
    {
        return 0;
    }

    kw_protocol_config_init(&config);
    config.manufacturer_id = 0x1234U;
    memcpy(config.factory_code, "0123456789ABCDEF0123456789ABCDEF", 33U);
    if (kw_protocol_wrap_json(&config, &workspace, tiny_frame, sizeof(tiny_frame), iv, json,
                              sizeof(json), &json_len) != KW_ERR_FORMAT)
    {
        return 0;
    }

    memset(&view, 0, sizeof(view));
    view.command = KW_CMD_DOWNLINK;
    view.data_len = 2U;
    if (kw_protocol_parse_relay_command(&view, &relay_state) != KW_ERR_FORMAT)
    {
        return 0;
    }
    view.command = KW_CMD_SERVER_RESPONSE;
    view.data_len = 1U;
    if (kw_protocol_parse_server_response(&view, &response_code) != KW_ERR_FORMAT)
    {
        return 0;
    }
    return 1;
}

/*******************************************************************************
* Function Name  : kw_test_json_roundtrip
* Description    : 分别验证二进制明文和HEX明文两种 AES-CBC JSON 往返
* Input          : mode - AES明文模式
* Output         : 无
* Return         : 1 - 通过；0 - 失败
* Attention      : 同时覆盖发送端拒绝CRC损坏帧
*******************************************************************************/
static int kw_test_json_roundtrip(kw_aes_plain_mode_t mode)
{
    kw_protocol_config_t config;
    static kw_protocol_workspace_t workspace;
    kw_frame_view_t view;
    static uint8_t frame[KW_PROTOCOL_MAX_FRAME_SIZE];
    static uint8_t decoded[KW_PROTOCOL_MAX_FRAME_SIZE];
    static uint8_t exact_frame[KW_PROTOCOL_FRAME_OVERHEAD + 2U];
    uint8_t data[2] = {KW_INSTRUCTION_RELAY_STATE, 1U};
    uint8_t iv[KW_PROTOCOL_AES_IV_LEN];
    static char json[KW_PROTOCOL_MAX_JSON_SIZE];
    size_t frame_len;
    size_t decoded_len;
    size_t json_len;
    size_t index;

    kw_protocol_config_init(&config);
    config.manufacturer_id = 0x4567U;
    config.aes_plain_mode = mode;
    memcpy(config.factory_code, "0123456789ABCDEF0123456789ABCDEF", 33U);
    for (index = 0U; index < KW_PROTOCOL_AES_KEY_LEN; ++index)
    {
        config.aes_key[index] = (uint8_t)index;
        iv[index] = (uint8_t)(0xF0U + index);
    }

    if ((kw_protocol_build_frame(&config, 0x0102U, KW_CMD_DOWNLINK, data, sizeof(data), frame,
                                 sizeof(frame), &frame_len) != KW_OK) ||
        (kw_protocol_wrap_json(&config, &workspace, frame, frame_len, iv, json, sizeof(json),
                               &json_len) != KW_OK) ||
        /* 回归：调用者只提供真实帧长度时也必须能解密，不能要求容纳填充。 */
        (kw_protocol_unwrap_json(&config, &workspace, json, json_len, exact_frame, frame_len,
                                 &decoded_len) != KW_OK) ||
        (decoded_len != frame_len) || (memcmp(exact_frame, frame, frame_len) != 0) ||
        (kw_protocol_unwrap_json(&config, &workspace, json, json_len, decoded, sizeof(decoded),
                                 &decoded_len) != KW_OK) ||
        (decoded_len != frame_len) || (memcmp(decoded, frame, frame_len) != 0) ||
        (kw_protocol_parse_frame(&config, decoded, decoded_len, &view) != KW_OK))
    {
        return 0;
    }

    /* 发送端也拒绝 CRC 已损坏的帧，避免加密无效数据。 */
    frame[11] ^= 0x01U;
    if (kw_protocol_wrap_json(&config, &workspace, frame, frame_len, iv, json, sizeof(json),
                              &json_len) != KW_ERR_CRC)
    {
        return 0;
    }
    frame[11] ^= 0x01U;
    return 1;
}

/*******************************************************************************
* Function Name  : kw_protocol_run_self_tests
* Description    : 汇总全部协议自测
* Input          : 无
* Output         : 无
* Return         : 0 - 全部通过；正数 - 失败用例数
* Attention      : 由产测或调试入口显式调用，不会自动运行
*******************************************************************************/
int kw_protocol_run_self_tests(void)
{
    int failures = 0;

    if (!kw_test_crc())
    {
        ++failures;
    }
    if (!kw_test_frame_and_payloads())
    {
        ++failures;
    }
    if (!kw_test_invalid_inputs())
    {
        ++failures;
    }
    if (!kw_test_json_roundtrip(KW_AES_PLAIN_HEX_FRAME))
    {
        ++failures;
    }
    if (!kw_test_json_roundtrip(KW_AES_PLAIN_BINARY_FRAME))
    {
        ++failures;
    }
    return failures;
}
