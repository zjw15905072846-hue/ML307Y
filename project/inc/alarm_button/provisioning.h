#pragma once
/*------------------------------------------includes--------------------------------------------*/
/* 可在构建时指定项目内私有头文件，使用宏覆盖下列默认值；不记录密钥到日志。 */
#ifdef ALARM_BUTTON_PROVISION_HEADER
#include ALARM_BUTTON_PROVISION_HEADER
#endif
/*-------------------------------------------define---------------------------------------------*/
/* 默认配置只供构建与隔离验证；未提供真实参数时不能启动云业务。 */
#ifndef ALARM_BUTTON_CLOUD_ENABLED
#define ALARM_BUTTON_CLOUD_ENABLED 0
#endif
#ifndef ALARM_BUTTON_CLOUD_DIAGNOSTICS
#define ALARM_BUTTON_CLOUD_DIAGNOSTICS 1 /* 输出连接、注册、心跳、报警及回执阶段，不打印凭据。 */
#endif
#ifndef ALARM_BUTTON_ENCRYPTION_ENABLED
#define ALARM_BUTTON_ENCRYPTION_ENABLED 1 /* 1=AES JSON；0=完整协议帧的十六进制文本。 */
#endif
#ifndef ALARM_BUTTON_PLAINTEXT_TRIAL
#define ALARM_BUTTON_PLAINTEXT_TRIAL 0 /* 明确允许明文联调，不等同于平台协议已验证。 */
#endif
#ifndef ALARM_BUTTON_UNKNOWN_TELEMETRY_TRIAL
#define ALARM_BUTTON_UNKNOWN_TELEMETRY_TRIAL 0 /* 允许联调未知遥测占位，不伪造有效采样。 */
#endif
#ifndef ALARM_BUTTON_PACKET_LOG_ENABLED
#define ALARM_BUTTON_PACKET_LOG_ENABLED 0 /* 输出实际 MQTT 上行报文；不输出登录密码或 AES 密钥。 */
#endif
#ifndef ALARM_BUTTON_UP_TOPIC_SUFFIX
#if ALARM_BUTTON_ENCRYPTION_ENABLED
#define ALARM_BUTTON_UP_TOPIC_SUFFIX "/sys/fire/aesdata/up"
#else
#define ALARM_BUTTON_UP_TOPIC_SUFFIX "/sys/fire/data/up"
#endif
#endif
#ifndef ALARM_BUTTON_DOWN_TOPIC_SUFFIX
#if ALARM_BUTTON_ENCRYPTION_ENABLED
#define ALARM_BUTTON_DOWN_TOPIC_SUFFIX "/sys/fire/aesdata/down"
#else
#define ALARM_BUTTON_DOWN_TOPIC_SUFFIX "/sys/fire/data/down"
#endif
#endif
#if ALARM_BUTTON_ENCRYPTION_ENABLED != 0 && ALARM_BUTTON_ENCRYPTION_ENABLED != 1
#error "Encryption mode must be 0 or 1"
#endif
#ifndef ALARM_BUTTON_BROKER_HOST
/* Broker、端口与账户由产品私有配置提供，不写入共用源码。 */
#define ALARM_BUTTON_BROKER_HOST ""
#endif
#ifndef ALARM_BUTTON_BROKER_PORT
#define ALARM_BUTTON_BROKER_PORT 1883
#endif
#ifndef ALARM_BUTTON_HEARTBEAT_HOURS
#define ALARM_BUTTON_HEARTBEAT_HOURS 12U /* 业务心跳间隔，与 MQTT 保活秒数分开。 */
#endif
#if ALARM_BUTTON_HEARTBEAT_HOURS < 1 || ALARM_BUTTON_HEARTBEAT_HOURS > 596
#error "Heartbeat hours must be between 1 and 596 for wrap-safe timing"
#endif
#ifndef ALARM_BUTTON_MQTT_USERNAME
#define ALARM_BUTTON_MQTT_USERNAME ""
#endif
#ifndef ALARM_BUTTON_MQTT_PASSWORD
#define ALARM_BUTTON_MQTT_PASSWORD ""
#endif
#ifndef ALARM_BUTTON_FACTORY_CODE
/* JSON 厂商密钥标识，由平台提供；当前本地容量最多 32 字符，不能照抄文档示例。 */
#define ALARM_BUTTON_FACTORY_CODE ""
#endif
#ifndef ALARM_BUTTON_MANUFACTURER_ID
#define ALARM_BUTTON_MANUFACTURER_ID 0
#endif
#ifndef ALARM_BUTTON_AES_KEY_BYTES
#define ALARM_BUTTON_AES_KEY_BYTES {0}
#endif
#ifndef ALARM_BUTTON_PROTOCOL_VERIFIED
/* 正式协议验证状态；明文联调通过独立开关允许尝试，不能冒充验证通过。 */
#define ALARM_BUTTON_PROTOCOL_VERIFIED 0
#endif
#ifndef ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED
/* 缺失电量或信号的占位字节须先经平台确认。 */
#define ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED 0
#endif
#ifndef ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE
#define ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE 0xff
#endif
#if ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE < 0 || ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE > 255
#error "Unknown telemetry placeholder must fit in one byte"
#endif
#if ALARM_BUTTON_MANUFACTURER_ID < 0 || ALARM_BUTTON_MANUFACTURER_ID > 65535
#error "Manufacturer ID must fit in two bytes; zero remains unprovisioned"
#endif
#ifndef ALARM_BUTTON_AES_HEX_PLAINTEXT
/* AES 明文形式与 CRC 字节序均须依平台实测协议设置。 */
#define ALARM_BUTTON_AES_HEX_PLAINTEXT 1
#endif
#ifndef ALARM_BUTTON_CRC_LITTLE_ENDIAN
#define ALARM_BUTTON_CRC_LITTLE_ENDIAN 0
#endif
/* 0=恢复记录等待平台约定；1=历史帧(须有发生UTC)；2=平台已确认允许实时重投。 */
#ifndef ALARM_BUTTON_REPLAY_MODE
#define ALARM_BUTTON_REPLAY_MODE 0
#endif
#ifndef ALARM_BUTTON_USE_TLS
/* TLS 开关与 SDK TLS 配置对象必须成对提供。 */
#define ALARM_BUTTON_USE_TLS 0
#endif
/* TLS端口对象按SDK类型在私有头文件提供，不允许只开TLS却传空配置。 */
#ifndef ALARM_BUTTON_TLS_CONFIG
#define ALARM_BUTTON_TLS_CONFIG NULL
#endif
#ifndef ALARM_BUTTON_FIRMWARE_VERSION
/* 上报字段的产品版本字节，修改前须核对平台定义。 */
#define ALARM_BUTTON_FIRMWARE_VERSION 0x10
#endif
#if ALARM_BUTTON_REPLAY_MODE < 0 || ALARM_BUTTON_REPLAY_MODE > 2
#error "ALARM_BUTTON_REPLAY_MODE must be 0, 1 or 2"
#endif
#if ALARM_BUTTON_BROKER_PORT < 1 || ALARM_BUTTON_BROKER_PORT > 65535
#error "Invalid MQTT broker port"
#endif
#ifndef ALARM_BUTTON_UTC_VERIFIED
/* 只有时钟来源及时区均已核验，才允许把 UTC 标为有效。 */
#define ALARM_BUTTON_UTC_VERIFIED 0
#endif
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
