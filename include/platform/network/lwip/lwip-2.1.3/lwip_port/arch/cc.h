/**
 * @file cc.h
 * @brief 芯翼LWIP平台适配文件
 * @version 1.0
 * @date 2023-03-06
 * @copyright Copyright (c) 2023  芯翼信息科技有限公司
 * 
 */
#ifndef __CC_H__
#define __CC_H__

#include "cpu.h"
/* modify by cmiot1325 暂时修改通过编译 */
//#include "xy_system.h"

#define X8_F    "02x"
#define U16_F   "u"
#define S16_F   "d"
#define X16_F   "x"
#define U32_F   "u"
#define S32_F   "d"
#define X32_F   "x"
#define SZT_F   "llu"

#ifndef SSIZE_MAX
#define SSIZE_MAX  LONG_MAX
#endif

/** 采用GNU编译.stdio.h头文件可能会包含<sys/timeval>
 *  xtensa编译时，stdio.h不会包含timeval，需lwip提供
 *  因此需要根据不同编译环境区分
 * */
#if defined(__GNUC__) && !defined(__XTENSA__)
#define LWIP_TIMEVAL_PRIVATE 0
#include <sys/time.h>
#endif

/* define compiler specific symbols */
#if defined (__ICCARM__)

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_USE_INCLUDES

#elif defined (__CC_ARM)

#define PACK_STRUCT_BEGIN __packed
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#elif defined (__GNUC__)

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__ ((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#elif defined (__TASKING__)

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#endif
uint32_t xy_rand(void);
#define LWIP_RAND()  ((u32_t)xy_rand())

#if LWIP_DBG_PRINTF

#define LWIP_PLATFORM_ASSERT(x)                                   \
    do                                                            \
    {                                                             \
        xy_prints(XYAPP, WARN_LOG,"Assertion \"%s\" failed at line %d in %s\n", \
                    x, __LINE__, __FILE__);                       \
        xy_assert(0);                                             \
    } while (0)

#define LWIP_ERROR(message, expression, handler)                               \
    do                                                                         \
    {                                                                          \
        if (!(expression))                                                     \
        {                                                                      \
            xy_prints(XYAPP, WARN_LOG,"Assertion \"%s\" failed at line %d in %s\n", message, \
                        __LINE__, __FILE__);                                   \
            handler;                                                           \
        }                                                                      \
    } while (0)
    
#else

#define LWIP_PLATFORM_ASSERT(x)                                   \
    do                                                            \
    {                                                             \
        xy_assert(0);                                             \
    } while (0)

#define LWIP_ERROR(message, expression, handler)                               \
    do                                                                         \
    {                                                                          \
        if (!(expression))                                                     \
        {                                                                      \
            handler;                                                           \
        }                                                                      \
    } while (0)

#endif /* LWIP_DBG_PRINTF */

#endif /* __CC_H__ */
