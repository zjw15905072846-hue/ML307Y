/*------------------------------------------includes--------------------------------------------*/
#include <stddef.h>
#include <stdint.h>
#include "product_build_config.h"

/*-------------------------------------------define---------------------------------------------*/
/* LittleFS LFS_ERR_NOENT is preserved by this SDK's xy_faccess implementation. */
#define PROJECT_FILE_MISSING 1
#define PROJECT_FILE_IO_ERROR (-2)

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/* Narrow private ABI: verified against the matching kernel library and link map. */
extern int xy_faccess(const char *path, int mode);
extern uint32_t xy_rand(void);

/*******************************************************************************
* Function Name  : project_base_identity
* Description    : 返回构建时生成的底包身份，防止应用使用不匹配接口表
* Input          : 无
* Output         : 无
* Return         : 静态身份字符串
* Attention      : 不释放返回地址；不包含产品账号
*******************************************************************************/
const char *project_base_identity(void)
{
    return PROJECT_BASE_ID;
}

/*******************************************************************************
* Function Name  : project_fs_probe
* Description    : 保留底层文件探测错误，区分缺失和介质读取故障
* Input          : path - 产品命名空间内的绝对或文件系统相对路径
* Output         : 无
* Return         : 0存在；1确实缺失；-2读错或参数错误
* Attention      : 不调用吞掉错误码的cm_fs_exist，不执行任何写操作
*******************************************************************************/
int project_fs_probe(const char *path)
{
    int result;
    if (!path || !path[0])
    {
        return PROJECT_FILE_IO_ERROR;
    }
    result = xy_faccess(path, 0);
    if (result == 0)
    {
        return 0;
    }
    return result == -2 ? PROJECT_FILE_MISSING : PROJECT_FILE_IO_ERROR;
}

/*******************************************************************************
* Function Name  : project_random_bytes
* Description    : 使用当前底包随机源生成每次报文独立的IV
* Input          : data - 输出缓冲；size - 所需字节数
* Output         : data - 随机字节
* Return         : 0成功；-1参数无效
* Attention      : 保持SDK随机源语义，不使用固定IV或时间戳替代
*******************************************************************************/
int project_random_bytes(uint8_t *data, size_t size)
{
    size_t offset;
    uint32_t value = 0;
    if (!data || size > 4096)
    {
        return -1;
    }
    for (offset = 0; offset < size; ++offset)
    {
        if ((offset & 3U) == 0)
        {
            value = xy_rand();
        }
        data[offset] = (uint8_t)(value >> ((offset & 3U) * 8U));
    }
    return 0;
}
