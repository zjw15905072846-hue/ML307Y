/*------------------------------------------includes--------------------------------------------*/
#include "kaiwan/kaiwan_session.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kaiwan_session_decode
* Description    : 校验下行密文、帧校验及当前产品协议身份
* Input          : config/workspace - 产品配置及独占工作区；json/json_size - 密文；frame/capacity - 缓冲
* Output         : view - 引用帧缓冲的解析视图
* Return         : KAIWAN_OK - 有效帧；其他 - 协议错误
* Attention      : 不处理产品状态；注册、心跳、事件和门禁命令由各产品关联
*******************************************************************************/
kaiwan_result_t kaiwan_session_decode(const kaiwan_protocol_config_t *config,
                              kaiwan_protocol_workspace_t *workspace, const char *json,
                              size_t json_size, uint8_t *frame, size_t capacity,
                              kaiwan_frame_view_t *view)
{
    size_t length;
    kaiwan_result_t result;
    if (!config || !workspace || !json || !frame || !view)
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    result = kaiwan_protocol_unwrap_json(config, workspace, json, json_size, frame, capacity, &length);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    /* 解密成功只是拿到帧，仍须做帧长度、CRC 和字段校验。 */
    result = kaiwan_protocol_parse_frame(config, frame, length, view);
    if (result != KAIWAN_OK)
    {
        return result;
    }
    /* 会话层再次绑定当前产品身份，避免跨产品报文进入业务逻辑。 */
    if (view->manufacturer_id != config->manufacturer_id ||
        view->protocol_version != config->protocol_version)
    {
        return KAIWAN_ERROR_ARGUMENT;
    }
    return KAIWAN_OK;
}
