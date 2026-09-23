#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "kw_protocol.h"
/*-------------------------------------------function---------------------------------------------*/
/* Validates JSON/AES/CRC and configured manufacturer/version; view borrows frame. */
kw_result_t kw_session_decode(const kw_protocol_config_t *config,
                              kw_protocol_workspace_t *workspace, const char *json,
                              size_t json_size, uint8_t *frame, size_t capacity,
                              kw_frame_view_t *view);
