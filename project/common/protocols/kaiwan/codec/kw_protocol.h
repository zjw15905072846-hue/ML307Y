#pragma once

/*
 * 铠湾无线终端协议公共接口。
 * 本文件定义帧格式、命令字、设备/事件类型，以及 AES+JSON 封装所需的数据结构。
 */

/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*-------------------------------------------define---------------------------------------------*/
#define KW_PROTOCOL_VERSION_DEFAULT 0x36U /* 协议默认版本号 V3.6 */
#define KW_PROTOCOL_IMEI_LEN 15U          /* IMEI 固定数字长度 */
#define KW_PROTOCOL_IMSI_LEN 15U          /* IMSI 固定数字长度 */
#define KW_PROTOCOL_ICCID_LEN 20U         /* ICCID 固定数字长度 */
#define KW_PROTOCOL_FACTORY_CODE_LEN 32U  /* 平台 factoryCode 最大长度 */
#define KW_PROTOCOL_AES_KEY_LEN 16U       /* AES-128 密钥长度 */
#define KW_PROTOCOL_AES_IV_LEN 16U        /* AES-CBC IV 长度 */
#define KW_PROTOCOL_FRAME_OVERHEAD 16U    /* 帧头、固定字段、CRC和END总长度 */
#define KW_PROTOCOL_MAX_FRAME_SIZE 512U   /* 单个协议帧最大长度 */
#define KW_PROTOCOL_MAX_DATA_SIZE                                                                  \
    (KW_PROTOCOL_MAX_FRAME_SIZE - KW_PROTOCOL_FRAME_OVERHEAD) /* 最大数据域 */
#define KW_PROTOCOL_MAX_AES_BYTES 1024U                       /* 加密工作区最大字节数 */
#define KW_PROTOCOL_MAX_CIPHER_HEX_CHARS (KW_PROTOCOL_MAX_AES_BYTES * 2U) /* HEX密文最大字符数 */
#define KW_PROTOCOL_MAX_JSON_SIZE 2200U /* MQTT JSON 最大长度 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 协议层统一返回值。 */
typedef enum
{
    KW_OK = 0,
    KW_ERR_ARGUMENT = -1,
    KW_ERR_CAPACITY = -2,
    KW_ERR_FORMAT = -3,
    KW_ERR_CRC = -4,
    KW_ERR_VERSION = -5,
    KW_ERR_MANUFACTURER = -6,
    KW_ERR_CRYPTO = -7,
    KW_ERR_JSON = -8,
    KW_ERR_UNSUPPORTED = -9
} kw_result_t;

/* CRC 在帧中的两个字节排列方式，可按平台实测切换。 */
typedef enum
{
    KW_CRC_BIG_ENDIAN = 0,
    KW_CRC_LITTLE_ENDIAN = 1
} kw_crc_order_t;

/* 协议没有明确 AES 明文是二进制帧还是 ASCII 十六进制帧，两种方式均保留。 */
/* 铠湾命令字。 */
typedef enum
{
    KW_AES_PLAIN_BINARY_FRAME = 0,
    KW_AES_PLAIN_HEX_FRAME = 1
} kw_aes_plain_mode_t;

/* 铠湾设备类型编码。 */
typedef enum
{
    KW_CMD_REGISTER = 0x01,
    KW_CMD_EVENT = 0x02,
    KW_CMD_DOWNLINK = 0x03,
    KW_CMD_COMMAND_RESULT = 0x04,
    KW_CMD_DATA = 0x06,
    KW_CMD_TIME_SYNC = 0x0B,
    KW_CMD_HISTORY = 0x0C,
    KW_CMD_SERVER_RESPONSE = 0xFF
} kw_command_t;

/* 铠湾事件类型编码。 */
typedef enum
{
    KW_DEVICE_DOOR_CONTACT = 0x01,
    KW_DEVICE_ALARM = 0x09,
    KW_DEVICE_IR_ALARM = 0x0B,
    KW_DEVICE_ELECTRIC_FENCE = 0x14
} kw_device_type_t;

/* 平台下发指令编码。 */
typedef enum
{
    KW_EVENT_HEARTBEAT = 0x01,
    KW_EVENT_ALARM = 0x02,
    KW_EVENT_ALARM_RECOVERY = 0x03,
    KW_EVENT_TAMPER = 0x04,
    KW_EVENT_TAMPER_RECOVERY = 0x05,
    KW_EVENT_LOW_VOLTAGE = 0x06,
    KW_EVENT_LOW_VOLTAGE_RECOVERY = 0x07,
    KW_EVENT_SENSOR_FAULT = 0x08,
    KW_EVENT_SENSOR_FAULT_RECOVERY = 0x09,
    KW_EVENT_TEST_ALARM = 0x0A,
    KW_EVENT_TEST_RECOVERY = 0x0B,
    KW_EVENT_EMERGENCY = 0x0C,
    KW_EVENT_EMERGENCY_RECOVERY = 0x0D,
    KW_EVENT_POWER_OFF = 0x27,
    KW_EVENT_POWER_RECOVERY = 0x28,
    KW_EVENT_ARM = 0x2A,
    KW_EVENT_DISARM = 0x2B,
    KW_EVENT_SELF_TEST = 0x2C,
    KW_EVENT_WARNING = 0x34,
    KW_EVENT_DATA_CHANGED = 0x47
} kw_event_type_t;

