#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "kaiwan/kaiwan_protocol.h"
/*-------------------------------------------function---------------------------------------------*/
/* 解开 JSON 并校验 AES、CRC、厂商和版本；view 借用 frame，使用期间不能覆盖它。 */
kaiwan_result_t kaiwan_session_decode(const kaiwan_protocol_config_t *config,
                              kaiwan_protocol_workspace_t *workspace, const char *json,
                              size_t json_size, uint8_t *frame, size_t capacity,
                              kaiwan_frame_view_t *view);
