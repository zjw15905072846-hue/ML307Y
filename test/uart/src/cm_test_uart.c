#include "cm_iomux.h"
#include "string.h"
#include "embedded_cli.h"
#include <stdlib.h>
#include "cm_uart.h"
#include "cm_demo_uart.h"

///测试添加
static char data[500]={0};
static uint32_t data_total_len  = 0; 
static char uart_event_param[20] = {0};
static char uart_event_cb_param[50] = {0};
static unsigned int uart_event = 0;
char callback_sign[8] = {0};
cm_uart_event_t event ={0};
#define UART_EVENT(event) #event

//特别注意！！！！！！！
//由于XY现有的irq event软中断框架方案是异步执行中断回调，而非扩展回调使用时，其底层的数据缓冲区非环形队列机制，如果异步中断回调不能得到及时调度，有后到的数据覆盖前到数据的风险。
//为规避此风险，非扩展回调没有使用软中断方案，而是直接在底层中断处理函数中直接执行回调，由于中断时不挂起flash，所以要保证使用非扩展回调时，回调函数及其内部调用的函数全部放到ram上，不可置于flash上。
void cm_uart_callback_cb(char *src, uint32_t len)
{
    // 安全检查：防止缓冲区溢出
    if (data_total_len + len > sizeof(data)) {
        // 处理溢出情况（根据项目规范应记录日志）
        cm_demo_printf(0, "UART RX buffer overflow! (%u+%u>%u)\n",    //该回调中打印东西，模组可能会死机
                     data_total_len, len, (unsigned int)sizeof(data));
        len = sizeof(data) - data_total_len;   //剩余空间
    }
    
    // 追加数据到缓冲区
    memcpy(data + data_total_len, src, len);
    data_total_len += len;
    
    // 注意：这里不添加\0，因为可能是二进制数据
    // 如果后续需要作为字符串处理，应单独进行转换
}


void cm_uart_callback_event_ex(void *param,unsigned int evt)
{
    strcpy(uart_event_cb_param,param);
    uart_event = evt;
}




