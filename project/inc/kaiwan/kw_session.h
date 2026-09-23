#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "kaiwan/kw_protocol.h"
/*-------------------------------------------function---------------------------------------------*/
/* 解开 JSON 并校验 AES、CRC、厂商和版本；view 借用 frame，使用期间不能覆盖它。 */
kw_result_t kw_session_decode(const kw_protocol_config_t *config,
                              kw_protocol_workspace_t *workspace, const char *json,
                              size_t json_size, uint8_t *frame, size_t capacity,
                              kw_frame_view_t *view);
