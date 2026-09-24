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
#define KAIWAN_PROTOCOL_VERSION_DEFAULT 0x36U /* 协议默认版本号 V3.6 */
#define KAIWAN_PROTOCOL_IMEI_LENGTH 15U          /* IMEI 固定数字长度 */
#define KAIWAN_PROTOCOL_IMSI_LENGTH 15U          /* IMSI 固定数字长度 */
#define KAIWAN_PROTOCOL_ICCID_LENGTH 20U         /* ICCID 固定数字长度 */
#define KAIWAN_PROTOCOL_FACTORY_CODE_LENGTH 32U  /* 平台 factoryCode 要求的固定长度 */
#define KAIWAN_PROTOCOL_AES_KEY_LENGTH 16U       /* AES-128 密钥长度 */
#define KAIWAN_PROTOCOL_AES_IV_LENGTH 16U        /* AES-CBC IV 长度 */
#define KAIWAN_PROTOCOL_FRAME_OVERHEAD 16U    /* 帧头、固定字段、CRC和END总长度 */
#define KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE 512U   /* 单个协议帧最大长度 */
#define KAIWAN_PROTOCOL_MAXIMUM_DATA_SIZE                                                                  \
    (KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE - KAIWAN_PROTOCOL_FRAME_OVERHEAD) /* 最大数据域 */
#define KAIWAN_PROTOCOL_MAXIMUM_AES_BYTES 1024U                       /* 加密工作区最大字节数 */
#define KAIWAN_PROTOCOL_MAXIMUM_CIPHER_HEX_CHARS (KAIWAN_PROTOCOL_MAXIMUM_AES_BYTES * 2U) /* HEX密文最大字符数 */
#define KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE 2200U /* MQTT JSON 最大长度 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 协议层统一返回值。 */
typedef enum
{
    KAIWAN_OK = 0,
    KAIWAN_ERROR_ARGUMENT = -1,
    KAIWAN_ERROR_CAPACITY = -2,
    KAIWAN_ERROR_FORMAT = -3,
    KAIWAN_ERROR_CRC = -4,
    KAIWAN_ERROR_VERSION = -5,
    KAIWAN_ERROR_MANUFACTURER = -6,
    KAIWAN_ERROR_CRYPTO = -7,
    KAIWAN_ERROR_JSON = -8,
    KAIWAN_ERROR_UNSUPPORTED = -9
} kaiwan_result_t;

/* CRC 在帧中的两个字节排列方式，可按平台实测切换。 */
typedef enum
{
    KAIWAN_CRC_BIG_ENDIAN = 0,
    KAIWAN_CRC_LITTLE_ENDIAN = 1
} kaiwan_crc_order_t;

/* AES 明文可按平台实际格式选择二进制帧或 ASCII 十六进制帧。 */
typedef enum
{
    KAIWAN_AES_PLAIN_BINARY_FRAME = 0,
    KAIWAN_AES_PLAIN_HEX_FRAME = 1
} kaiwan_aes_plain_mode_t;

/* 帧中的命令字。 */
typedef enum
{
    KAIWAN_COMMAND_REGISTER = 0x01,
    KAIWAN_COMMAND_EVENT = 0x02,
    KAIWAN_COMMAND_DOWNLINK = 0x03,
    KAIWAN_COMMAND_COMMAND_RESULT = 0x04,
    KAIWAN_COMMAND_DATA = 0x06,
    KAIWAN_COMMAND_TIME_SYNC = 0x0B,
    KAIWAN_COMMAND_HISTORY = 0x0C,
    KAIWAN_COMMAND_SERVER_RESPONSE = 0xFF
} kaiwan_command_t;

/* 注册时使用的设备类型编码。 */
typedef enum
{
    KAIWAN_DEVICE_DOOR_CONTACT = 0x01,
    KAIWAN_DEVICE_ALARM = 0x09,
    KAIWAN_DEVICE_IR_ALARM = 0x0B,
    KAIWAN_DEVICE_ELECTRIC_FENCE = 0x14
} kaiwan_device_type_t;

