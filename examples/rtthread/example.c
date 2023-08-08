/***************************************************************
* @file           example.c
* @brief          
* @author         WKJay
* @Version        
* @Date           2023-08-07
***************************************************************/
#include <rtthread.h>
#include "sfdb.h"
#include <stdlib.h>

#define TEST_FILE_PATH  "/sdcard/test.sdb"
#define MAX_RECORD_NUM  10000
#define RECORD_LEN      32
#define TEST_APPEND_NUM 10100
void sdcard_test(void *param) {
    sfdb_t sfdb;
    sfdb_cfg_t cfg = {0};
    uint8_t record[32] = {0};
    uint32_t tick_old, duration;

    cfg.path = TEST_FILE_PATH;
    cfg.flags = SFDB_SYNC | SFDB_OVERWRITE;
    cfg.record_len = RECORD_LEN;
    cfg.max_record_num = MAX_RECORD_NUM;

    if (sfdb_open(&sfdb, &cfg) < 0) {
        return;
    }

    for (int i = 0; i < TEST_APPEND_NUM; i++) {
        snprintf((char *)record, sizeof(record), "sfdb test record %d", i + 1);
        tick_old = rt_tick_get();
        if (sfdb_append(&sfdb, record, sizeof(record)) < 0) {
            SF_LOG("append %d record failed.", i + 1);
            sfdb_close(&sfdb);
            return;
        }
        duration = rt_tick_get() - tick_old;
        SF_LOG("append %5d data cost %4d ms", i + 1, duration);
    }

    sfdb_close(&sfdb);
}

int sfdb_test(void) {
    rt_thread_t tid = rt_thread_create("sd_test", sdcard_test, NULL, 4096, 12, 5);
    if (tid) {
        rt_thread_startup(tid);
    }
    return 0;
}
MSH_CMD_EXPORT(sfdb_test, sfdb_test);

int sfdb_read_test(int argc, char *argv[]) {
    int ret = -1, print_index = 0;
    uint32_t tick_old, duration, offset;
    uint16_t number;
    uint8_t order;
    sfdb_t sfdb;
    sfdb_cfg_t cfg;

    if (argc != 1 && argc != 4) {
        printf("invalid arguments,please input:\r\n");
        printf("1. sfdb_read\r\n");
        printf("2. sfdb_read [offset] [number] [order(0:asc 1:dsc)]\r\n");
        return -1;
    }

    if (argc == 4) {
        offset = atoi(argv[1]);
        number = atoi(argv[2]);
        order = atoi(argv[3]) ? SFDB_READ_DESC : SFDB_READ_ASC;
    }

    cfg.path = TEST_FILE_PATH;
    cfg.max_record_num = MAX_RECORD_NUM;
    cfg.record_len = RECORD_LEN;

    tick_old = rt_tick_get();
    if (sfdb_open(&sfdb, &cfg) < 0) {
        SF_LOG("open %s failed.", TEST_FILE_PATH);
        return -1;
    }
    duration = rt_tick_get() - tick_old;
    SF_LOG("open database cost %4d ms", duration);

    if (argc == 1) {
        SF_LOG("db index:%d", sfdb.hdr.record_index);
        SF_LOG("db count:%d", sfdb.hdr.record_count);
        SF_LOG("record len:%d", sfdb.hdr.record_len);
    } else {
        if (number > 100) number = 100;
        uint32_t data_sz = number * sfdb.hdr.record_len;
        uint8_t *data_buf = malloc(data_sz);
        if (data_buf == NULL) {
            SF_LOG("allocate memory failed.");
            goto close_db;
        }
        tick_old = rt_tick_get();
        ret = sfdb_read(&sfdb, data_buf, data_sz, offset, number, order);
        duration = rt_tick_get() - tick_old;
        SF_LOG("read %d data from %d cost %4d ms", ret, offset, duration);
        SF_LOG("------------DATA------------");
        for (int i = 0; i < ret; i++) {
            if (order == SFDB_READ_ASC) {
                print_index = i;
            } else {
                print_index = ret - i - 1;
            }
            SF_LOG("%-5d:%s", offset + i + 1, (char *)&data_buf[print_index * sfdb.hdr.record_len]);
        }
        SF_LOG("----------DATA END----------");
        free(data_buf);
    }

close_db:
    tick_old = rt_tick_get();
    sfdb_close(&sfdb);
    duration = rt_tick_get() - tick_old;
    SF_LOG("close database cost %4d ms", duration);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(sfdb_read_test, sfdb_read, sfdb read data);

