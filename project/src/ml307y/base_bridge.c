/*------------------------------------------includes--------------------------------------------*/
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ml307y/ml307y_port.h"
#include "cm_mqtt.h"
#include "lwip/sockets.h"
#include "product_build_config.h"

/*-------------------------------------------define---------------------------------------------*/
/* 当前 SDK 的 xy_faccess 保留 LittleFS 的 LFS_ERR_NOENT，便于区分缺失和读错。 */
#define PROJECT_FILE_MISSING 1    /* 路径确实不存在。 */
#define PROJECT_FILE_IO_ERROR (-2) /* 文件系统错误或参数错误。 */
/* 本 SDK RV64 MQTT 库的 DWARF 和 socket 创建指令均确认 net.socket 位于 92。 */
/* 更换厂家 MQTT 库时必须重新核验；地址查询只在成功 CONNACK 回调中调用。 */
#define PROJECT_MQTT_SOCKET_OFFSET 92U
#define PROJECT_MQTT_ADDRESS_CAPACITY 46U /* 含终止符的完整 IPv6 文本容量。 */

/*-------------------------------------------typedef---------------------------------------------*/
typedef union
{
    struct sockaddr_storage storage; /* 提供两种地址族的完整输出容量。 */
    struct sockaddr_in ipv4; /* SDK IPv4 本地端点。 */
    struct sockaddr_in6 ipv6; /* SDK IPv6 本地端点。 */
} project_socket_address_t;

_Static_assert(sizeof(void *) == 8 && sizeof(int) == 4, "MQTT socket ABI requires this SDK RV64 build");
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/* 仅声明当前底包已核验的私有 ABI，接口来源对应内核库和链接映射。 */
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
    /* 只把底层明确的 NOENT 当作空白，其他错误阻止初始化覆盖。 */
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
    /* 每个 32 位随机数按低字节到高字节填充，不重复使用固定 IV。 */
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

/*******************************************************************************
* Function Name  : project_mqtt_local_address
* Description    : 查询当前 MQTT socket 实际使用的本地源地址与 IP 版本
* Input          : client - SDK 客户端；address/capacity - 输出缓冲及容量
* Output         : address - IP 文本；失败时为空，绝不返回上一次地址
* Return         : 4 为 IPv4；6 为 IPv6；-1 参数；-2 断开；-3 查询；-4 地址；-5 转换
* Attention      : 仅在成功 CONNACK 回调中读取；SDK 同一循环任务持有 socket 生命周期
*******************************************************************************/
int project_mqtt_local_address(void *client, char *address, size_t capacity)
{
    project_socket_address_t local;
    socklen_t length = sizeof(local);
    const unsigned char *source;
    int socket;
    int family;
    int version;
    unsigned index;
    bool nonzero = false;
    bool mapped = true;
    if (address && capacity)
    {
        address[0] = '\0';
    }
    if (!client || !address || capacity < PROJECT_MQTT_ADDRESS_CAPACITY)
    {
        return -1;
    }
    /* 公共接口先验证句柄和连接状态，再访问已核验的私有 socket 字段。 */
    if (cm_mqtt_client_get_state(client) != CM_MQTT_STATE_CONNECTED)
    {
        return -2;
    }
    memcpy(&socket, (const unsigned char *)client + PROJECT_MQTT_SOCKET_OFFSET, sizeof(socket));
    if (socket < 0)
    {
        return -2;
    }
    memset(&local, 0, sizeof(local));
    if (getsockname(socket, (struct sockaddr *)&local, &length) != 0)
    {
        return -3;
    }
    if (local.storage.ss_family == AF_INET && length >= sizeof(local.ipv4))
    {
        source = (const unsigned char *)&local.ipv4.sin_addr;
        family = AF_INET;
        version = 4;
        length = 4;
    }
    else if (local.storage.ss_family == AF_INET6 && length >= sizeof(local.ipv6))
    {
        source = (const unsigned char *)&local.ipv6.sin6_addr;
        family = AF_INET6;
        version = 6;
        length = 16;
        /* ::ffff:a.b.c.d 的实际 IP 通信为 IPv4，打印对应的四字节地址。 */
        for (index = 0; index < 10; ++index)
        {
            if (source[index] != 0)
            {
                mapped = false;
            }
        }
        if (mapped && source[10] == 0xff && source[11] == 0xff)
        {
            source += 12;
            family = AF_INET;
            version = 4;
            length = 4;
        }
    }
    else
    {
        return -4;
    }
    for (index = 0; index < length; ++index)
    {
        nonzero = nonzero || source[index] != 0;
    }
    if (!nonzero)
    {
        return -4;
    }
    if (!inet_ntop(family, source, address, PROJECT_MQTT_ADDRESS_CAPACITY))
    {
        address[0] = '\0';
        return -5;
    }
    address[PROJECT_MQTT_ADDRESS_CAPACITY - 1U] = '\0';
    return version;
}