/* 事件上报的数据类型编码。 */
typedef enum
{
    KAIWAN_EVENT_HEARTBEAT = 0x01,
    KAIWAN_EVENT_ALARM = 0x02,
    KAIWAN_EVENT_ALARM_RECOVERY = 0x03,
    KAIWAN_EVENT_TAMPER = 0x04,
    KAIWAN_EVENT_TAMPER_RECOVERY = 0x05,
    KAIWAN_EVENT_LOW_VOLTAGE = 0x06,
    KAIWAN_EVENT_LOW_VOLTAGE_RECOVERY = 0x07,
    KAIWAN_EVENT_SENSOR_FAULT = 0x08,
    KAIWAN_EVENT_SENSOR_FAULT_RECOVERY = 0x09,
    KAIWAN_EVENT_TEST_ALARM = 0x0A,
    KAIWAN_EVENT_TEST_RECOVERY = 0x0B,
    KAIWAN_EVENT_EMERGENCY = 0x0C,
    KAIWAN_EVENT_EMERGENCY_RECOVERY = 0x0D,
    KAIWAN_EVENT_POWER_OFF = 0x27,
    KAIWAN_EVENT_POWER_RECOVERY = 0x28,
    KAIWAN_EVENT_ARM = 0x2A,
    KAIWAN_EVENT_DISARM = 0x2B,
    KAIWAN_EVENT_SELF_TEST = 0x2C,
    KAIWAN_EVENT_WARNING = 0x34,
    KAIWAN_EVENT_DATA_CHANGED = 0x47
} kaiwan_event_type_t;

/* 平台下发指令编码。 */
typedef enum
{
    KAIWAN_INSTRUCTION_RELAY_STATE = 0x62
} kaiwan_instruction_t;

/* 单台设备的协议参数；密钥和厂商信息由端口配置层填写。 */
typedef struct
{
    uint8_t protocol_version;                       /* 帧版本，解析时也用于校验。 */
    uint16_t manufacturer_id;                       /* 平台分配的厂商编号。 */
    uint8_t aes_key[KAIWAN_PROTOCOL_AES_KEY_LENGTH];       /* AES-128 密钥，不以字符串结束。 */
    char factory_code[KAIWAN_PROTOCOL_FACTORY_CODE_LENGTH + 1U]; /* JSON 中的厂商标识。 */
    kaiwan_crc_order_t crc_order;                       /* 帧内 CRC 的字节顺序。 */
    kaiwan_aes_plain_mode_t aes_plain_mode;             /* 加密前的帧表示形式。 */
} kaiwan_protocol_config_t;

/* 工作区由 cloud task 独占，避免在任务栈上放置大数组。 */
typedef struct
{
    uint8_t plain[KAIWAN_PROTOCOL_MAXIMUM_AES_BYTES + 1U]; /* 解密或编码前的明文。 */
    uint8_t crypt[KAIWAN_PROTOCOL_MAXIMUM_AES_BYTES];       /* AES 输入/输出缓冲区。 */
    char cipher_hex[KAIWAN_PROTOCOL_MAXIMUM_CIPHER_HEX_CHARS + 1U]; /* JSON 使用的密文 HEX。 */
} kaiwan_protocol_workspace_t;

/* 解析后的只读帧视图；raw 和 data 指向调用方传入的帧，不能独立保存。 */
typedef struct
{
    const uint8_t *raw; /* 原始帧起点。 */
    size_t raw_length;     /* 原始帧字节数。 */
    uint8_t protocol_version;
    uint16_t manufacturer_id;
    uint16_t sequence;
    uint8_t command;
    const uint8_t *data; /* 数据域起点，借用 raw 所属缓冲区。 */
    uint16_t data_length;   /* 数据域字节数。 */
    uint16_t crc;
} kaiwan_frame_view_t;

/* 设备注册命令的数据域。IMEI/IMSI/ICCID 需要以 '\0' 结束。 */
typedef struct
{
    uint8_t device_type; /* kaiwan_device_type_t 中的平台设备类型。 */
    char imei[KAIWAN_PROTOCOL_IMEI_LENGTH + 1U];
    char imsi[KAIWAN_PROTOCOL_IMSI_LENGTH + 1U];
    char iccid[KAIWAN_PROTOCOL_ICCID_LENGTH + 1U];
    uint8_t battery_voltage_0_1v; /* 0.1 V 为单位。 */
    uint8_t csq;                  /* 模组信号质量编码。 */
    uint8_t battery_percent;      /* 电量百分比。 */
    uint8_t firmware_version;     /* 平台协议字段中的固件版本。 */
} kaiwan_registration_t;

/* 门磁/门状态事件的数据域。 */
typedef struct
{
    char imei[KAIWAN_PROTOCOL_IMEI_LENGTH + 1U];
    uint8_t event_type;
    uint8_t door_state; /* 上报的门状态编码。 */
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
} kaiwan_door_event_t;

/* 通用报警设备事件的数据域。 */
typedef struct
{
    char imei[KAIWAN_PROTOCOL_IMEI_LENGTH + 1U];
    uint8_t event_type;
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
} kaiwan_alarm_event_t;

