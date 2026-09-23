/*------------------------------------------includes--------------------------------------------*/
#include "alarm_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define CHECK(x) assert(x)

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    al_image_t disk;
    int read_status;
    bool fail_write;
    unsigned writes;
    unsigned sends;
    uint16_t seq;
    uint32_t id;
} fake_t;

/*-------------------------------------------variables-------------------------------------------*/
static fake_t f;
static al_store_t store;
static al_reporter_t report;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : read_image
* Description    : 读取模拟持久镜像
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static int read_image(void *u, void *data, size_t size)
{
    fake_t *p = u;
    if (p->read_status)
    {
        return p->read_status;
    }
    memcpy(data, &p->disk, size);
    return 0;
}

/*******************************************************************************
* Function Name  : write_image
* Description    : 模拟原子写入及失败
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static bool write_image(void *u, const void *data, size_t size)
{
    fake_t *p = u;
    if (p->fail_write)
    {
        return false;
    }
    memcpy(&p->disk, data, size);
    p->read_status = 0;
    p->writes++;
    return true;
}

/*******************************************************************************
* Function Name  : send_event
* Description    : 记录实际请求发送的事件和序号
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static bool send_event(void *u, const al_event_t *e, uint16_t seq)
{
    fake_t *p = u;
    p->sends++;
    p->id = e->id;
    p->seq = seq;
    return true;
}

/*******************************************************************************
* Function Name  : setup
* Description    : 建立空存储及可靠上报上下文
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static void setup(void)
{
    al_storage_port_t p = {&f, read_image, write_image};
    memset(&f, 0, sizeof(f));
    f.read_status = AL_STORAGE_EMPTY;
    CHECK(al_store_open(&store, 0x41420101U, &p) == AL_OK);
    al_reporter_init(&report, &store, send_event, &f, 10000, 5000, 30000);
}

/*******************************************************************************
* Function Name  : test_key
* Description    : 覆盖消抖、短按、长按保持、释放和Tick回绕
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static void test_key(void)
{
    al_key_t k = {0};
    CHECK(al_key_sample(&k, true, 0, 30) == AL_KEY_NONE);
    CHECK(al_key_sample(&k, false, 15, 30) == AL_KEY_NONE);
    CHECK(al_key_sample(&k, true, 20, 30) == AL_KEY_NONE);
    CHECK(al_key_sample(&k, true, 49, 30) == AL_KEY_NONE);
    CHECK(al_key_sample(&k, true, 50, 30) == AL_KEY_PRESS);
    CHECK(al_key_sample(&k, true, 100000, 30) == AL_KEY_NONE);
    CHECK(al_key_sample(&k, false, 100001, 30) == AL_KEY_NONE);
    CHECK(al_key_sample(&k, false, 100031, 30) == AL_KEY_RELEASE);
    memset(&k, 0, sizeof(k));
    al_key_sample(&k, true, 0xfffffff0U, 30);
    CHECK(al_key_sample(&k, true, 14, 30) == AL_KEY_PRESS);
}

/*******************************************************************************
* Function Name  : test_indicator
* Description    : 覆盖全部阶段及提前成功仍完成前四秒
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static void test_indicator(void)
{
    al_indicator_t v;
    al_pattern_t p = {1000, 3000, 30000, 250};
    al_output_t o;
    uint32_t times[] = {500, 2000, 4000, 10000, 33999, 34000};
    unsigned i;
    for (i = 0; i < 6; i++)
    {
        al_indicator_start(&v, &p, 17, 0);
        o = al_indicator_tick(&v, times[i] - 1);
        CHECK(o.active);
        al_indicator_ack(&v, 17);
        o = al_indicator_tick(&v, times[i]);
        CHECK(o.active == (times[i] < 4000));
    }
    al_indicator_start(&v, &p, 17, 0);
    for (i = 0; i < 12; i++)
    {
        o = al_indicator_tick(&v, 1000 + i * 250);
        CHECK(o.active && o.led == (i % 2 == 0) && o.buzzer == o.led);
    }
    al_indicator_start(&v, &p, 17, 0);
    o = al_indicator_tick(&v, 999);
    CHECK(o.led && !o.buzzer && o.active);
    o = al_indicator_tick(&v, 1000);
    CHECK(o.led && o.buzzer);
    o = al_indicator_tick(&v, 1250);
    CHECK(!o.led && !o.buzzer && o.active);
    o = al_indicator_tick(&v, 3750);
    CHECK(!o.led && !o.buzzer);
    o = al_indicator_tick(&v, 4000);
    CHECK(o.led && !o.buzzer);
    al_indicator_ack(&v, 16);
    CHECK(al_indicator_tick(&v, 10000).led);
    al_indicator_ack(&v, 17);
    CHECK(!al_indicator_tick(&v, 10000).active);
    al_indicator_start(&v, &p, 18, 500);
    al_indicator_ack(&v, 17);
    CHECK(al_indicator_tick(&v, 4500).led);
    CHECK(al_indicator_tick(&v, 34499).active);
    CHECK(!al_indicator_tick(&v, 34500).active);
    al_indicator_start(&v, &p, 19, 0xfffffff0U);
    CHECK(al_indicator_tick(&v, 984).buzzer);
    al_indicator_stop(&v);
    o = al_indicator_tick(&v, 985);
    CHECK(!o.active && !o.led && !o.buzzer);
}

/*******************************************************************************
* Function Name  : test_store
* Description    : 覆盖掉电恢复、满队列、写入失败、身份及CRC隔离
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static void test_store(void)
{
    al_event_t e = {0};
    uint32_t id;
    unsigned i;
    al_store_t reopened;
    al_storage_port_t p = {&f, read_image, write_image};
    setup();
    f.fail_write = true;
    CHECK(al_store_enqueue(&store, &e, &id) == AL_ERR_STORAGE);
    CHECK(id == 0 && store.image.count == 0);
    CHECK(al_store_pending(NULL) == AL_ERR_NOT_READY);
    CHECK(al_store_pending(&store) == 0);
    f.fail_write = false;
    for (i = 0; i < AL_CAPACITY; i++)
    {
        CHECK(al_store_enqueue(&store, &e, &id) == AL_OK);
        CHECK(id == i + 1);
    }
    CHECK(al_store_enqueue(&store, &e, &id) == AL_ERR_FULL);
    CHECK(id == 0);
    CHECK(al_store_pending(&store) == AL_CAPACITY);
    CHECK(al_store_open(&reopened, 0x41420101U, &p) == AL_OK);
    CHECK(reopened.image.count == AL_CAPACITY);
    i = f.writes;
    CHECK(al_store_open(&reopened, 22, &p) == AL_ERR_FOREIGN);
    CHECK(f.writes == i);
    f.disk.events[0].id ^= 1;
    CHECK(al_store_open(&reopened, 0x41420101U, &p) == AL_ERR_CORRUPT);
    CHECK(f.writes == i);
    f.read_status = AL_ERR_STORAGE;
    CHECK(al_store_open(&reopened, 0x41420101U, &p) == AL_ERR_STORAGE);
    CHECK(f.writes == i);
}

/*******************************************************************************
* Function Name  : test_ack_retry
* Description    : 覆盖序号匹配、超时退避、失败持久化和旧回执
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static void test_ack_retry(void)
{
    al_event_t e = {0};
    uint32_t a;
    uint32_t b;
    uint32_t done;
    uint16_t old;
    setup();
    CHECK(al_store_enqueue(&store, &e, &a) == 0);
    CHECK(al_store_enqueue(&store, &e, &b) == 0);
    al_reporter_poll(&report, true, 0);
    CHECK(f.sends == 1 && f.id == a);
    old = f.seq;
    CHECK(al_reporter_ack(&report, old + 1, 0, 1, &done) == AL_ERR_STALE && done == 0);
    al_reporter_poll(&report, true, 10000);
    al_reporter_poll(&report, true, 14999);
    CHECK(f.sends == 1);
    al_reporter_poll(&report, true, 15000);
    CHECK(f.sends == 2 && f.seq != old);
    f.fail_write = true;
    CHECK(al_reporter_ack(&report, old, 0, 15002, &done) == AL_ERR_STORAGE && done == 0);
    CHECK(store.image.count == 2);
    f.fail_write = false;
    CHECK(al_reporter_ack(&report, old, 0, 15003, &done) == 0 && done == a);
    CHECK(al_reporter_ack(&report, f.seq, 0, 15004, &done) == AL_ERR_STALE);
    al_reporter_poll(&report, true, 15005);
    CHECK(f.id == b && f.sends == 3);
    CHECK(al_reporter_ack(&report, old, 0, 15005, &done) == AL_ERR_STALE);
    CHECK(al_reporter_ack(&report, f.seq, 1, 15006, &done) == AL_ERR_REJECTED);
    CHECK(store.image.count == 1);
    al_reporter_poll(&report, true, 20006);
    CHECK(f.sends == 4);
    CHECK(al_reporter_ack(&report, f.seq, 0, 20007, &done) == 0 && done == b);
    CHECK(store.image.count == 0);
}

/*******************************************************************************
* Function Name  : test_sequence_exhaustion
* Description    : 序号不回绕冒认旧回执
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
static void test_sequence_exhaustion(void)
{
    uint16_t s;
    setup();
    store.image.next_sequence = 65536;
    CHECK(al_store_sequence(&store, &s) == AL_ERR_EXHAUSTED);
}

/*******************************************************************************
* Function Name  : main
* Description    : 执行主机回归测试
* Input          : 见函数签名；测试函数无参数
* Output         : 测试断言与模拟状态
* Return         : 见函数签名
* Attention      : 仅主机测试，不访问硬件
*******************************************************************************/
int main(void)
{
    test_key();
    test_indicator();
    test_store();
    test_ack_retry();
    test_sequence_exhaustion();
    puts("alarm_core: 5 suites passed");
    return 0;
}
