#pragma once
/*-------------------------------------------define---------------------------------------------*/
/* 主机测试仅编译AES-CBC，不替代固件的TLS配置。 */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
/*-------------------------------------------function---------------------------------------------*/
