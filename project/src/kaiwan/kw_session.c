/*------------------------------------------includes--------------------------------------------*/
#include "kaiwan/kw_session.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kw_session_decode
* Description    : 校验下行密文、帧校验及当前产品协议身份
* Input          : config/workspace - 产品配置及独占工作区；json/json_size - 密文；frame/capacity - 缓冲
* Output         : view - 引用帧缓冲的解析视图
* Return         : KW_OK - 有效帧；其他 - 协议错误
* Attention      : 不处理产品状态；注册、心跳、事件和门禁命令由各产品关联
*******************************************************************************/
kw_result_t kw_session_decode(const kw_protocol_config_t *config,
                              kw_protocol_workspace_t *workspace, const char *json,
                              size_t json_size, uint8_t *frame, size_t capacity,
                              kw_frame_view_t *view)
{
    size_t length;
    kw_result_t result;
    if (!config || !workspace || !json || !frame || !view)
    {
        return KW_ERR_ARGUMENT;
    }
    result = kw_protocol_unwrap_json(config, workspace, json, json_size, frame, capacity, &length);
    if (result != KW_OK)
    {
        return result;
    }
    /* 解密成功只是拿到帧，仍须做帧长度、CRC 和字段校验。 */
    result = kw_protocol_parse_frame(config, frame, length, view);
    if (result != KW_OK)
    {
        return result;
    }
    /* 会话层再次绑定当前产品身份，避免跨产品报文进入业务逻辑。 */
    if (view->manufacturer_id != config->manufacturer_id ||
        view->protocol_version != config->protocol_version)
    {
        return KW_ERR_ARGUMENT;
    }
    return KW_OK;
}
