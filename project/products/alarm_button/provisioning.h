#pragma once
/*------------------------------------------includes--------------------------------------------*/
/* 可在构建时指定项目内私有头文件，使用宏覆盖下列默认值；不记录密钥到日志。 */
#ifdef AB_PROVISION_HEADER
#include AB_PROVISION_HEADER
#endif
/*-------------------------------------------define---------------------------------------------*/
#ifndef AB_CLOUD_ENABLED
#define AB_CLOUD_ENABLED 0
#endif
#ifndef AB_BROKER_HOST
#define AB_BROKER_HOST ""
#endif
#ifndef AB_BROKER_PORT
#define AB_BROKER_PORT 1883
#endif
#ifndef AB_MQTT_USERNAME
#define AB_MQTT_USERNAME ""
#endif
#ifndef AB_MQTT_PASSWORD
#define AB_MQTT_PASSWORD ""
#endif
#ifndef AB_FACTORY_CODE
#define AB_FACTORY_CODE ""
#endif
#ifndef AB_MANUFACTURER_ID
#define AB_MANUFACTURER_ID 0
#endif
#ifndef AB_AES_KEY_BYTES
#define AB_AES_KEY_BYTES {0}
#endif
#ifndef AB_PROTOCOL_VERIFIED
#define AB_PROTOCOL_VERIFIED 0
#endif
#ifndef AB_UNKNOWN_TELEMETRY_VERIFIED
#define AB_UNKNOWN_TELEMETRY_VERIFIED 0
#endif
#ifndef AB_UNKNOWN_TELEMETRY_BYTE
#define AB_UNKNOWN_TELEMETRY_BYTE 0xff
#endif
#ifndef AB_AES_HEX_PLAINTEXT
#define AB_AES_HEX_PLAINTEXT 1
#endif
#ifndef AB_CRC_LITTLE_ENDIAN
#define AB_CRC_LITTLE_ENDIAN 0
#endif
/* 0=恢复记录等待平台约定；1=历史帧(须有发生UTC)；2=平台已确认允许实时重投。 */
#ifndef AB_REPLAY_MODE
#define AB_REPLAY_MODE 0
#endif
#ifndef AB_USE_TLS
#define AB_USE_TLS 0
#endif
/* TLS端口对象按SDK类型在私有头文件提供，不允许只开TLS却传空配置。 */
#ifndef AB_TLS_CONFIG
#define AB_TLS_CONFIG NULL
#endif
#ifndef AB_FIRMWARE_VERSION
#define AB_FIRMWARE_VERSION 0x10
#endif
#if AB_REPLAY_MODE < 0 || AB_REPLAY_MODE > 2
#error "AB_REPLAY_MODE must be 0, 1 or 2"
#endif
#if AB_BROKER_PORT < 1 || AB_BROKER_PORT > 65535
#error "Invalid MQTT broker port"
#endif
#ifndef AB_UTC_VERIFIED
#define AB_UTC_VERIFIED 0
#endif
/* UTC is marked valid only after the clock source and timezone have been verified. */
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
