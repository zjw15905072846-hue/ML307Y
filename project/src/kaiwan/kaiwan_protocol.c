/* 铠湾协议帧、CRC16、AES-CBC和MQTT JSON编解码实现。 */
/*------------------------------------------includes--------------------------------------------*/
#include "kaiwan/kaiwan_protocol.h"

#include <limits.h>
#include <string.h>

#include "cJSON.h"
#include "mbedtls/aes.h"

/*-------------------------------------------define---------------------------------------------*/
#define KAIWAN_FRAME_PREFIX_SIZE 11U /* 数据域之前的固定字节数 */
#define KAIWAN_FRAME_CRC_SIZE 2U     /* CRC16 字节数 */
#define KAIWAN_FRAME_TAIL_SIZE 3U    /* “END”帧尾字节数 */

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/* 协议固定的“WTK”帧头和“END”帧尾。 */
static const uint8_t kaiwan_frame_header[3] = {0x57U, 0x54U, 0x4BU};
static const uint8_t kaiwan_frame_tail[3] = {0x45U, 0x4EU, 0x44U};

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kaiwan_read_be16
* Description    : 从网络字节序读取16位无符号数
* Input          : data - 2字节数据指针
* Output         : 无
* Return         : 大端值，uint16_t
* Attention      : data需至少2字节
*******************************************************************************/
static uint16_t kaiwan_read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

/*******************************************************************************
* Function Name  : kaiwan_write_be16
* Description    : 将16位无符号数写成网络字节序
* Input          : output - 2字节输出指针
*                  value - 待写值
* Output         : output - 大端序写入
* Return         : 无
* Attention      : output需至少2字节
*******************************************************************************/
static void kaiwan_write_be16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

/*******************************************************************************
* Function Name  : kaiwan_read_crc
* Description    : 按配置的高低字节顺序读取帧尾CRC
* Input          : data - CRC所在数据指针
*                  order - 字节序配置
* Output         : 无
* Return         : CRC值，uint16_t
* Attention      : 依据crc_order选择大小端
*******************************************************************************/
static uint16_t kaiwan_read_crc(const uint8_t *data, kaiwan_crc_order_t order)
{
    if (order == KAIWAN_CRC_LITTLE_ENDIAN)
    {
        return (uint16_t)(((uint16_t)data[1] << 8U) | data[0]);
    }
    return kaiwan_read_be16(data);
}

/*******************************************************************************
* Function Name  : kaiwan_write_crc
* Description    : 按配置的高低字节顺序写入帧尾CRC
* Input          : output - CRC写入位置
*                  value - CRC值
*                  order - 字节序配置
* Output         : output - 写入CRC
* Return         : 无
* Attention      : 依据crc_order选择大小端
*******************************************************************************/
static void kaiwan_write_crc(uint8_t *output, uint16_t value, kaiwan_crc_order_t order)
{
    if (order == KAIWAN_CRC_LITTLE_ENDIAN)
    {
        output[0] = (uint8_t)value;
        output[1] = (uint8_t)(value >> 8U);
        return;
    }
    kaiwan_write_be16(output, value);
}

/*******************************************************************************
* Function Name  : kaiwan_hex_value
* Description    : 将单个HEX字符转换为0~15，非法字符返回-1
* Input          : value - 待转换的HEX字符
* Output         : 无
* Return         : 0~15；-1 - 非法字符
* Attention      : 大小写均可，16进制字符
*******************************************************************************/
static int kaiwan_hex_value(char value)
{
    if ((value >= '0') && (value <= '9'))
    {
        return value - '0';
    }
    if ((value >= 'a') && (value <= 'f'))
    {
        return value - 'a' + 10;
    }
    if ((value >= 'A') && (value <= 'F'))
    {
        return value - 'A' + 10;
    }
    return -1;
}

/*******************************************************************************
* Function Name  : kaiwan_ascii_is_exact
* Description    : 校验定长字段中无提前结束符且末尾恰好结束
* Input          : input - 待校验字符串
*                  input_capacity - 字符串容量
*                  required_length - 期望长度
* Output         : 无
* Return         : 1 - 合法；0 - 非法或容量不足
* Attention      : 这里只检查长度和结束符，不检查内容是否为数字
*******************************************************************************/
static int kaiwan_ascii_is_exact(const char *input, size_t input_capacity, size_t required_length)
{
    size_t index;

    if ((input == NULL) || (input_capacity <= required_length))
    {
        return 0;
    }
    for (index = 0U; index < required_length; ++index)
    {
        if (input[index] == '\0')
        {
            return 0;
        }
    }
    return input[required_length] == '\0';
}