/* 红外报警设备事件的数据域。 */
typedef struct
{
    char imei[KAIWAN_PROTOCOL_IMEI_LENGTH + 1U];
    uint8_t event_type;
    uint8_t battery_voltage_0_1v;
    uint8_t csq;
    uint8_t battery_percent;
    uint8_t firmware_version;
    uint8_t armed_state; /* 当前布撤防状态。 */
} kaiwan_ir_event_t;

/* 下行指令执行结果的数据域。 */
typedef struct
{
    char imei[KAIWAN_PROTOCOL_IMEI_LENGTH + 1U];
    uint8_t instruction;
    uint8_t result;
} kaiwan_command_result_t;

/*-------------------------------------------function---------------------------------------------*/
/* 填入默认协议参数；密钥和厂商配置仍需由产品配置层提供。 */
void kaiwan_protocol_config_init(kaiwan_protocol_config_t *config);

/* 计算指定字节序列的 XMODEM CRC16。 */
uint16_t kaiwan_protocol_crc16_xmodem(const uint8_t *data, size_t data_length);

/* HEX 编解码：输出长度经 output_length 返回，容量不足返回 KAIWAN_ERROR_CAPACITY。 */
kaiwan_result_t kaiwan_protocol_hex_encode(const uint8_t *input, size_t input_length, char *output,
                                   size_t output_capacity, size_t *output_length);

kaiwan_result_t kaiwan_protocol_hex_decode(const char *input, size_t input_length, uint8_t *output,
                                   size_t output_capacity, size_t *output_length);

/* 将数据域和固定帧字段组装为二进制帧；output 由调用方持有。 */
kaiwan_result_t kaiwan_protocol_build_frame(const kaiwan_protocol_config_t *config, uint16_t sequence,
                                    uint8_t command, const uint8_t *data, uint16_t data_length,
                                    uint8_t *output, size_t output_capacity, size_t *output_length);

/* 校验帧格式、版本、厂商和 CRC；view 借用 frame，frame 应保持有效。 */
kaiwan_result_t kaiwan_protocol_parse_frame(const kaiwan_protocol_config_t *config, const uint8_t *frame,
                                    size_t frame_length, kaiwan_frame_view_t *view);

/* 使用给定 IV 加密完整帧并封装 JSON；workspace 不可并发复用。 */
kaiwan_result_t kaiwan_protocol_wrap_json(const kaiwan_protocol_config_t *config,
                                  kaiwan_protocol_workspace_t *workspace, const uint8_t *frame,
                                  size_t frame_length, const uint8_t iv[KAIWAN_PROTOCOL_AES_IV_LENGTH],
                                  char *output_json, size_t output_capacity, size_t *output_length);

/* 从 JSON 解密并还原帧；调用方需再调用 parse_frame 校验协议字段。 */
kaiwan_result_t kaiwan_protocol_unwrap_json(const kaiwan_protocol_config_t *config,
                                    kaiwan_protocol_workspace_t *workspace, const char *json,
                                    size_t json_length, uint8_t *output_frame, size_t output_capacity,
                                    size_t *output_length);

/* 将各类业务结构编码为帧数据域，结果长度经 output_length 返回。 */
kaiwan_result_t kaiwan_protocol_build_registration_data(const kaiwan_registration_t *registration,
                                                uint8_t *output, size_t output_capacity,
                                                uint16_t *output_length);

kaiwan_result_t kaiwan_protocol_build_door_event_data(const kaiwan_door_event_t *event, uint8_t *output,
                                              size_t output_capacity, uint16_t *output_length);

kaiwan_result_t kaiwan_protocol_build_alarm_event_data(const kaiwan_alarm_event_t *event, uint8_t *output,
                                               size_t output_capacity, uint16_t *output_length);

kaiwan_result_t kaiwan_protocol_build_ir_event_data(const kaiwan_ir_event_t *event, uint8_t *output,
                                            size_t output_capacity, uint16_t *output_length);

kaiwan_result_t kaiwan_protocol_build_command_result_data(const kaiwan_command_result_t *result,
                                                  uint8_t *output, size_t output_capacity,
                                                  uint16_t *output_length);

/* 解析继电器指令或平台应答；输出参数仅在返回 KAIWAN_OK 时有效。 */
kaiwan_result_t kaiwan_protocol_parse_relay_command(const kaiwan_frame_view_t *frame, uint8_t *relay_state);

kaiwan_result_t kaiwan_protocol_parse_server_response(const kaiwan_frame_view_t *frame, uint8_t *response_code);

/* 协议层自检返回 0 表示全部通过，非 0 表示存在失败项。 */
int kaiwan_protocol_run_self_tests(void);

#ifdef __cplusplus
}
#endif
