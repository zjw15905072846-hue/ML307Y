/*********************************************************
 *  @file    cm_demo_ssl.c
 *  @brief   OpenCPU ssl示例
 *  Copyright (c) 2023 China Mobile IOT.
 *  All rights reserved.
 *  created by ShiMingRui 2023/6/29
 ********************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cm_os.h"
#include "cm_mem.h"
#include "cm_fs.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "cm_ssl.h"
#include "cm_demo_uart.h"
#include "cm_demo_ssl.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define STR_LEN(s) (sizeof(s) - 1)
#define STR_ITEM(s) (s), STR_LEN(s)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 测试前请先补充如下参数 */
static char *test_ssl_addr = "";     //测试地址, 例: "192.168.0.1"
static char *test_ssl_hostname = "";
static int test_ssl_port = 9999;       //测试端口

static void *test_ssl_ctx = NULL;
static int test_ssl_sock = -1;
static osThreadId_t ssl_recv_task_handle = NULL;

static char *test_serve_ca_filename = "serve_ca";
static char *test_client_ca_filename = "client_ca";
static char *test_client_key_filename = "client_key";

/* ca证书 */
static char *test_serve_ca = "";

/* 客户端证书 */
static char *test_client_ca = "";

/* 客户端密钥 */
static char *test_client_key = "";


