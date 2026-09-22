/**
 *  @file    cm_demo_deepseek.c
 *  @brief   OpenCPU deepseek接入示例
 *  @copyright copyright © 2025 China Mobile IOT. All rights reserved.
 *  @author by cmiot0752
 *  @date 2025/06/04
 */


#include <stdio.h>
#include <string.h>
#include "cm_http.h"
#include "cm_ssl.h"
#include "cm_demo_uart.h"
#include "embedded_cli.h"
 
/**
 * @brief 发送DeepSeek API请求（同步方式）
 */
cm_httpclient_ret_code_e send_deepseek_api_request_sync(const char *cmd, const char *api_key)
{
    cm_httpclient_handle_t client_handle = NULL;
    cm_httpclient_ret_code_e ret;
    cm_httpclient_cfg_t cfg = {0};
    const char *header_fmt = 
        "Authorization: Bearer %s\r\n"
        "Content-Type: application/json\r\n\r\n";
    const char *request_fmt = "{"
            "\"model\": \"deepseek-chat\","
            "\"messages\": ["
                "{\"role\": \"system\", \"content\": \"You are a helpful assistant\"},"
                "{\"role\": \"user\", \"content\": \"%s\"}"
            "]"
        "}";
    char headers[256] = {0};
    char request_body[1024] = {0};
    cm_httpclient_sync_param_t sync_param = {0};
    cm_httpclient_sync_response_t response = {0};
    int tmp = 0;
 
    // 创建HTTP客户端实例
    ret = cm_httpclient_create((const uint8_t*)"https://api.deepseek.com", NULL, &client_handle);
    if (ret != CM_HTTP_RET_CODE_OK)
    {
        cm_demo_printf("create client err: %d\n", ret);
        return ret;
    }
 
    // 配置HTTP客户端参数（必须在create之后设置）
    cfg.cid = 0xff;                    // PDP索引
    cfg.conn_timeout = 60;         // 连接超时时间（秒）
    cfg.rsp_timeout = 60;          // 响应超时时间（秒）
    cfg.ssl_enable = true;            // 使用HTTPS
    cfg.ssl_id = 2;               // 默认SSL配置
    cfg.dns_priority = 1;
 
    cm_ssl_setopt(2, CM_SSL_PARAM_VERIFY, (void *)(uintptr_t)tmp);
    ret = cm_httpclient_set_cfg(client_handle, cfg);
    if (ret != CM_HTTP_RET_CODE_OK)
    {
        cm_demo_printf("cfg client err: %d\n", ret);
        goto cleanup;
    }
 
    // 构造请求头
    snprintf(headers, sizeof(headers), header_fmt, api_key);
    cm_demo_printf("header :%s\n", headers);
 
    ret = cm_httpclient_custom_header_set(client_handle, (uint8_t *)headers, strlen(headers));
    if (ret != CM_HTTP_RET_CODE_OK)
    {
        cm_demo_printf("header set err: %d\n", ret);
        goto cleanup;
    }
 
    // 构造请求体
    snprintf(request_body, sizeof(request_body), request_fmt, cmd);
    cm_demo_printf("body :%s\n", request_body);

 
    // 设置同步请求参数
    sync_param.method = HTTPCLIENT_REQUEST_POST;              // POST 请求
    sync_param.path = (const uint8_t*)"/v1/chat/completions"; // DeepSeek 接口路径
    sync_param.content_length = strlen(request_body);         // 请求体长度
    sync_param.content = (uint8_t*)request_body;              // 请求体内容
 
    // 发送同步请求
    ret = cm_httpclient_sync_request(client_handle, sync_param, &response);
    if (ret != CM_HTTP_RET_CODE_OK)
    {
        cm_demo_printf("request err: %d\n", ret);
        goto cleanup;
    }
 
    // 打印响应结果
    cm_demo_printf("Response Code: %u\n", response.response_code);
    cm_demo_printf("Header Length: %u\n", response.response_header_len);
    cm_demo_printf("Content Length: %u\n", response.response_content_len);
 
    if (response.response_header && response.response_header_len > 0)
    {
         cm_demo_printf("Response Header:\n%.*s\n", response.response_header_len, response.response_header);
    }

    if (response.response_content && response.response_content_len > 0)
    {
        cm_demo_printf("Response Content:\n%.*s\n", response.response_content_len, response.response_content);
    }
 
    // 释放同步接口分配的资源
    cm_httpclient_sync_free_data(client_handle);
 
cleanup:
    // 清理资源
    if (client_handle != NULL)
    {
        cm_httpclient_terminate(client_handle);
        cm_httpclient_custom_header_free(client_handle);
        cm_httpclient_delete(client_handle);
    }
 
    return ret;
}

/**
 *  @brief deepseek接入测试命令（同步接口）
 *
 *  @param [in] cli cli句柄
 *  @param [in] args 参数列表
 *  @param [in] context 用户参数
 *  @return None
 *
 *  @details 第一个参数为交互命令，第二个参数为apikey
 */
void cm_test_deepseek(EmbeddedCli *cli, char *args, void *context)
{
    (void)context;

    const char *cmd = embeddedCliGetToken(args, 1);     // 获取第一个参数
    const char *apikey = embeddedCliGetToken(args, 2);  // 获取第二个参数

    cm_httpclient_ret_code_e ret = send_deepseek_api_request_sync(cmd, apikey);
    if (ret != CM_HTTP_RET_CODE_OK)
    {
       cm_demo_printf("DeepSeek API ERR: %d\n", ret);
    }
}