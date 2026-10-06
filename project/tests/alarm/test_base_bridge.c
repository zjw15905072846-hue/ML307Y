/*------------------------------------------includes--------------------------------------------*/
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/ml307y/base_bridge.c"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static unsigned char client_bytes[304];
static int client_state = CM_MQTT_STATE_CONNECTED;
static int socket_result;
static int address_family = AF_INET;
static socklen_t returned_length;
static bool conversion_failure;
static bool unspecified_address;
static bool mapped_address;
static unsigned socket_queries;
static unsigned conversions;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : cm_mqtt_client_get_state
* Description    : 验证客户端身份并提供当前连接状态
* Input          : client - 模拟客户端
* Output         : 无
* Return         : 客户端状态或无效句柄错误
* Attention      : 不访问真实 SDK 或网络
*******************************************************************************/
int cm_mqtt_client_get_state(cm_mqtt_client_t *client)
{
    return (void *)client == client_bytes ? client_state : -1;
}

/*******************************************************************************
* Function Name  : getsockname
* Description    : 按真实 lwIP 地址布局返回本地 socket 地址
* Input          : socket - 当前连接描述符；name/length - 输出缓冲
* Output         : name/length - 模拟实际连接的源地址
* Return         : socket_result
* Attention      : 断言只查询当前客户端的描述符，不遍历其他连接
*******************************************************************************/
int getsockname(int socket, struct sockaddr *name, socklen_t *length)
{
    static const unsigned char ipv4[4] = {10, 37, 151, 65};
    static const unsigned char ipv6[16] = {0x24, 0x0e, 0x08, 0x7c, 0x08, 0x87, 0xf8, 0xb5,
                                        0, 0, 0, 0, 0, 0, 0, 1};
    struct sockaddr_in *local4 = (struct sockaddr_in *)name;
    struct sockaddr_in6 *local6 = (struct sockaddr_in6 *)name;
    unsigned char *bytes;
    assert(socket == 7);
    assert(*length >= sizeof(struct sockaddr_in6));
    ++socket_queries;
    if (socket_result != 0)
    {
        return socket_result;
    }
    memset(name, 0, *length);
    name->sa_family = address_family;
    if (address_family == AF_INET)
    {
        if (!unspecified_address)
        {
            memcpy(&local4->sin_addr, ipv4, sizeof(ipv4));
        }
        *length = sizeof(*local4);
    }
    else
    {
        bytes = (unsigned char *)&local6->sin6_addr;
        if (!unspecified_address)
        {
            memcpy(bytes, ipv6, sizeof(ipv6));
        }
        if (mapped_address)
        {
            memset(bytes, 0, 16);
            bytes[10] = 0xff;
            bytes[11] = 0xff;
            memcpy(bytes + 12, ipv4, sizeof(ipv4));
        }
        *length = sizeof(*local6);
    }
    if (returned_length)
    {
        *length = returned_length;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : inet_ntop
* Description    : 核对转换的地址族和二进制源地址并返回文本
* Input          : family/source - 地址；destination/size - 输出
* Output         : destination - IP 文本
* Return         : 文本或 NULL
* Attention      : 验证桥接读取真实地址字段，格式化本身由 SDK 提供
*******************************************************************************/
const char *inet_ntop(int family, const void *source, char *destination, socklen_t size)
{
    const unsigned char *bytes = source;
    ++conversions;
    assert(size >= 46);
    if (conversion_failure)
    {
        strcpy(destination, "partial");
        return NULL;
    }
    if (family == AF_INET)
    {
        assert(bytes[0] == 10 && bytes[1] == 37 && bytes[2] == 151 && bytes[3] == 65);
        strcpy(destination, "10.37.151.65");
    }
    else
    {
        assert(family == AF_INET6 && bytes[0] == 0x24 && bytes[15] == 1);
        strcpy(destination, "240E:87C:887:F8B5::1");
    }
    return destination;
}

/*******************************************************************************
* Function Name  : main
* Description    : 覆盖本地地址读取、地址族以及失败输出清空
* Input          : 无
* Output         : 断言与测试结果
* Return         : 0 成功
* Attention      : 模拟 socket 查询不证明实机连接使用的 IP
*******************************************************************************/
int main(void)
{
    char address[46] = "old-address";
    int socket = 7;
    unsigned previous;
    memcpy(client_bytes + 92, &socket, sizeof(socket));
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == 4);
    assert(strcmp(address, "10.37.151.65") == 0);
    address_family = AF_INET6;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == 6);
    assert(strcmp(address, "240E:87C:887:F8B5::1") == 0);
    mapped_address = true;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == 4);
    assert(strcmp(address, "10.37.151.65") == 0);
    mapped_address = false;
    socket_result = -1;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -3);
    assert(!address[0]);
    socket_result = 0;
    conversion_failure = true;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -5);
    assert(!address[0]);
    conversion_failure = false;
    address_family = 99;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -4);
    assert(!address[0]);
    address_family = AF_INET6;
    returned_length = 2;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -4);
    returned_length = 0;
    unspecified_address = true;
    previous = conversions;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -4);
    address_family = AF_INET;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -4);
    assert(conversions == previous && !address[0]);
    unspecified_address = false;
    previous = socket_queries;
    client_state = CM_MQTT_STATE_DISCONNECTED;
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -2);
    client_state = CM_MQTT_STATE_CONNECTED;
    socket = -1;
    memcpy(client_bytes + 92, &socket, sizeof(socket));
    assert(project_mqtt_local_address(client_bytes, address, sizeof(address)) == -2);
    assert(project_mqtt_local_address(NULL, address, sizeof(address)) == -1);
    assert(project_mqtt_local_address((void *)1, address, sizeof(address)) == -2);
    assert(project_mqtt_local_address(client_bytes, NULL, 46) == -1);
    assert(project_mqtt_local_address(client_bytes, address, 0) == -1);
    strcpy(address, "old-address");
    assert(project_mqtt_local_address(client_bytes, address, 1) == -1);
    assert(!address[0] && socket_queries == previous);
    puts("MQTT source address: IPv4, IPv6, mapped IPv4 and unavailable cases passed");
    return 0;
}
