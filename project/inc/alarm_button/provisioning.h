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
#ifndef ALARM_BUTTON_BROKER_HOST
/* Broker、端口与账户由产品私有配置提供，不写入共用源码。 */
#define ALARM_BUTTON_BROKER_HOST ""
#endif
#ifndef ALARM_BUTTON_BROKER_PORT
#define ALARM_BUTTON_BROKER_PORT 1883
#endif
#ifndef ALARM_BUTTON_MQTT_USERNAME
#define ALARM_BUTTON_MQTT_USERNAME ""
#endif
#ifndef ALARM_BUTTON_MQTT_PASSWORD
#define ALARM_BUTTON_MQTT_PASSWORD ""
#endif
#ifndef ALARM_BUTTON_FACTORY_CODE
/* 厂商码、厂商 ID 和 AES 密钥必须与平台下发样例核对。 */
#define ALARM_BUTTON_FACTORY_CODE ""
#endif
#ifndef ALARM_BUTTON_MANUFACTURER_ID
#define ALARM_BUTTON_MANUFACTURER_ID 0
#endif
#ifndef ALARM_BUTTON_AES_KEY_BYTES
#define ALARM_BUTTON_AES_KEY_BYTES {0}
#endif
#ifndef ALARM_BUTTON_PROTOCOL_VERIFIED
/* 协议字段未经平台确认时保持关闭，不能用测试值投入实机。 */
#define ALARM_BUTTON_PROTOCOL_VERIFIED 0
#endif
#ifndef ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED
/* 缺失电量或信号的占位字节须先经平台确认。 */
#define ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED 0
#endif
#ifndef ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE
#define ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE 0xff
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
