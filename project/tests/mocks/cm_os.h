#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
/*-------------------------------------------define---------------------------------------------*/
#define osOK 0
#define __ATOMIC_RELEASE 0
#define __ATOMIC_ACQUIRE 0
#define __ATOMIC_ACQ_REL 0
/* Single-thread callback ordering tests only; production uses GCC atomic builtins. */
#define __atomic_store_n(p, v, order) (*(p) = (v))
#define __atomic_load_n(p, order) (*(p))
#define __atomic_add_fetch(p, v, order) (*(p) += (v))
#define __atomic_exchange_n(p, v, order) mock_exchange((p), (v))
/*-------------------------------------------typedef---------------------------------------------*/
typedef void *osMessageQueueId_t;
typedef int osStatus_t;

typedef struct
{
    unsigned reserved;
} osMessageQueueAttr_t;

/*-------------------------------------------function---------------------------------------------*/
uint32_t mock_exchange(uint32_t *p, uint32_t value);
osMessageQueueId_t osMessageQueueNew(uint32_t count, uint32_t bytes,
                                     const osMessageQueueAttr_t *attr);
osStatus_t osMessageQueuePut(osMessageQueueId_t queue, const void *message, uint8_t priority,
                             uint32_t timeout);
osStatus_t osMessageQueueGet(osMessageQueueId_t queue, void *message, uint8_t *priority,
                             uint32_t timeout);
uint32_t osMessageQueueGetCount(osMessageQueueId_t queue);
osStatus_t osMessageQueueDelete(osMessageQueueId_t queue);
