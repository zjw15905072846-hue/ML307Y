#ifndef __CM_UART_DEMO_H__
#define __CM_UART_DEMO_H__
#include "cm_os.h"
#include "cm_demo_common.h"

void cm_demo_uart(void);
void cm_demo_printf (char *str, ...);
void cm_test_uart_close(char **cmd, int len);

#endif
