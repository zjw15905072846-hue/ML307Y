#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    void *user;
    bool (*read)(void *user, uint32_t pin, bool *level);
    bool (*write)(void *user, uint32_t pin, bool level);
} gpio_if_t;

/*-------------------------------------------function---------------------------------------------*/
