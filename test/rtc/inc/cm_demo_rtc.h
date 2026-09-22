#ifndef __CM_DEMO_RTC_H__
#define __CM_DEMO_RTC_H__

#include "embedded_cli.h"

/**
 *  @brief 注册CLI命令并初始化RTC测试
 *
 *  @param [in] cli CLI句柄
 *  @return None
 */
void cm_test_rtc(EmbeddedCli *cli, char *args, void *context);

#endif