typedef enum
{
    KW_INSTRUCTION_RELAY_STATE = 0x62
} kw_instruction_t;

/* 单台设备的协议参数；密钥和厂商信息由端口配置层填写。 */
typedef struct
{
    uint8_t protocol_version;
    uint16_t manufacturer_id;
    uint8_t aes_key[KW_PROTOCOL_AES_KEY_LEN];
    char factory_code[KW_PROTOCOL_FACTORY_CODE_LEN + 1U];
    kw_crc_order_t crc_order;
    kw_aes_plain_mode_t aes_plain_mode;
} kw_protocol_config_t;

/* 工作区由 cloud task 独占，避免在任务栈上放置大数组。 */
/* 解析后的只读帧视图；其中指针引用调用方传入的原始帧。 */
typedef struct
{
    uint8_t plain[KW_PROTOCOL_MAX_AES_BYTES + 1U];
    uint8_t crypt[KW_PROTOCOL_MAX_AES_BYTES];
    char cipher_hex[KW_PROTOCOL_MAX_CIPHER_HEX_CHARS + 1U];
} kw_protocol_workspace_t;

/* 设备注册命令的数据域。 */
typedef struct
{
    const uint8_t *raw;
    size_t raw_len;
    uint8_t protocol_version;
    uint16_t manufacturer_id;
    uint16_t sequence;
    uint8_t command;
    const uint8_t *data;
    uint16_t data_len;
    uint16_t crc;
} kw_frame_view_t;

/* 门磁/门状态事件的数据域。 */
typedef struct
{
    uint8_t device_type;
    char imei[KW_PROTOCOL_IMEI_LEN + 1U];
    char imsi[KW_PROTOCOL_IMSI_LEN + 1U];
    char iccid[KW_PROTOCOL_ICCID_LEN + 1U];
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
} kw_registration_t;

/* 通用报警设备事件的数据域。 */
typedef struct
{
    char imei[KW_PROTOCOL_IMEI_LEN + 1U];
    uint8_t event_type;
    uint8_t door_state;
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
} kw_door_event_t;

/* 红外报警设备事件的数据域。 */
typedef struct
{
    char imei[KW_PROTOCOL_IMEI_LEN + 1U];
    uint8_t event_type;
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
} kw_alarm_event_t;

/* 下行指令执行结果的数据域。 */
typedef struct
{
    char imei[KW_PROTOCOL_IMEI_LEN + 1U];
    uint8_t event_type;
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
    uint8_t armed_state;
} kw_ir_event_t;

typedef struct
{
    char imei[KW_PROTOCOL_IMEI_LEN + 1U];
    uint8_t instruction;
    uint8_t result;
} kw_command_result_t;

/*-------------------------------------------function---------------------------------------------*/
void kw_protocol_config_init(kw_protocol_config_t *config);

uint16_t kw_protocol_crc16_xmodem(const uint8_t *data, size_t data_len);

kw_result_t kw_protocol_hex_encode(const uint8_t *input, size_t input_len, char *output,
                                   size_t output_capacity, size_t *output_len);

kw_result_t kw_protocol_hex_decode(const char *input, size_t input_len, uint8_t *output,
                                   size_t output_capacity, size_t *output_len);

kw_result_t kw_protocol_build_frame(const kw_protocol_config_t *config, uint16_t sequence,
                                    uint8_t command, const uint8_t *data, uint16_t data_len,
                                    uint8_t *output, size_t output_capacity, size_t *output_len);

kw_result_t kw_protocol_parse_frame(const kw_protocol_config_t *config, const uint8_t *frame,
                                    size_t frame_len, kw_frame_view_t *view);

kw_result_t kw_protocol_wrap_json(const kw_protocol_config_t *config,
                                  kw_protocol_workspace_t *workspace, const uint8_t *frame,
                                  size_t frame_len, const uint8_t iv[KW_PROTOCOL_AES_IV_LEN],
                                  char *output_json, size_t output_capacity, size_t *output_len);

kw_result_t kw_protocol_unwrap_json(const kw_protocol_config_t *config,
                                    kw_protocol_workspace_t *workspace, const char *json,
                                    size_t json_len, uint8_t *output_frame, size_t output_capacity,
                                    size_t *output_len);

kw_result_t kw_protocol_build_registration_data(const kw_registration_t *registration,
                                                uint8_t *output, size_t output_capacity,
                                                uint16_t *output_len);

kw_result_t kw_protocol_build_door_event_data(const kw_door_event_t *event, uint8_t *output,
                                              size_t output_capacity, uint16_t *output_len);

kw_result_t kw_protocol_build_alarm_event_data(const kw_alarm_event_t *event, uint8_t *output,
                                               size_t output_capacity, uint16_t *output_len);

kw_result_t kw_protocol_build_ir_event_data(const kw_ir_event_t *event, uint8_t *output,
                                            size_t output_capacity, uint16_t *output_len);

kw_result_t kw_protocol_build_command_result_data(const kw_command_result_t *result,
                                                  uint8_t *output, size_t output_capacity,
                                                  uint16_t *output_len);

kw_result_t kw_protocol_parse_relay_command(const kw_frame_view_t *frame, uint8_t *relay_state);

kw_result_t kw_protocol_parse_server_response(const kw_frame_view_t *frame, uint8_t *response_code);

int kw_protocol_run_self_tests(void);

#ifdef __cplusplus
}
#endif