void cm_test_uart_cfg(EmbeddedCli *cli, char *args, void *context)
{
    cm_uart_byte_size_e byte_size = atoi(embeddedCliGetToken(args, 1)); //8,7,6,5
    cm_uart_parity_e parity = atoi(embeddedCliGetToken(args, 2));       //0,1,2,3,4      /*!< 校验位，枚举*/
    cm_uart_stop_bit_e stop_bit = atoi(embeddedCliGetToken(args, 3));   //0,1,2     /*!< 停止位，枚举*/
    cm_uart_flow_ctrl_e flow_ctrl = atoi(embeddedCliGetToken(args, 4)); //0,1   /*!< 流控制，枚举*/
    int baudrate = atoi(embeddedCliGetToken(args, 5));                  //9600,19200,38400,57600,115200    /*!< 波特率，枚举*/
    int is_lpuart = atoi(embeddedCliGetToken(args, 6));
    int rxrb_buf_size = atoi(embeddedCliGetToken(args, 7));
    cm_uart_cfg_t uart_cfg = {
    byte_size,     /*!< 数据位，枚举*/
    parity,           /*!< 校验位，枚举*/
    stop_bit,       /*!< 停止位，枚举*/
    flow_ctrl,      /*!< 流控制，枚举*/
    baudrate,                       /*!< 波特率，枚举*/
    is_lpuart,          /*!< 1:低功耗串口(115200以内的标准波特率)；0:普通串口*/
    rxrb_buf_size,
    };

    strcpy(uart_event_param,embeddedCliGetToken(args, 8));
    void *event_param = uart_event_param;
    cm_uart_event_type_e event_type = atoi(embeddedCliGetToken(args, 9));//1,2,4;1|2|4=7
    cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 10));//0 1 2
    strcpy(callback_sign,embeddedCliGetToken(args, 11)); //为ex / noex，如果不需要回调函数，输入指令时该参数位不得为空(实测为空时会死机)，可以随意写一些无效字符，比如"nx","0"之类的

    if(strcmp(callback_sign,"noex") == 0)  //配置不同类型的回调函数
    { 
        event.event_type = event_type;
        event.event_param = event_param;
        event.event_entry = cm_uart_callback_cb;
        event.event_entry_ex = NULL;   
    };

    if(strcmp(callback_sign,"ex") == 0)
    { 
        event.event_type = event_type;
        event.event_param = event_param;
        event.event_entry = NULL;
        event.event_entry_ex = cm_uart_callback_event_ex;   
    };
    
   
    if(CM_UART_DEV_1 == dev)
    {

        cm_iomux_set_pin_func(CM_IOMUX_PIN_28, CM_IOMUX_FUNC_FUNCTION1);
        cm_iomux_set_pin_func(CM_IOMUX_PIN_29, CM_IOMUX_FUNC_FUNCTION1);
    }
    if(CM_UART_DEV_0 == dev)
    {
        if ((flow_ctrl == CM_UART_FLOW_CTRL_HW))
        {

            cm_iomux_set_pin_func(CM_IOMUX_PIN_22, CM_IOMUX_FUNC_FUNCTION1);
            cm_iomux_set_pin_func(CM_IOMUX_PIN_23, CM_IOMUX_FUNC_FUNCTION1);
            
        /* 可注册流控事件上报 */
            event.event_type |= CM_UART_EVENT_TYPE_RX_FLOWCTRL;
        }

        cm_iomux_set_pin_func(CM_IOMUX_PIN_17, CM_IOMUX_FUNC_FUNCTION1);
        cm_iomux_set_pin_func(CM_IOMUX_PIN_18, CM_IOMUX_FUNC_FUNCTION1);
    }
    if(CM_UART_DEV_2 == dev)
    {
        /* 若为UART2，需要先将log打印从debug切换到usb */
        //cm_log_config_set(CM_LOG_MODE_USB, 0);
        cm_iomux_set_pin_func(38, CM_IOMUX_FUNC_FUNCTION1);
        cm_iomux_set_pin_func(39, CM_IOMUX_FUNC_FUNCTION1);
        
    }
    cm_demo_printf("UART_CFG:\n dev=%d\n byte=%d\n parity=%d\n stop=%d\n flow=%d\n baudrate=%d\n is_lpuart=%d\n rx_buf=%d\n",
        dev,
        uart_cfg.byte_size,
        uart_cfg.parity,
        uart_cfg.stop_bit,
        uart_cfg.flow_ctrl,
        uart_cfg.baudrate,
        uart_cfg.is_lpuart,
        uart_cfg.rxrb_buf_size
    );

    cm_demo_printf("uart cm_uart_register_event ret:%d\n", cm_uart_register_event(dev, &event));
    cm_demo_printf("uart cm_uart_open ret:%d\n", cm_uart_open(dev, &uart_cfg));
    memset(&event, 0, sizeof(event));
}

