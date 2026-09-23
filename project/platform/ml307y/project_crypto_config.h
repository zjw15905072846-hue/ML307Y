#pragma once
/*-------------------------------------------define---------------------------------------------*/
/* Target-only configuration matching the shipped software AES context (288 bytes on RV64). */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_PLATFORM_C
