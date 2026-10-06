#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "storage_interface.h"
/*-------------------------------------------define---------------------------------------------*/
#define SNAPSHOT_MAXIMUM_PAYLOAD 2048U /* 单个产品镜像的容量上限。 */
#define SNAPSHOT_HEADER_BYTES 32U /* 身份、版本、代数及 CRC 的固定头长度。 */
#define SNAPSHOT_BYTES (SNAPSHOT_HEADER_BYTES + SNAPSHOT_MAXIMUM_PAYLOAD)

/*-------------------------------------------typedef---------------------------------------------*/
/* 双文件槽的介质操作；slot 仅允许 0 或 1。 */
typedef struct
{
    void *user; /* 文件适配上下文，必须覆盖快照端口生命周期。 */
    /* 仅确认文件是否存在；缺失、读取故障必须返回不同状态。 */
    int (*probe)(void *user, unsigned slot);
    /* 成功时 actual 为真实字节数，供上层验证头、长度和 CRC。 */
    int (*read)(void *user, unsigned slot, void *data, size_t capacity, size_t *actual);
    /* 写入、同步并关闭均成功才返回 true。 */
    bool (*write_sync)(void *user, unsigned slot, const void *data, size_t size);
    /* 复用损坏槽前留存原文件；失败时禁止覆盖该槽。 */
    bool (*preserve)(void *user, unsigned slot);
} snapshot_file_interface_t;

/* 同一产品独占一个上下文，代数只在回读校验成功后推进。 */
typedef struct
{
    snapshot_file_interface_t files;
    uint32_t product_id; /* 阻止加载其他产品的镜像。 */
    uint64_t generation; /* 最近完整提交的代数。 */
    size_t payload_size; /* 当前产品镜像的固定字节数。 */
    int active; /* 最新完整槽；-1 表示尚无有效记录。 */
    int warning; /* 可恢复损坏槽的保留告警。 */
    unsigned corrupt_mask; /* 曾检测为损坏、复用前需留存的槽位。 */
    bool ready; /* 读取验证成功后才允许写入。 */
    bool has_confirmed_payload; /* 保留故障前已确认镜像，禁止恢复时覆盖未知提交。 */
    uint8_t confirmed_payload[SNAPSHOT_MAXIMUM_PAYLOAD];
    uint8_t record[SNAPSHOT_BYTES];
    uint8_t verify[SNAPSHOT_BYTES]; /* 新写入槽的回读校验缓冲。 */
} snapshot_store_t;

/*-------------------------------------------function---------------------------------------------*/
/* 绑定双槽接口；真正读取时先检查两槽，未知格式保持原样并禁止写入。 */
storage_interface_t snapshot_storage(snapshot_store_t *store, uint32_t product_id,
                              const snapshot_file_interface_t *files);