void cm_test_uart_operation(EmbeddedCli *cli, char *args, void *context)
{
    const char *cmd = embeddedCliGetToken(args, 1);
    if (!cmd) {
        cm_demo_printf("Usage: UART <WRITE|READ|CLOSE|EVT_PRINT|GET_INFO|LENGTH|CLEAN_READ|IS_SENDING> [dev] [data]\n");
        return;
    }
    if (strcasecmp(cmd, "WRITE") == 0)
    {
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));
        const void *data = "1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890qwertyuiopasdfghjklzxc";
        int32_t len = 4000;
        // const char data[5] = {0x55,0x55,0x55,0x00,0x99};
        // int32_t len = 5;
        int32_t timeout = 20;
        // cm_log_printf(0, "uart cm_uart_write ret:%d\n", cm_uart_write(dev, data, len, timeout));
        // cm_log_printf(0, "uart cm_uart_is_sending ret:%d\n", cm_uart_is_sending(dev));
        cm_demo_printf("uart cm_uart_write ret:%d\n", cm_uart_write(dev, data, len, timeout));
    }else if (strcasecmp(cmd, "READ") == 0)//配合uart_cfg指令的ex回调或者无回调情况下，回显串口接收到的数据
    {
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));
        char data[500] = {0};                 //最多读取500字节
        int32_t len = 100;
        int32_t timeout = 20;
        cm_demo_printf("uart cm_uart_read ret:%d\n",  cm_uart_read( dev,  data,  len,  timeout));
        cm_uart_write(dev, data, len, timeout);
        cm_demo_printf("read data:%s\n",data);
    }else if (strcasecmp(cmd, "CLOSE") == 0)
    {
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));
        cm_demo_printf("uart cm_uart_close ret:%d\n",  cm_uart_close( dev));
    }else if(strcasecmp(cmd, "EVT_PRINT") == 0)
    {
        cm_demo_printf("param:%s, event:%d\n",uart_event_cb_param,uart_event);

        uart_event = 0;
        memset(uart_event_cb_param, 0, sizeof(uart_event_cb_param));    //打印后清空
    }else if(strcasecmp(cmd, "DATA_PRINT") == 0) //配合uart_cfg指令的noex回调情况下，回显串口接收到的数据
    {
        cm_demo_printf("callback_data:%s\n  len:%d\n",data,data_total_len);

        data_total_len = 0;
        memset(data, 0, sizeof(data));    //打印后清空
    }
    else if(strcasecmp(cmd, "GET_INFO") == 0){
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));
        cm_uart_cfg_t cfg = {0};
        cm_uart_get_cfg(dev, &cfg);
        cm_demo_printf("cfg byte_size:%d\n",cfg.byte_size);               /*!< 数据位，枚举*/
        cm_demo_printf("cfg parity:%d\n",cfg.parity);        /*!< 校验位，枚举*/
        cm_demo_printf("cfg stop_bit:%d\n",cfg.stop_bit);    /*!< 停止位，枚举*/
        cm_demo_printf("cfg flow_ctrl:%d\n",cfg.flow_ctrl);   /*!< 流控制，枚举*/
        cm_demo_printf("cfg baudrate:%d\n",cfg.baudrate);     /*!< 波特率，支持的波特率见本文件中标准波特率宏定义和特殊波特率宏定义*/
        cm_demo_printf("cfg is_lpuart:%d\n",cfg.is_lpuart);   /*!< 1:低功耗串口(仅支持115200以内的标准波特率(不支持76800))；0:普通串口*/
        cm_demo_printf("cfg rxrb_buf_size:%d\n",cfg.rxrb_buf_size);                         /*!< 失败次数 */
        // cm_demo_printf("cfg fc_high_threshold:%d\n",cfg.fc_high_threshold);         /*!< 用户配置时间参数 */
        // cm_demo_printf("cfg fc_low_threshold:%d\n",cfg.fc_low_threshold);   
        // cm_demo_printf("cfg rx_wakeup_en:%d\n",cfg.rx_wakeup_en);  
        // cm_demo_printf("cfg rx_wakeup_pin:%d\n",cfg.rx_wakeup_pin);  
    }
    else if (strcasecmp(cmd, "LENGTH") == 0)
    {
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));;
        cm_demo_printf("uart cm_uart_get_rxrb_data_len ret:%d\n",  cm_uart_get_rxrb_data_len( dev));
    }else if (strcasecmp(cmd, "CLEAN_READ") == 0)
    {
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));
        cm_demo_printf("uart cm_uart_clean ret:%d\n",  cm_uart_clean( dev));
    }
    else if (strcasecmp(cmd, "IS_SENDING") == 0)
    {
        cm_uart_dev_e dev = atoi(embeddedCliGetToken(args, 2));;
        cm_demo_printf("uart cm_uart_is_sending ret:%d\n",  cm_uart_is_sending( dev));
    }
    else{
        cm_demo_printf("param error!\n");
    }
}
