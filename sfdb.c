/***************************************************************
 * @file           sfdb.c
 * @brief
 * @author         WKJay
 * @Version
 * @Date           2023-02-13
 ***************************************************************/
#include <rtthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include "sfdb.h"

#define FILE_HEADER "SFDB001"  // Simple file database

#define DB_HDR_OFFSET  0
#define DB_DATA_OFFSET 512
#define MAX_RECORD_LEN 512

#define DBG_ENABLE
#define DBG_SECTION_NAME "sfdb"
#define DBG_LEVEL        DBG_INFO
#define DBG_COLOR
#include <rtdbg.h>

static int seek_and_write(int fd, uint8_t *buf, uint32_t offset, uint32_t sz) {
    int ret = -1;
    if (lseek(fd, offset, SEEK_SET) < 0) return -1;
    ret = write(fd, buf, sz);
    fsync(fd);
    return ret;
}

static int seek_and_read(int fd, uint8_t *buf, uint32_t offset, uint32_t sz) {
    if (lseek(fd, offset, SEEK_SET) < 0) return -1;
    return read(fd, buf, sz);
}

static int sfdb_new_db(sfdb_t *db, uint32_t max_record_num, uint32_t record_len) {
    rt_memcpy(db->hdr.magic, FILE_HEADER, sizeof(db->hdr.magic));
    db->hdr.max_record_num = max_record_num;
    db->hdr.record_len = record_len;
    rt_memset(db->page_data, 0xff, sizeof(db->page_data));
    rt_memcpy(db->page_data, &db->hdr, sizeof(db->hdr));

    if (seek_and_write(db->fd, db->page_data, 0, sizeof(db->page_data)) != sizeof(db->page_data)) {
        return -1;
    }

    return 0;
}

