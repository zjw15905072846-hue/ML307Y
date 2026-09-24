#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*-------------------------------------------define---------------------------------------------*/
/* 介质探测必须区分真正空白、I/O 故障、外来格式和损坏内容。 */
#define STORAGE_OK 0
#define STORAGE_EMPTY 1
#define STORAGE_IO_ERROR (-2)
#define STORAGE_FOREIGN (-4)
#define STORAGE_CORRUPT (-5)
#define STORAGE_NOT_READY (-9)

/*-------------------------------------------typedef---------------------------------------------*/
/* 产品镜像端口；读取未验证为空白前不得初始化或覆盖介质。 */
typedef struct
{
    void *user; /* 端口实例上下文，由创建方管理生命周期。 */
    /* 读出指定大小的完整镜像；返回明确状态码，不以零长度表示空白。 */
    int (*read)(void *user, void *data, size_t size);
    /* 完整持久提交成功才返回 true；失败时保留旧完整镜像。 */
    bool (*write)(void *user, const void *data, size_t size);
    /* 返回恢复时留下的告警，不改变镜像或清除故障记录。 */
    int (*warning)(void *user);
} storage_interface_t;

/*-------------------------------------------function---------------------------------------------*/
