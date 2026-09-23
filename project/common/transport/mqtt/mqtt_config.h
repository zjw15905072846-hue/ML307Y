#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stddef.h>
/*-------------------------------------------function---------------------------------------------*/
/* Internal bounded-text helper shared by config validation and the backend. */
int kw_cloud_text_length(const char *text, size_t capacity, size_t *length);