/* 证书格式如下，以www.baidu.com根证书为例 */
/*static char *http_ca = "-----BEGIN CERTIFICATE-----\r\n" \
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\r\n" \
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\r\n" \
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\r\n" \
"MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\r\n" \
"YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\r\n" \
"aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\r\n" \
"jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\r\n" \
"xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\r\n" \
"1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\r\n" \
"snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\r\n" \
"U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\r\n" \
"9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\r\n" \
"BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\r\n" \
"AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\r\n" \
"yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\r\n" \
"38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\r\n" \
"AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\r\n" \
"DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\r\n" \
"HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\r\n" \
"-----END CERTIFICATE-----\r\n" \
"-----BEGIN CERTIFICATE-----\r\n" \
"MIIEaTCCA1GgAwIBAgILBAAAAAABRE7wQkcwDQYJKoZIhvcNAQELBQAwVzELMAkG\r\n" \
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\r\n" \
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw0xNDAyMjAxMDAw\r\n" \
"MDBaFw0yNDAyMjAxMDAwMDBaMGYxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\r\n" \
"YWxTaWduIG52LXNhMTwwOgYDVQQDEzNHbG9iYWxTaWduIE9yZ2FuaXphdGlvbiBW\r\n" \
"YWxpZGF0aW9uIENBIC0gU0hBMjU2IC0gRzIwggEiMA0GCSqGSIb3DQEBAQUAA4IB\r\n" \
"DwAwggEKAoIBAQDHDmw/I5N/zHClnSDDDlM/fsBOwphJykfVI+8DNIV0yKMCLkZc\r\n" \
"C33JiJ1Pi/D4nGyMVTXbv/Kz6vvjVudKRtkTIso21ZvBqOOWQ5PyDLzm+ebomchj\r\n" \
"SHh/VzZpGhkdWtHUfcKc1H/hgBKueuqI6lfYygoKOhJJomIZeg0k9zfrtHOSewUj\r\n" \
"mxK1zusp36QUArkBpdSmnENkiN74fv7j9R7l/tyjqORmMdlMJekYuYlZCa7pnRxt\r\n" \
"Nw9KHjUgKOKv1CGLAcRFrW4rY6uSa2EKTSDtc7p8zv4WtdufgPDWi2zZCHlKT3hl\r\n" \
"2pK8vjX5s8T5J4BO/5ZS5gIg4Qdz6V0rvbLxAgMBAAGjggElMIIBITAOBgNVHQ8B\r\n" \
"Af8EBAMCAQYwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQUlt5h8b0cFilT\r\n" \
"HMDMfTuDAEDmGnwwRwYDVR0gBEAwPjA8BgRVHSAAMDQwMgYIKwYBBQUHAgEWJmh0\r\n" \
"dHBzOi8vd3d3Lmdsb2JhbHNpZ24uY29tL3JlcG9zaXRvcnkvMDMGA1UdHwQsMCow\r\n" \
"KKAmoCSGImh0dHA6Ly9jcmwuZ2xvYmFsc2lnbi5uZXQvcm9vdC5jcmwwPQYIKwYB\r\n" \
"BQUHAQEEMTAvMC0GCCsGAQUFBzABhiFodHRwOi8vb2NzcC5nbG9iYWxzaWduLmNv\r\n" \
"bS9yb290cjEwHwYDVR0jBBgwFoAUYHtmGkUNl8qJUC99BM00qP/8/UswDQYJKoZI\r\n" \
"hvcNAQELBQADggEBAEYq7l69rgFgNzERhnF0tkZJyBAW/i9iIxerH4f4gu3K3w4s\r\n" \
"32R1juUYcqeMOovJrKV3UPfvnqTgoI8UV6MqX+x+bRDmuo2wCId2Dkyy2VG7EQLy\r\n" \
"XN0cvfNVlg/UBsD84iOKJHDTu/B5GqdhcIOKrwbFINihY9Bsrk8y1658GEV1BSl3\r\n" \
"30JAZGSGvip2CTFvHST0mdCF/vIhCPnG9vHQWe3WVjwIKANnuvD58ZAWR65n5ryA\r\n" \
"SOlCdjSXVWkkDoPWoC209fN5ikkodBpBocLTJIg1MGCUF7ThBCIxPTsvFwayuJ2G\r\n" \
"K1pp74P1S8SqtCr4fKGxhZSM9AyHDPSsQPhZSZg=\r\n" \
"-----END CERTIFICATE-----\r\n";*/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void __ssl_recv_task(void)
{
    uint8_t recv_buf[512] = {0};
    int ret = 0;

    while (1)
    {
        if (cm_ssl_check_pending(test_ssl_ctx) == 0)
        {
            /* SSL缓冲区无挂起数据，尝试从底层socket读取 */
            ret = cm_ssl_read(test_ssl_ctx, recv_buf, sizeof(recv_buf) - 1);
            if (ret > 0)
            {
                recv_buf[ret] = '\0';
                cm_demo_printf("ssl recv len=%d: %s\n", ret, recv_buf);
            }
            else if (ret < 0 && ret != -0x6900)
            {
                /* -0x6900 = MBEDTLS_ERR_SSL_WANT_READ，表示暂无数据，非错误 */
                cm_demo_printf("ssl recv error %d\n", ret);
                osThreadSuspend(ssl_recv_task_handle);
                continue;
            }
            /* ret == 0 或 WANT_READ 表示暂无数据，继续轮询 */
        }
        else
        {
            /* SSL缓冲区有数据，逐块读出 */
            while (cm_ssl_get_bytes_avail(test_ssl_ctx) > 0)
            {
                ret = cm_ssl_read(test_ssl_ctx, recv_buf, sizeof(recv_buf) - 1);
                if (ret > 0)
                {
                    recv_buf[ret] = '\0';
                    cm_demo_printf("ssl recv len=%d: %s\n", ret, recv_buf);
                }
                else if (ret <= 0)
                {
                    cm_demo_printf("ssl connection closed\n");
                    close(test_ssl_sock);
                    osThreadSuspend(ssl_recv_task_handle);
                    break;
                }
            }
        }

        osDelay(100);
    }
    cm_demo_printf("ssl recv task exit\n");
}

static int __tcp_connect(void)
{
    int ret = 0;

    test_ssl_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (test_ssl_sock == -1)
    {
        cm_demo_printf("tcp socket create error\n");
        return -1;
    }

    cm_demo_printf("tcp socket id %d\n", test_ssl_sock);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(test_ssl_port);
    server_addr.sin_addr.s_addr = inet_addr(test_ssl_addr);

    ret = connect(test_ssl_sock, (const struct sockaddr *)&server_addr, sizeof(server_addr));

    if (ret < 0)
    {
        if (EINPROGRESS == errno)
        {
            cm_demo_printf("tcp wait connect...\n");
        }
        else
        {
            cm_demo_printf("tcp connect error\n");
            close(test_ssl_sock);
            test_ssl_sock = -1;
            return -1;
        }
    }

    cm_demo_printf("tcp connect succ\n");
    return 0;
}

