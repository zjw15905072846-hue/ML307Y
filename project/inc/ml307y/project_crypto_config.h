#pragma once
/*-------------------------------------------define---------------------------------------------*/
/* 仅用于当前 ML307Y 目标构建；配置须与底包的 mbedTLS AES 上下文 ABI 匹配。 */
/* 平台以软件 AES 和 CBC 模式处理协议负载，不由该头文件提供密钥。 */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_PLATFORM_C