/*******************************************************************************
* Function Name  : kaiwan_copy_ascii_exact
* Description    : 校验并复制固定长度ASCII字段，不复制字符串结尾
* Input          : input - 源字符串
*                  input_capacity - 源容量
*                  required_length - 期望定长
*                  output - 目标缓冲区
* Output         : output - 复制required_length字节
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_ARGUMENT - 校验失败
* Attention      : 只复制定长内容，不含'\0'
*******************************************************************************/
static kaiwan_result_t kaiwan_copy_ascii_exact(const char *input, size_t input_capacity,
                                       size_t required_length, uint8_t *output)
{
    if ((output == NULL) || !kaiwan_ascii_is_exact(input, input_capacity, required_length))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    memcpy(output, input, required_length);
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_recover_frame_length
* Description    : 根据解密帧内dataLen恢复真实长度，分离末尾0x30填充
* Input          : frame - 解密后帧数据
*                  available_length - 可用长度
*                  frame_length - 输出帧长度
* Output         : frame_length - 解码后的真实长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_FORMAT - 帧头或长度非法
* Attention      : 帧头必须是WTK，长度需在available与MAXIMUM之内
*******************************************************************************/
static kaiwan_result_t kaiwan_recover_frame_length(const uint8_t *frame, size_t available_length,
                                           size_t *frame_length)
{
    size_t expected_length;

    if ((frame == NULL) || (frame_length == NULL) || (available_length < KAIWAN_PROTOCOL_FRAME_OVERHEAD))
    {
        return KAIWAN_ERROR_FORMAT;
    }
    if (memcmp(frame, kaiwan_frame_header, sizeof(kaiwan_frame_header)) != 0)
    {
        return KAIWAN_ERROR_FORMAT;
    }

    expected_length = (size_t)kaiwan_read_be16(&frame[9]) + KAIWAN_PROTOCOL_FRAME_OVERHEAD;
    if ((expected_length > available_length) || (expected_length > KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE))
    {
        return KAIWAN_ERROR_FORMAT;
    }
    *frame_length = expected_length;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_config_init
* Description    : 初始化协议版本、CRC字节序和AES明文模式
* Input          : config - 待填充的协议配置
* Output         : config - 已填默认值
* Return         : 无
* Attention      : 填入版本V3.6、大端CRC、HEX明文模式
*******************************************************************************/
void kaiwan_protocol_config_init(kaiwan_protocol_config_t *config)
{
    if (config == NULL)
    {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->protocol_version = KAIWAN_PROTOCOL_VERSION_DEFAULT;
    config->crc_order = KAIWAN_CRC_BIG_ENDIAN;
    config->aes_plain_mode = KAIWAN_AES_PLAIN_HEX_FRAME;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_crc16_xmodem
* Description    : 按多项式0x1021、初始值0计算CRC16-XMODEM
* Input          : data - 待计算数据
*                  data_length - 数据长度
* Output         : 无
* Return         : CRC16值，uint16_t
* Attention      : data为NULL且长度非0时返回0
*******************************************************************************/
uint16_t kaiwan_protocol_crc16_xmodem(const uint8_t *data, size_t data_length)
{
    uint16_t crc = 0x0000U;
    size_t index;
    uint8_t bit;

    if ((data == NULL) && (data_length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < data_length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8U;
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc <<= 1U;
            }
        }
    }
    return crc;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_hex_encode
* Description    : 二进制转大写ASCII HEX，并补字符串结束符
* Input          : input - 二进制数据
*                  input_length - 数据长度
*                  output - 目标缓冲区
*                  output_capacity - 目标容量
*                  output_length - 输出HEX长度
* Output         : output - HEX字符串；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_ARGUMENT / KAIWAN_ERROR_CAPACITY - 失败
* Attention      : 容量需满足 input_length*2+1
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_hex_encode(const uint8_t *input, size_t input_length, char *output,
                                   size_t output_capacity, size_t *output_length)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t index;

    if ((output == NULL) || (output_length == NULL) || ((input == NULL) && (input_length != 0U)))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if ((input_length > (SIZE_MAX / 2U)) || (output_capacity < ((input_length * 2U) + 1U)))
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    for (index = 0U; index < input_length; ++index)
    {
        output[index * 2U] = hex[input[index] >> 4U];
        output[(index * 2U) + 1U] = hex[input[index] & 0x0FU];
    }
    output[input_length * 2U] = '\0';
    *output_length = input_length * 2U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_hex_decode
* Description    : ASCII HEX转二进制，拒绝奇数长度和非法字符
* Input          : input - HEX字符串
*                  input_length - HEX长度（偶数）
*                  output - 目标缓冲区
*                  output_capacity - 目标容量
*                  output_length - 输出字节数
* Output         : output - 二进制；output_length - 字节数
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_ARGUMENT / KAIWAN_ERROR_FORMAT / KAIWAN_ERROR_CAPACITY - 失败
* Attention      : 奇数长度或非法HEX字符返回FORMAT
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_hex_decode(const char *input, size_t input_length, uint8_t *output,
                                   size_t output_capacity, size_t *output_length)
{
    size_t index;
    int high;
    int low;

    if ((input == NULL) || (output == NULL) || (output_length == NULL))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if ((input_length & 1U) != 0U)
    {
        return KAIWAN_ERROR_FORMAT;
    }
    if (output_capacity < (input_length / 2U))
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    for (index = 0U; index < input_length; index += 2U)
    {
        high = kaiwan_hex_value(input[index]);
        low = kaiwan_hex_value(input[index + 1U]);
        if ((high < 0) || (low < 0))
        {
            return KAIWAN_ERROR_FORMAT;
        }
        output[index / 2U] = (uint8_t)((high << 4) | low);
    }
    *output_length = input_length / 2U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_build_frame
* Description    : 组装完整WTK帧，依次写入固定字段、数据、CRC和END
* Input          : config - 协议配置
*                  sequence - 序号
*                  command - 命令字
*                  data - 数据域
*                  data_length - 数据域长度
*                  output - 目标缓冲区
*                  output_capacity - 目标容量
*                  output_length - 输出帧长度
* Output         : output - 完整帧；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_ARGUMENT / KAIWAN_ERROR_CAPACITY - 失败
* Attention      : 帧长为 data_length + FRAME_OVERHEAD
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_build_frame(const kaiwan_protocol_config_t *config, uint16_t sequence,
                                    uint8_t command, const uint8_t *data, uint16_t data_length,
                                    uint8_t *output, size_t output_capacity, size_t *output_length)
{
    size_t frame_length;
    uint16_t crc;

    if ((config == NULL) || (output == NULL) || (output_length == NULL) ||
        ((data == NULL) && (data_length != 0U)))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (data_length > KAIWAN_PROTOCOL_MAXIMUM_DATA_SIZE)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    frame_length = (size_t)data_length + KAIWAN_PROTOCOL_FRAME_OVERHEAD;
    if (output_capacity < frame_length)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    memcpy(output, kaiwan_frame_header, sizeof(kaiwan_frame_header));
    output[3] = config->protocol_version;
    kaiwan_write_be16(&output[4], config->manufacturer_id);
    kaiwan_write_be16(&output[6], sequence);
    output[8] = command;
    kaiwan_write_be16(&output[9], data_length);
    if (data_length != 0U)
    {
        memcpy(&output[KAIWAN_FRAME_PREFIX_SIZE], data, data_length);
    }

    crc = kaiwan_protocol_crc16_xmodem(output, KAIWAN_FRAME_PREFIX_SIZE + data_length);
    kaiwan_write_crc(&output[KAIWAN_FRAME_PREFIX_SIZE + data_length], crc, config->crc_order);
    memcpy(&output[KAIWAN_FRAME_PREFIX_SIZE + data_length + KAIWAN_FRAME_CRC_SIZE], kaiwan_frame_tail,
           sizeof(kaiwan_frame_tail));
    *output_length = frame_length;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_parse_frame
* Description    : 校验完整帧并生成引用原始缓存的零拷贝视图
* Input          : config - 协议配置
*                  frame - 帧数据
*                  frame_length - 帧长度
*                  view - 只读帧视图
* Output         : view - 填充的帧视图
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 校验失败
* Attention      : view引用调用方原始frame，不拷贝
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_parse_frame(const kaiwan_protocol_config_t *config, const uint8_t *frame,
                                    size_t frame_length, kaiwan_frame_view_t *view)
{
    size_t expected_length;
    uint16_t data_length;
    uint16_t received_crc;
    uint16_t calculated_crc;
    kaiwan_crc_order_t crc_order = KAIWAN_CRC_BIG_ENDIAN;

    if ((frame == NULL) || (view == NULL) || (frame_length < KAIWAN_PROTOCOL_FRAME_OVERHEAD))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (memcmp(frame, kaiwan_frame_header, sizeof(kaiwan_frame_header)) != 0)
    {
        return KAIWAN_ERROR_FORMAT;
    }
    if ((config != NULL) && (frame[3] != config->protocol_version))
    {
        return KAIWAN_ERROR_VERSION;
    }
    if ((config != NULL) && (kaiwan_read_be16(&frame[4]) != config->manufacturer_id))
    {
        return KAIWAN_ERROR_MANUFACTURER;
    }

    data_length = kaiwan_read_be16(&frame[9]);
    expected_length = (size_t)data_length + KAIWAN_PROTOCOL_FRAME_OVERHEAD;
    if ((frame_length != expected_length) || (expected_length > KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE))
    {
        return KAIWAN_ERROR_FORMAT;
    }
    if (memcmp(&frame[expected_length - KAIWAN_FRAME_TAIL_SIZE], kaiwan_frame_tail, sizeof(kaiwan_frame_tail)) != 0)
    {
        return KAIWAN_ERROR_FORMAT;
    }

    if (config != NULL)
    {
        crc_order = config->crc_order;
    }
    received_crc = kaiwan_read_crc(&frame[KAIWAN_FRAME_PREFIX_SIZE + data_length], crc_order);
    calculated_crc = kaiwan_protocol_crc16_xmodem(frame, KAIWAN_FRAME_PREFIX_SIZE + data_length);
    if (received_crc != calculated_crc)
    {
        return KAIWAN_ERROR_CRC;
    }

    memset(view, 0, sizeof(*view));
    view->raw = frame;
    view->raw_length = frame_length;
    view->protocol_version = frame[3];
    view->manufacturer_id = kaiwan_read_be16(&frame[4]);
    view->sequence = kaiwan_read_be16(&frame[6]);
    view->command = frame[8];
    view->data = &frame[KAIWAN_FRAME_PREFIX_SIZE];
    view->data_length = data_length;
    view->crc = received_crc;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_wrap_json
* Description    : 补0x30、AES-128-CBC加密，并生成平台要求的JSON
* Input          : config - 协议配置
*                  workspace - 协议工作区
*                  frame - 待加密帧
*                  frame_length - 帧长度
*                  iv - 16字节IV
*                  output_json - JSON输出缓冲
*                  output_capacity - JSON容量
*                  output_length - 输出JSON长度
* Output         : output_json - 平台JSON；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 工作区由cloud task独占，避免大数组放任务栈
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_wrap_json(const kaiwan_protocol_config_t *config,
                                  kaiwan_protocol_workspace_t *workspace, const uint8_t *frame,
                                  size_t frame_length, const uint8_t iv[KAIWAN_PROTOCOL_AES_IV_LENGTH],
                                  char *output_json, size_t output_capacity, size_t *output_length)
{
    mbedtls_aes_context aes;
    cJSON *root = NULL;
    uint8_t iv_work[KAIWAN_PROTOCOL_AES_IV_LENGTH];
    char iv_hex[(KAIWAN_PROTOCOL_AES_IV_LENGTH * 2U) + 1U];
    size_t plain_length;
    size_t padded_length;
    size_t cipher_hex_length;
    size_t iv_hex_length;
    int aes_result;
    kaiwan_result_t result;
    kaiwan_frame_view_t frame_view;

    if ((config == NULL) || (workspace == NULL) || (frame == NULL) || (iv == NULL) ||
        (output_json == NULL) || (output_length == NULL) || (frame_length > KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE) ||
        (output_capacity > (size_t)INT_MAX))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (frame_length < KAIWAN_PROTOCOL_FRAME_OVERHEAD)
    {
        return KAIWAN_ERROR_FORMAT;
    }
    if (!kaiwan_ascii_is_exact(config->factory_code, sizeof(config->factory_code),
                           KAIWAN_PROTOCOL_FACTORY_CODE_LENGTH))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    result = kaiwan_protocol_parse_frame(config, frame, frame_length, &frame_view);
    if (result != KAIWAN_OK)
    {
        return result;
    }

    if (config->aes_plain_mode == KAIWAN_AES_PLAIN_HEX_FRAME)
    {
        result = kaiwan_protocol_hex_encode(frame, frame_length, (char *)workspace->plain,
                                        sizeof(workspace->plain), &plain_length);
        if (result != KAIWAN_OK)
        {
            return result;
        }
    }
    else if (config->aes_plain_mode == KAIWAN_AES_PLAIN_BINARY_FRAME)
    {
        memcpy(workspace->plain, frame, frame_length);
        plain_length = frame_length;
    }
    else
    {
        return KAIWAN_ERROR_UNSUPPORTED;
    }

    /* 平台约定用字符 '0' 补足 AES 块，不采用 PKCS#7。 */
    padded_length =
        (plain_length + (KAIWAN_PROTOCOL_AES_KEY_LENGTH - 1U)) & ~(size_t)(KAIWAN_PROTOCOL_AES_KEY_LENGTH - 1U);
    if (padded_length > KAIWAN_PROTOCOL_MAXIMUM_AES_BYTES)
    {
        return KAIWAN_ERROR_CAPACITY;
    }
    if (padded_length > plain_length)
    {
        memset(&workspace->plain[plain_length], 0x30, padded_length - plain_length);
    }

    /* CBC 会更新 IV，因此复制后再交给加密库，保留调用方原始 IV 供 JSON 使用。 */
    memcpy(iv_work, iv, sizeof(iv_work));
    mbedtls_aes_init(&aes);
    aes_result = mbedtls_aes_setkey_enc(&aes, config->aes_key, 128U);
    if (aes_result == 0)
    {
        aes_result = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_length, iv_work,
                                           workspace->plain, workspace->crypt);
    }
    mbedtls_aes_free(&aes);
    if (aes_result != 0)
    {
        return KAIWAN_ERROR_CRYPTO;
    }

    result = kaiwan_protocol_hex_encode(workspace->crypt, padded_length, workspace->cipher_hex,
                                    sizeof(workspace->cipher_hex), &cipher_hex_length);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    result =
        kaiwan_protocol_hex_encode(iv, KAIWAN_PROTOCOL_AES_IV_LENGTH, iv_hex, sizeof(iv_hex), &iv_hex_length);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    (void)cipher_hex_length;
    (void)iv_hex_length;

    root = cJSON_CreateObject();
    if ((root == NULL) ||
        (cJSON_AddStringToObject(root, "factoryCode", config->factory_code) == NULL) ||
        (cJSON_AddStringToObject(root, "iv", iv_hex) == NULL) ||
        (cJSON_AddStringToObject(root, "encryptData", workspace->cipher_hex) == NULL))
    {
        cJSON_Delete(root);
        return KAIWAN_ERROR_JSON;
    }
    if (!cJSON_PrintPreallocated(root, output_json, (int)output_capacity, 0))
    {
        cJSON_Delete(root);
        return KAIWAN_ERROR_CAPACITY;
    }
    cJSON_Delete(root);
    *output_length = strlen(output_json);
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_unwrap_json
* Description    : 解析JSON、AES解密、去填充，并重新校验完整协议帧
* Input          : config - 协议配置
*                  workspace - 协议工作区
*                  json - JSON字符串
*                  json_length - JSON长度
*                  output_frame - 帧输出缓冲
*                  output_capacity - 帧容量
*                  output_length - 输出帧长度
* Output         : output_frame - 解密帧；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 会校验末尾0x30填充与完整CRC
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_unwrap_json(const kaiwan_protocol_config_t *config,
                                    kaiwan_protocol_workspace_t *workspace, const char *json,
                                    size_t json_length, uint8_t *output_frame, size_t output_capacity,
                                    size_t *output_length)
{
    cJSON *root = NULL;
    const cJSON *iv_item;
    const cJSON *cipher_item;
    const char *iv_text;
    const char *cipher_text;
    size_t cipher_text_length;
    size_t cipher_length;
    size_t exact_frame_length;
    size_t encoded_frame_length;
    size_t index;
    uint8_t prefix[KAIWAN_PROTOCOL_FRAME_OVERHEAD];
    uint8_t iv[KAIWAN_PROTOCOL_AES_IV_LENGTH];
    size_t iv_length;
    mbedtls_aes_context aes;
    int aes_result;
    kaiwan_result_t result;
    kaiwan_frame_view_t frame_view;

    if ((config == NULL) || (workspace == NULL) || (json == NULL) || (output_frame == NULL) ||
        (output_length == NULL))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }

    /* 只在给定长度内解析，不依赖 MQTT 负载以 NUL 结束。 */
    root = cJSON_ParseWithLength(json, json_length);
    if (root == NULL)
    {
        return KAIWAN_ERROR_JSON;
    }
    iv_item = cJSON_GetObjectItemCaseSensitive(root, "iv");
    cipher_item = cJSON_GetObjectItemCaseSensitive(root, "encryptData");
    if (!cJSON_IsString(iv_item) || !cJSON_IsString(cipher_item))
    {
        cJSON_Delete(root);
        return KAIWAN_ERROR_JSON;
    }
    iv_text = cJSON_GetStringValue(iv_item);
    cipher_text = cJSON_GetStringValue(cipher_item);
    if ((iv_text == NULL) || (cipher_text == NULL))
    {
        cJSON_Delete(root);
        return KAIWAN_ERROR_JSON;
    }
    cipher_text_length = strlen(cipher_text);
    if ((strlen(iv_text) != (KAIWAN_PROTOCOL_AES_IV_LENGTH * 2U)) || ((cipher_text_length & 31U) != 0U) ||
        (cipher_text_length == 0U) || (cipher_text_length > KAIWAN_PROTOCOL_MAXIMUM_CIPHER_HEX_CHARS))
    {
        cJSON_Delete(root);
        return KAIWAN_ERROR_FORMAT;
    }

    result = kaiwan_protocol_hex_decode(iv_text, KAIWAN_PROTOCOL_AES_IV_LENGTH * 2U, iv, sizeof(iv), &iv_length);
    if (result == KAIWAN_OK)
    {
        result = kaiwan_protocol_hex_decode(cipher_text, cipher_text_length, workspace->crypt,
                                        sizeof(workspace->crypt), &cipher_length);
    }
    cJSON_Delete(root);
    if ((result != KAIWAN_OK) || (iv_length != KAIWAN_PROTOCOL_AES_IV_LENGTH))
    {
        return (result == KAIWAN_OK) ? KAIWAN_ERROR_FORMAT : result;
    }

    mbedtls_aes_init(&aes);
    aes_result = mbedtls_aes_setkey_dec(&aes, config->aes_key, 128U);
    if (aes_result == 0)
    {
        aes_result = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipher_length, iv,
                                           workspace->crypt, workspace->plain);
    }
    mbedtls_aes_free(&aes);
    if (aes_result != 0)
    {
        return KAIWAN_ERROR_CRYPTO;
    }

    if (config->aes_plain_mode == KAIWAN_AES_PLAIN_HEX_FRAME)
    {
        if (cipher_length < (KAIWAN_PROTOCOL_FRAME_OVERHEAD * 2U))
        {
            return KAIWAN_ERROR_FORMAT;
        }
        result =
            kaiwan_protocol_hex_decode((const char *)workspace->plain, KAIWAN_PROTOCOL_FRAME_OVERHEAD * 2U,
                                   prefix, sizeof(prefix), &encoded_frame_length);
        if (result != KAIWAN_OK)
        {
            return result;
        }
        result = kaiwan_recover_frame_length(prefix,
                                         /* prefix只保存帧头；完整可用帧容量来自密文长度。 */
                                         cipher_length / 2U, &exact_frame_length);
        if (result != KAIWAN_OK)
        {
            return result;
        }
        encoded_frame_length = exact_frame_length * 2U;
        if ((encoded_frame_length > cipher_length) || (output_capacity < exact_frame_length))
        {
            return KAIWAN_ERROR_CAPACITY;
        }
        /* 根据帧内 dataLen 找到真实结束位置，再逐字节确认剩余填充。 */
        for (index = encoded_frame_length; index < cipher_length; ++index)
        {
            if (workspace->plain[index] != 0x30U)
            {
                return KAIWAN_ERROR_FORMAT;
            }
        }
        result = kaiwan_protocol_hex_decode((const char *)workspace->plain, encoded_frame_length,
                                        output_frame, output_capacity, &encoded_frame_length);
        if ((result != KAIWAN_OK) || (encoded_frame_length != exact_frame_length))
        {
            return (result == KAIWAN_OK) ? KAIWAN_ERROR_FORMAT : result;
        }
    }
    else if (config->aes_plain_mode == KAIWAN_AES_PLAIN_BINARY_FRAME)
    {
        result = kaiwan_recover_frame_length(workspace->plain, cipher_length, &exact_frame_length);
        if (result != KAIWAN_OK)
        {
            return result;
        }
        if (output_capacity < exact_frame_length)
        {
            return KAIWAN_ERROR_CAPACITY;
        }
        for (index = exact_frame_length; index < cipher_length; ++index)
        {
            if (workspace->plain[index] != 0x30U)
            {
                return KAIWAN_ERROR_FORMAT;
            }
        }
        memcpy(output_frame, workspace->plain, exact_frame_length);
    }
    else
    {
        return KAIWAN_ERROR_UNSUPPORTED;
    }

    /* 解密和去填充后仍要重新校验完整帧与 CRC。 */
    result = kaiwan_protocol_parse_frame(config, output_frame, exact_frame_length, &frame_view);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    *output_length = exact_frame_length;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_build_registration_data
* Description    : 按协议字段顺序序列化设备注册数据域
* Input          : registration - 注册信息
*                  output - 数据域输出
*                  output_capacity - 容量
*                  output_length - 输出长度
* Output         : output - 数据域；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 数据域固定55字节，包含device_type与IMEI/IMSI/ICCID等
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_build_registration_data(const kaiwan_registration_t *registration,
                                                uint8_t *output, size_t output_capacity,
                                                uint16_t *output_length)
{
    kaiwan_result_t result;

    if ((registration == NULL) || (output == NULL) || (output_length == NULL))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (output_capacity < 55U)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    output[0] = registration->device_type;
    result = kaiwan_copy_ascii_exact(registration->imei, sizeof(registration->imei),
                                 KAIWAN_PROTOCOL_IMEI_LENGTH, &output[1]);
    if (result == KAIWAN_OK)
    {
        result = kaiwan_copy_ascii_exact(registration->imsi, sizeof(registration->imsi),
                                     KAIWAN_PROTOCOL_IMSI_LENGTH, &output[16]);
    }
    if (result == KAIWAN_OK)
    {
        result = kaiwan_copy_ascii_exact(registration->iccid, sizeof(registration->iccid),
                                     KAIWAN_PROTOCOL_ICCID_LENGTH, &output[31]);
    }
    if (result != KAIWAN_OK)
    {
        return result;
    }

    output[51] = registration->battery_voltage_0_1v;
    output[52] = registration->csq;
    output[53] = registration->battery_percent;
    output[54] = registration->firmware_version;
    *output_length = 55U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_build_door_event_data
* Description    : 序列化门磁设备0x01事件数据域
* Input          : event - 门磁事件
*                  output - 数据域输出
*                  output_capacity - 容量
*                  output_length - 输出长度
* Output         : output - 数据域；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 数据域固定22字节，door_state校验为0/1
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_build_door_event_data(const kaiwan_door_event_t *event, uint8_t *output,
                                              size_t output_capacity, uint16_t *output_length)
{
    kaiwan_result_t result;

    if ((event == NULL) || (output == NULL) || (output_length == NULL) || (event->door_state > 1U))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (output_capacity < 22U)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    output[0] = KAIWAN_DEVICE_DOOR_CONTACT;
    result =
        kaiwan_copy_ascii_exact(event->imei, sizeof(event->imei), KAIWAN_PROTOCOL_IMEI_LENGTH, &output[1]);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    output[16] = event->event_type;
    output[17] = event->door_state;
    output[18] = event->battery_voltage_0_1v;
    output[19] = event->csq;
    output[20] = event->battery_percent;
    output[21] = event->firmware_version;
    *output_length = 22U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_build_alarm_event_data
* Description    : 序列化通用报警设备0x09事件数据域
* Input          : event - 报警事件
*                  output - 数据域输出
*                  output_capacity - 容量
*                  output_length - 输出长度
* Output         : output - 数据域；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 数据域固定21字节
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_build_alarm_event_data(const kaiwan_alarm_event_t *event, uint8_t *output,
                                               size_t output_capacity, uint16_t *output_length)
{
    kaiwan_result_t result;

    if ((event == NULL) || (output == NULL) || (output_length == NULL))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (output_capacity < 21U)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    output[0] = KAIWAN_DEVICE_ALARM;
    result =
        kaiwan_copy_ascii_exact(event->imei, sizeof(event->imei), KAIWAN_PROTOCOL_IMEI_LENGTH, &output[1]);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    output[16] = event->event_type;
    output[17] = event->battery_voltage_0_1v;
    output[18] = event->csq;
    output[19] = event->battery_percent;
    output[20] = event->firmware_version;
    *output_length = 21U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_build_ir_event_data
* Description    : 序列化红外报警设备0x0B事件数据域
* Input          : event - 红外事件
*                  output - 数据域输出
*                  output_capacity - 容量
*                  output_length - 输出长度
* Output         : output - 数据域；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 数据域固定22字节，armed_state允许0/1/0xFF
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_build_ir_event_data(const kaiwan_ir_event_t *event, uint8_t *output,
                                            size_t output_capacity, uint16_t *output_length)
{
    kaiwan_result_t result;

    if ((event == NULL) || (output == NULL) || (output_length == NULL) ||
        ((event->armed_state > 1U) && (event->armed_state != 0xFFU)))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (output_capacity < 22U)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    output[0] = KAIWAN_DEVICE_IR_ALARM;
    result =
        kaiwan_copy_ascii_exact(event->imei, sizeof(event->imei), KAIWAN_PROTOCOL_IMEI_LENGTH, &output[1]);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    output[16] = event->event_type;
    output[17] = event->battery_voltage_0_1v;
    output[18] = event->csq;
    output[19] = event->battery_percent;
    output[20] = event->firmware_version;
    output[21] = event->armed_state;
    *output_length = 22U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_build_command_result_data
* Description    : 序列化0x04命令结果，result=1表示成功、2表示失败
* Input          : result_data - 命令结果信息
*                  output - 数据域输出
*                  output_capacity - 容量
*                  output_length - 输出长度
* Output         : output - 数据域；output_length - 长度
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 数据域固定17字节；result仅允许1/2
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_build_command_result_data(const kaiwan_command_result_t *result_data,
                                                  uint8_t *output, size_t output_capacity,
                                                  uint16_t *output_length)
{
    kaiwan_result_t result;

    if ((result_data == NULL) || (output == NULL) || (output_length == NULL) ||
        ((result_data->result != 1U) && (result_data->result != 2U)))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if (output_capacity < 17U)
    {
        return KAIWAN_ERROR_CAPACITY;
    }

    result = kaiwan_copy_ascii_exact(result_data->imei, sizeof(result_data->imei), KAIWAN_PROTOCOL_IMEI_LENGTH,
                                 output);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    output[15] = result_data->instruction;
    output[16] = result_data->result;
    *output_length = 17U;
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_parse_relay_command
* Description    : 校验0x03/0x62下行格式并提取继电器状态
* Input          : frame - 已解析的帧视图
*                  relay_state - 输出继电器状态
* Output         : relay_state - 0关/1开
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 命令字必须为DOWNLINK，data_length必须为2
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_parse_relay_command(const kaiwan_frame_view_t *frame, uint8_t *relay_state)
{
    if ((frame == NULL) || (relay_state == NULL))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if ((frame->data == NULL) || (frame->command != KAIWAN_COMMAND_DOWNLINK) || (frame->data_length != 2U) ||
        (frame->data[0] != KAIWAN_INSTRUCTION_RELAY_STATE) || (frame->data[1] > 1U))
    {
        return KAIWAN_ERROR_FORMAT;
    }
    *relay_state = frame->data[1];
    return KAIWAN_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_protocol_parse_server_response
* Description    : 校验0xFF平台响应格式并提取业务响应码
* Input          : frame - 已解析的帧视图
*                  response_code - 输出业务响应码
* Output         : response_code - 0/1/2/3
* Return         : KAIWAN_OK - 成功；KAIWAN_ERROR_* - 失败
* Attention      : 命令字必须为SERVER_RESPONSE，data_length必须为1
*******************************************************************************/
kaiwan_result_t kaiwan_protocol_parse_server_response(const kaiwan_frame_view_t *frame, uint8_t *response_code)
{
    if ((frame == NULL) || (response_code == NULL))
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    if ((frame->data == NULL) || (frame->command != KAIWAN_COMMAND_SERVER_RESPONSE) ||
        (frame->data_length != 1U) || (frame->data[0] > 3U))
    {
        return KAIWAN_ERROR_FORMAT;
    }
    *response_code = frame->data[0];
    return KAIWAN_OK;
}