static void __ssl_conn(int type)
{
    int ret = 0;
    int ssl_id = 1;
    uint16_t negotime = 60;
    uint8_t session = 0;
    uint8_t sni = 0;
    uint8_t version = 255;
    int32_t cipher_suite = 0x0000;
    uint8_t ignorstamp = 1;
    uint8_t ignorverify = 1;

    cm_demo_printf("ssl set serve_ca ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_CA_CERT_FILENAME, test_serve_ca_filename));
    cm_demo_printf("ssl set client_ca ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_CLI_CERT_FILENAME, test_client_ca_filename));
    cm_demo_printf("ssl set client_key ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_CLI_KEY_FILENAME, test_client_key_filename));
    cm_demo_printf("ssl set negotime ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_NEGOTIME, &negotime));
    cm_demo_printf("ssl set session ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_SESSION, &session));
    cm_demo_printf("ssl set sni ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_SNI, &sni));
    cm_demo_printf("ssl set version ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_VERSION, &version));
    cm_demo_printf("ssl set cipher_suite ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_SUITES, &cipher_suite));
    cm_demo_printf("ssl set ignorstamp ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_IGNORE_STAMP, &ignorstamp));
    cm_demo_printf("ssl set ignorverify ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_IGNORE_VERIFY, &ignorverify));

    switch (type)
    {
        case 0:
        {
            int verify = 0;
            cm_demo_printf("ssl set ret verify %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_VERIFY, &verify));
            break;
        }

        case 1:
        {
            int verify = 1;
            cm_demo_printf("ssl set verify ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_VERIFY, &verify));
            cm_demo_printf("ssl write_cert ret %d\n", cm_ssl_write_cert_file((uint8_t *)test_serve_ca_filename, strlen(test_serve_ca), 0, (uint8_t *)test_serve_ca, CM_SSL_FILE_TYPE_CERT));
            break;
        }

        case 2:
        {
            int verify = 2;
            int write_ok = 1;
            cm_demo_printf("ssl set verify ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_VERIFY, &verify));

            /* 先清除可能残留的旧证书记录 */
            // cm_ssl_rm_cert_file((uint8_t *)test_serve_ca_filename);
            // cm_ssl_rm_cert_file((uint8_t *)test_client_ca_filename);
            // cm_ssl_rm_cert_file((uint8_t *)test_client_key_filename);

            ret = cm_ssl_write_cert_file((uint8_t *)test_client_key_filename, strlen(test_client_key), 0, (uint8_t *)test_client_key, CM_SSL_FILE_TYPE_KEY);
            cm_demo_printf("ssl write client_key ret %d, len=%d\n", ret, strlen(test_client_key));
            if (ret != 0) 
            {
                break;
            }

            ret = cm_ssl_write_cert_file((uint8_t *)test_serve_ca_filename, strlen(test_serve_ca), 0, (uint8_t *)test_serve_ca, CM_SSL_FILE_TYPE_CERT);
            cm_demo_printf("ssl write serve_ca ret %d, len=%d\n", ret, strlen(test_serve_ca));
            if (ret != 0)
            {
                break;
            }

            ret = cm_ssl_write_cert_file((uint8_t *)test_client_ca_filename, strlen(test_client_ca), 0, (uint8_t *)test_client_ca, CM_SSL_FILE_TYPE_CERT);
            cm_demo_printf("ssl write client_ca ret %d, len=%d\n", ret, strlen(test_client_ca));
            if (ret != 0)
            {
                break;
            }
            break;
        }

        default:
        {
            cm_demo_printf("invalid type\n");
            return;
        }
    }
    if(ret == 0)
    {
       ret = cm_ssl_conn((void **)&test_ssl_ctx, ssl_id, test_ssl_sock, 0);
       cm_demo_printf("ssl connect ret %d\n", ret);
    }
    
    if (ret != 0)
    {
        return;
    }
    if (ssl_recv_task_handle == NULL)
    {
        osThreadAttr_t attr = {0};
        attr.name = "ssl_recv";
        attr.stack_size = 1024 * 2;
        attr.priority = osPriorityNormal;

        ssl_recv_task_handle = osThreadNew((osThreadFunc_t)__ssl_recv_task, NULL, &attr);
        if (ssl_recv_task_handle == NULL)
        {
            cm_demo_printf("create ssl recv task failed\n");
        }
    }
    else
    {
        osThreadResume(ssl_recv_task_handle);
    }
}

static void __on_cmd_ssl_conn_ex()
{
    /* 事先需要设置SSL参数，先选择使用通道，再设置该通道的验证类型，验证所需的证书 */
    int ssl_id = 1;     //SSL通道
    uint16_t negotime = 60;  //握手超时(s)
    uint8_t session = 1;
    uint8_t sni = 1;
    uint8_t version = 255;
    int32_t cipher_suite = 0x0000;
    uint8_t ignorstamp  = 0;
    uint8_t ignorverify  = 0;
    cm_demo_printf("ssl set serve_ca ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_CA_CERT_FILENAME, test_serve_ca_filename));
    cm_demo_printf("ssl set client_ca ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_CLI_CERT_FILENAME, test_client_ca_filename));
    cm_demo_printf("ssl set client_key ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_CLI_KEY_FILENAME, test_client_key_filename));
    cm_demo_printf("ssl set negotime ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_NEGOTIME, &negotime));
    cm_demo_printf("ssl set session ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_SESSION, &session));
    cm_demo_printf("ssl set sni ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_SNI, &sni));
    cm_demo_printf("ssl set version ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_VERSION, &version));
    cm_demo_printf("ssl set cipher_suite ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_SUITES, &cipher_suite));
    cm_demo_printf("ssl set ignorstamp ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_IGNORE_STAMP, &ignorstamp));
    cm_demo_printf("ssl set ignorverify ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_IGNORE_VERIFY, &ignorverify));

    int verify = 0;     //无证书验证
    cm_demo_printf("ssl set ret %d\n", cm_ssl_setopt(ssl_id, CM_SSL_PARAM_VERIFY, &verify)); 

    int ret = cm_ssl_conn_ex((void **)&test_ssl_ctx, ssl_id, test_ssl_sock, 0, test_ssl_hostname);

    cm_demo_printf("ssl connect ret %d\n", ret);
    if (ret != 0)
    {
        return;
    }
    if (ssl_recv_task_handle == NULL)
    {
        osThreadAttr_t attr = {0};
        attr.name = "ssl_recv";
        attr.stack_size = 1024 * 2;
        attr.priority = osPriorityNormal;

        ssl_recv_task_handle = osThreadNew((osThreadFunc_t)__ssl_recv_task, NULL, &attr);
        if (ssl_recv_task_handle == NULL)
        {
            cm_demo_printf("create ssl recv task failed\n");
        }
    }
    else
    {
        osThreadResume(ssl_recv_task_handle);
    }
}

static void __ssl_send(void)
{
    char send_buffer[128] = "hello";
    int len = strlen(send_buffer);
    int send_len = 0;
    int ret = 0;

    while (send_len < len)
    {
        ret = cm_ssl_write(test_ssl_ctx, (void *)(send_buffer + send_len), len - send_len);

        if (ret == -1)
        {
            cm_demo_printf("ssl write error\n");
            return;
        }

        if (ret > 0)
        {
            send_len += ret;
        }
    }

    cm_demo_printf("ssl send len: %d\n", send_len);
}

void _on_cmd_cakey()
{
    int ret = cm_ssl_check_cert_file_exists((uint8_t *)"demo_test.cert");
    if(ret == 1)
    {
        cm_ssl_rm_cert_file((uint8_t *)"demo_test.cert");
    }
    ret = cm_ssl_write_cert_file((uint8_t *)"demo_test.cert", strlen(test_serve_ca), 0, (uint8_t *)test_serve_ca, CM_SSL_FILE_TYPE_CERT);
    if(ret != 0)
    {
        cm_demo_printf("demo_test.cert write error %d\r\n", ret);
        return;
    }
    ret = cm_ssl_check_cert_file_exists((uint8_t *)"demo_test.cert");
    if(ret == 0)
    {
        cm_demo_printf("demo_test.cert not exitsts\r\n");
        return;
    }
    cm_demo_printf("demo_test.cert exists\r\n");
    ret = cm_ssl_get_cert_file_type((uint8_t *)"demo_test.cert");
    cm_demo_printf("demo_test.cert type %d\r\n", ret);
    uint8_t * file_data = NULL;
    uint16_t file_length = 0;
    ret = cm_ssl_read_cert_file((uint8_t *)"demo_test.cert", &file_data, &file_length);
    cm_demo_printf("demo_test.cert read %d\r\n", ret);
    if(ret == 0)
    {
        cm_demo_printf("demo_test.cert length %u\r\n", file_length);
        cm_demo_printf("%.500s\r\n", file_data);
        cm_free(file_data);
    }
    ret = cm_ssl_write_cert_file((uint8_t *)"demo_test2.cert", strlen(test_serve_ca), 0, (uint8_t *)test_serve_ca, CM_SSL_FILE_TYPE_CERT);
    cm_demo_printf("demo_test2.cert write %d\r\n", ret);
    cm_ssl_certfile_t* cert_list = cm_ssl_list_cert_file();
    while(cert_list != NULL)
    {
        cm_demo_printf("%.500s  ", cert_list->file_name);
        cert_list = cert_list->next;
    }
    cm_demo_printf("\r\n");
    char *check_value = NULL;
    cm_ssl_check_cert_file((uint8_t *)"demo_test.cert", 0, (uint8_t **)&check_value);
    if(check_value != NULL)
    {
        cm_demo_printf("check md5 %s\r\n", check_value);
        cm_free(check_value);
    }
    cm_ssl_rm_cert_file((uint8_t *)"demo_test.cert");
    cm_ssl_rm_cert_file((uint8_t *)"demo_test2.cert");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void cm_test_ssl(EmbeddedCli *cli, char *args, void *context)
{
    int i = 0;
    const char *cmd = embeddedCliGetToken(args, 1);
    if (cmd == NULL)
    {
        cm_demo_printf("invalid param\n");
        return;
    }

    if (strncmp(cmd, STR_ITEM("open")) == 0)
    {
        __tcp_connect();
    }
    else if (strncmp(cmd, STR_ITEM("conn_ex")) == 0)
    {
        __on_cmd_ssl_conn_ex();
    }

    /* ssl conn [type] */
    /* ssl conn 0  无证书验证 */
    /* ssl conn 1  单向验证 */
    /* ssl conn 2  双向验证 */
    /* SSL握手，连接前先设置好认证方式和证书 */
    else if (strncmp(cmd, STR_ITEM("conn")) == 0)
    {
        const char *param = embeddedCliGetToken(args, 2);
        if (param == NULL)
        {
            cm_demo_printf("invalid param\n");
            return;
        }

        __ssl_conn(atoi(param));
    }    
    else if (strncmp(cmd, STR_ITEM("close")) == 0)
    {
        if (ssl_recv_task_handle)
        {
            osThreadSuspend(ssl_recv_task_handle);
        }
            
        if (test_ssl_ctx)
        {
            cm_ssl_close((void **)&test_ssl_ctx);
        }
        
        if (test_ssl_sock != -1)
        {
            close(test_ssl_sock);
            test_ssl_sock = -1;
        }
        cm_demo_printf("ssl close succ\n");
    }
    else if (strncmp(cmd, STR_ITEM("send")) == 0)
    {
        __ssl_send();
    }
    else if (strncmp(cmd, STR_ITEM("cipher")) == 0)
    {
        int *list = cm_ssl_lis_cipher();
        int size = 0;

        while (list[size] != 0)
        {
            size++;
        }

        uint8_t *data = cm_malloc(size * 7 + 1);

        if (data)
        {
            memset(data, 0, size * 7 + 1);
            uint32_t len = 0;

            for (i = 0; i < size; i++)
            {
                len += snprintf((char *)data + len, (size * 7 + 1 - len), "0x%x,", list[i]);
            }

            cm_demo_printf("cipher list: %s\n", data);
            cm_free(data);
        }
    }
     else if (strncmp(cmd, STR_ITEM("ca_key")) == 0)
    {
        _on_cmd_cakey();
    }
}