static int sfdb_get_db_info(sfdb_t *db) {
    if (seek_and_read(db->fd, (void *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        return -1;
    }
    return 0;
}

int sfdb_close(sfdb_t *db) {
    if (close(db->fd) == 0) {
        db->fd = -1;
        return 0;
    }
    return -1;
}

/**
 * @brief    创建一个SFDB
 * @param    path:              数据库路径
 * @param    db:                数据库结构体
 * @param    max_record_num:    最大数据量
 * @param    record_len:        单条数据长度
 * @param    overwrite:         当 record_len 和 max_record_num 参数和数据库保存的参数不匹配时
 *                              若该参数为1则进行覆盖
 * @return   return
 */
int sfdb_open(const char *path, sfdb_t *db, uint32_t max_record_num, uint32_t record_len,
              uint8_t overwrite) {
    uint8_t retry_cnt = 0;
    if (record_len > MAX_RECORD_LEN) {
        LOG_E("record len:%d is larger than max record len:%d", record_len, MAX_RECORD_LEN);
        return -1;
    }
retry:
    rt_memset(db, 0, sizeof(sfdb_t));
    db->fd = -1;

    db->fd = open(path, O_RDWR);
    if (db->fd < 0) {
        LOG_W("open file %s failed. try to create.", path);
        db->fd = open(path, O_CREAT | O_RDWR);
        if (db->fd < 0) {
            LOG_E("create file %s failed.", path);
            return -1;
        }

        if (sfdb_new_db(db, max_record_num, record_len) < 0) {
            LOG_E("create new database failed.");
            goto err_close_db;
        }
    } else {
        if (sfdb_get_db_info(db) < 0) {
            LOG_W("get database information failed.");
            goto overwrite;
        }

        if (db->hdr.max_record_num != max_record_num) {
            LOG_W("max record num not match.");
            goto try_overwrite;
        }

        if (db->hdr.record_len != record_len) {
            LOG_W("record len not match");
            goto try_overwrite;
        }
    }

    return 0;
try_overwrite:
    if (overwrite == 0) {
        goto err_close_db;
    }
overwrite:
    if (db->fd > 0) {
        close(db->fd);
        db->fd = -1;
    }

    retry_cnt++;
    if (retry_cnt > 3) {
        goto err_close_db;
    }

    LOG_W("try to overwrite sfdb %s - %d", path, retry_cnt);
    if (sfdb_delete(path) < 0) {
        LOG_E("delete %s failed.", path);
        goto err_close_db;
    }
    rt_thread_mdelay(10);
    goto retry;
err_close_db:
    LOG_E("open %s failed.");
    if (db->fd > 0) {
        close(db->fd);
        db->fd = -1;
    }
    return -1;
}

int sfdb_append(sfdb_t *db, uint8_t *buf, uint16_t sz) {
    uint32_t offset = 0;
    if (sz != db->hdr.record_len) {
        LOG_E("data size %d is invalid(should be %d).", sz, db->hdr.record_len);
        return -1;
    }

    if (db->fd < 0) {
        LOG_E("invalid fd");
        return -1;
    }

    /* 计算本次插入数据的索引 */
    if (db->hdr.record_index >= (db->hdr.max_record_num - 1)) {
        db->hdr.record_index = 0;
    } else {
        db->hdr.record_index++;
    }

    if (db->hdr.record_count < db->hdr.max_record_num) {
        db->hdr.record_count++;
    }

    // 当前为第一条时重置索引为0
    if (db->hdr.record_count == 1) db->hdr.record_index = 0;

    /* 根据索引计算数据存储的偏移 */
    offset = db->hdr.record_index * db->hdr.record_len + DB_DATA_OFFSET;

    /* 写入数据 */
    if (seek_and_write(db->fd, buf, offset, sz) != sz) {
        LOG_E("write data failed.");
        return -1;
    }

    /* 写入头 */
    if (seek_and_write(db->fd, (uint8_t *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        LOG_E("update hdr failed.");
        return -1;
    }

    return 0;
}

/**
 * @brief    读数据库数据
 * @param offset: 读取的起始偏移地址（从0开始，且0表示最新一条数据）
 * @note
 * 通过该接口读取的数组第0个元素对应当前集合中最早存入的数据，数组下标最大的即为最近一次存入的数据
 * @return   return
 */
int sfdb_read(sfdb_t *db, uint8_t *buf, uint32_t buf_sz, uint32_t offset, uint32_t num) {
    uint32_t data_count = 0, read_num = 0, start_index = 0, read_total = 0;
    if (db->fd < 0) {
        LOG_E("invalid fd");
        return -1;
    }
    if (buf_sz < num * db->hdr.record_len) {
        LOG_E("provided buffer size %d < %d (required).", buf_sz, num * db->hdr.record_len);
        return -1;
    }

    /* 获取存储量总数 */
    data_count = db->hdr.record_count;
    if (data_count > db->hdr.max_record_num) data_count = db->hdr.max_record_num;

    // 没有数据时直接返回
    if (data_count <= 0) return 0;
    if (num == 0) return 0;

    /* 根据提供的起始偏移和读数数量来计算实际能够读取的数量 */
    if (num > data_count) num = data_count;
    if ((offset + num) > data_count) num = data_count - offset;
    read_total = num;

    /* 计算起始读取索引 */
    if (db->hdr.record_index < (offset + num - 1)) {  // 存在一部分数据在底部
        start_index = db->hdr.max_record_num + db->hdr.record_index - (offset + num - 1);
    } else {
        start_index = db->hdr.record_index - (offset + num - 1);
    }

    while (num) {
        uint32_t read_offset = 0, read_len = 0;
        read_offset = DB_DATA_OFFSET + start_index * db->hdr.record_len;
        if (start_index + num > db->hdr.max_record_num - 1) {  // 有一部分数据在头部，需要二次读取
            read_num = db->hdr.max_record_num - start_index;
        } else {
            read_num = num;
        }
        read_len = read_num * db->hdr.record_len;

        if (seek_and_read(db->fd, buf, read_offset, read_len) != read_len) {
            LOG_E("read data failed.");
            return -1;
        }

        num -= read_num;
        start_index += read_num;
        if (start_index >= db->hdr.max_record_num) start_index = 0;
        buf += read_len;
    }
    return read_total;
}

int sfdb_read_info(sfdb_t *db, sfdb_info_t *info) {
    if (db->fd < 0) {
        LOG_E("invalid fd");
        return -1;
    }

    info->record_index = db->hdr.record_index;
    info->record_count = db->hdr.record_count;
    info->max_record_num = db->hdr.max_record_num;
    info->record_len = db->hdr.record_len;

    return 0;
}

/**
 * @brief    重置数据库（清除数据库中的数据）
 * @param    db: 数据库对象
 * @return   成功：0 失败：-1
 */
int sfdb_reset(sfdb_t *db) {
    if (db->fd < 0) {
        LOG_E("invalid fd");
        return -1;
    }

    db->hdr.record_index = 0;
    db->hdr.record_count = 0;

    /* 更新头 */
    if (seek_and_write(db->fd, (uint8_t *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        LOG_E("update hdr failed.");
        return -1;
    }

    return 0;
}

/**
 * @brief    删除数据库（后续需要修改为传入sfdb对象删除，对象中需要加入path）
 * @param    param
 * @return   return
 */
int sfdb_delete(const char *path) { return unlink(path); }
/**************************************** TEST API ****************************************/
// #define TEST_FILE_PATH "/sdcard/testFile.sdb"

// void sfdb_test(void) {
//     int fd = -1;
//     uint8_t buffer[64];
//     uint32_t tick_old = 0, duration = 0;
//     rt_kprintf("-----open file\r\n");
//     fd = open(TEST_FILE_PATH, O_CREAT | O_RDWR);
//     rt_kprintf("-----open file end\r\n");
//     if (fd < 0) return;

//     for (int i = 0; i < 10; i++) {
//         for (int j = 0; j < sizeof(buffer); j++) {
//             buffer[j] = i;
//         }
//         rt_kprintf("-----write  cnt %d\r\n", i);
//         seek_and_write(fd, buffer, i * sizeof(buffer), sizeof(buffer));
//         rt_kprintf("-----write cnt %d end\r\n", i);
//     }

//     for (int j = 0; j < sizeof(buffer); j++) {
//         buffer[j] = 0x33;
//     }
//     rt_kprintf("-----modify file\r\n");
//     seek_and_write(fd, buffer, 0, sizeof(buffer));
//     rt_kprintf("-----modify file end\r\n");

//     rt_kprintf("-----close file\r\n");
//     close(fd);
//     rt_kprintf("-----close file end\r\n");
// }
// MSH_CMD_EXPORT(sfdb_test, sfdb_test)

#include <stdlib.h>
#define TEST_FILE_PATH  "/sdcard/test.sdb"
#define MAX_RECORD_NUM  10000
#define RECORD_LEN      32
#define TEST_APPEND_NUM 10100
void sdcard_test(void *param) {
    sfdb_t sfdb;
    uint8_t record[32] = {0};
    uint32_t tick_old, duration;
    if (sfdb_open(TEST_FILE_PATH, &sfdb, MAX_RECORD_NUM, RECORD_LEN, 0) < 0) {
        return;
    }

    for (int i = 0; i < TEST_APPEND_NUM; i++) {
        snprintf((char *)record, sizeof(record), "sfdb test record %d", i + 1);
        tick_old = rt_tick_get();
        if (sfdb_append(&sfdb, record, sizeof(record)) < 0) {
            LOG_E("append %d record failed.", i + 1);
            sfdb_close(&sfdb);
            return;
        }
        duration = rt_tick_get() - tick_old;
        LOG_I("append %5d data cost %4d ms", i + 1, duration);
    }

    sfdb_close(&sfdb);
    while (1) {
        rt_thread_mdelay(10);
    }
}

int sfdb_test_init(void) {
    rt_thread_t tid = rt_thread_create("sd_test", sdcard_test, NULL, 4096, 12, 5);
    if (tid) {
        rt_thread_startup(tid);
    }
    return 0;
}
MSH_CMD_EXPORT(sfdb_test_init, sfdb_test_init);

int sfdb_read_test(int argc, char *argv[]) {
    int ret = -1;
    uint32_t tick_old, duration, offset;
    uint16_t number;
    sfdb_t sfdb;

    if (argc != 1 && argc != 3) {
        printf("invalid arguments,please input:\r\n");
        printf("1. sfdb_read\r\n");
        printf("2. sfdb_read [offset] [number]\r\n");
        return -1;
    }

    if (argc == 3) {
        offset = atoi(argv[1]);
        number = atoi(argv[2]);
    }

    tick_old = rt_tick_get();
    if (sfdb_open(TEST_FILE_PATH, &sfdb, MAX_RECORD_NUM, RECORD_LEN, 0) < 0) {
        LOG_E("open %s failed.", TEST_FILE_PATH);
        return -1;
    }
    duration = rt_tick_get() - tick_old;
    LOG_I("open database cost %4d ms", duration);

    if (argc == 1) {
        LOG_I("db index:%d", sfdb.hdr.record_index);
        LOG_I("db count:%d", sfdb.hdr.record_count);
        LOG_I("record len:%d", sfdb.hdr.record_len);
    } else {
        if (number > 100) number = 100;
        uint32_t data_sz = number * sfdb.hdr.record_len;
        uint8_t *data_buf = malloc(data_sz);
        if (data_buf == NULL) {
            LOG_E("allocate memory failed.");
            goto close_db;
        }
        tick_old = rt_tick_get();
        ret = sfdb_read(&sfdb, data_buf, data_sz, offset, number);
        duration = rt_tick_get() - tick_old;
        LOG_I("read %d data from %d cost %4d ms", ret, offset, duration);
        LOG_I("------------DATA------------");
        for (int i = 0; i < ret; i++) {
            LOG_I("%-5d:%s", offset + i + 1,
                  (char *)&data_buf[(ret - i - 1) * sfdb.hdr.record_len]);
        }
        LOG_I("----------DATA END----------");
        free(data_buf);
    }

close_db:
    tick_old = rt_tick_get();
    sfdb_close(&sfdb);
    duration = rt_tick_get() - tick_old;
    LOG_I("close database cost %4d ms", duration);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(sfdb_read_test, sfdb_read, sfdb read data);
