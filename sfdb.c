/***************************************************************
 * @file           sfdb.c
 * @brief
 * @author         WKJay
 * @Version
 * @Date           2023-02-13
 ***************************************************************/
#include "sfdb.h"

#define FILE_HEADER "SFDB002"  // Simple file database

#define DB_HDR_OFFSET  0
#define DB_DATA_OFFSET 512
#define MAX_RECORD_LEN 512

static int seek_and_write(sfdb_t *db, uint8_t *buf, uint32_t offset, uint32_t sz) {
    int ret = -1;
    if (db->fs->sk(db->fd, offset) != offset) {
        SF_LOG("seek failed");
        return -1;
    }
    ret = db->fs->wr(db->fd, buf, sz);

    if (db->flags & SFDB_SYNC) db->fs->sy(db->fd);

    return ret;
}

static int seek_and_read(sfdb_t *db, uint8_t *buf, uint32_t offset, uint32_t sz) {
    if (db->fs->sk(db->fd, offset) != offset) {
        SF_LOG("seek failed");
        return -1;
    }
    return db->fs->rd(db->fd, buf, sz);
}

static int sfdb_new_db(sfdb_t *db, uint32_t max_record_num, uint32_t record_len) {
    SF_MEMCPY(db->hdr.magic, FILE_HEADER, sizeof(db->hdr.magic));
    db->hdr.max_record_num = max_record_num;
    db->hdr.record_len = record_len;

    if (seek_and_write(db, (uint8_t *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        return -1;
    }

    return 0;
}

static int sfdb_get_db_info(sfdb_t *db) {
    if (seek_and_read(db, (uint8_t *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        return -1;
    }
    return 0;
}

int sfdb_close(sfdb_t *db) {
    if (db->state != SFDB_STATE_OPENED) {
        SF_LOG("%s not opened", db->path);
        return -1;
    }
    db->fs->cl(db->fd);
    db->state = SFDB_STATE_CLOSED;
    return 0;
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
int sfdb_open(sfdb_t *db, sfdb_cfg_t *cfg) {
    uint8_t retry_cnt = 0;
    if (cfg->record_len > MAX_RECORD_LEN) {
        SF_LOG("record len:%d is larger than max record len:%d", cfg->record_len, MAX_RECORD_LEN);
        return -1;
    }
retry:
    SF_MEMSET(db, 0, sizeof(sfdb_t));
    db->fs = &sfdb_fs;
    db->path = cfg->path;
    db->flags = cfg->flags;

    if (db->fs->op(db, db->path, SFDB_O_READ | SFDB_O_WRITE) < 0) {
        SF_LOG("open file %s failed. try to create.", db->path);
        if (db->fs->op(db, db->path, SFDB_O_CREATE | SFDB_O_READ | SFDB_O_WRITE) < 0) {
            SF_LOG("create file %s failed.", db->path);
            return -1;
        }
        db->state = SFDB_STATE_OPENED;

        if (sfdb_new_db(db, cfg->max_record_num, cfg->record_len) < 0) {
            SF_LOG("create new database failed.");
            goto err_close_db;
        }
    } else {
        db->state = SFDB_STATE_OPENED;

        if (sfdb_get_db_info(db) < 0) {
            SF_LOG("get database information failed.");
            goto try_overwrite;
        }

        if (db->hdr.max_record_num != cfg->max_record_num) {
            SF_LOG("max record num not match.");
            goto try_overwrite;
        }

        if (db->hdr.record_len != cfg->record_len) {
            SF_LOG("record len not match");
            goto try_overwrite;
        }
    }
    return 0;

try_overwrite:
    if ((db->flags & SFDB_OVERWRITE) == 0) {
        goto err_close_db;
    }

    if (retry_cnt++ < 1) {
        SF_LOG("try to overwrite sfdb %s.", db->path);
    } else {
        SF_LOG(" overwrite sfdb %s failed.", db->path);
        goto err_close_db;
    }

    db->fs->cl(db->fd);
    db->state = SFDB_STATE_CLOSED;

    if (sfdb_delete(db) < 0) {
        SF_LOG("delete %s failed.", db->path);
        goto err_close_db;
    }

    goto retry;

err_close_db:
    SF_LOG("open %s failed.", db->path);
    db->fs->cl(db->fd);
    db->state = SFDB_STATE_CLOSED;
    return -1;
}

int sfdb_append(sfdb_t *db, uint8_t *buf, uint16_t sz) {
    uint32_t offset = 0;
    if (sz != db->hdr.record_len) {
        SF_LOG("data size %d is invalid(should be %d).", sz, db->hdr.record_len);
        return -1;
    }

    if (db->fd < 0) {
        SF_LOG("invalid fd");
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
    if (seek_and_write(db, buf, offset, sz) != sz) {
        SF_LOG("write data failed.");
        return -1;
    }

    /* 写入头 */
    if (seek_and_write(db, (uint8_t *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        SF_LOG("update hdr failed.");
        return -1;
    }

    return 0;
}

/**
 * @brief    读数据库数据
 * @param    db     数据库对象
 * @param    buf    读取数据的缓冲区
 * @param    buf_sz 缓冲区大小
 * @param    offset 读取的起始偏移地址（从0开始，且0表示最新一条数据）
 * @param    num    读取的数据量
 * @param    order  读取顺序 SFDB_READ_ASC：从旧到新 SFDB_READ_DESC：从新到旧
 * @note
 * 通过该接口读取的数组第0个元素对应当前集合中最早存入的数据，数组下标最大的即为最近一次存入的数据
 * @return   成功：返回读取的数据量 失败：-1
 */
int sfdb_read(sfdb_t *db, uint8_t *buf, uint32_t buf_sz, uint32_t offset, uint32_t num, uint8_t order) {
    uint32_t data_count = 0, read_num = 0, start_index = 0, read_total = 0;
    if (db->fd < 0) {
        SF_LOG("invalid fd");
        return -1;
    }
    if (buf_sz < num * db->hdr.record_len) {
        SF_LOG("provided buffer size %d < %d (required).", buf_sz, num * db->hdr.record_len);
        return -1;
    }

    /* 获取存储量总数 */
    data_count = db->hdr.record_count;
    if (data_count > db->hdr.max_record_num) data_count = db->hdr.max_record_num;

    // 没有数据时直接返回
    if (data_count <= 0) return 0;
    if (offset >= data_count) return 0;
    if (num == 0) return 0;

    /* 根据提供的起始偏移和读数数量来计算实际能够读取的数量 */
    if ((offset + num) > data_count) num = data_count - offset;
    read_total = num;

    /* 计算起始读取索引 */
    if (order == SFDB_READ_ASC) {  // 从旧到新
        if (db->hdr.record_index < (db->hdr.record_count - offset - 1)) {
            start_index = db->hdr.record_index + offset + 1;
        } else {
            start_index = db->hdr.record_index + offset + 1 - db->hdr.record_count;
        }
    } else {                                              // 从新到旧
        if (db->hdr.record_index < (offset + num - 1)) {  // 存在一部分数据在底部
            start_index = db->hdr.record_count + db->hdr.record_index - (offset + num - 1);
        } else {
            start_index = db->hdr.record_index - (offset + num - 1);
        }
    }

    while (num) {
        uint32_t read_offset = 0, read_len = 0;
        read_offset = DB_DATA_OFFSET + start_index * db->hdr.record_len;

        if (start_index + num > db->hdr.max_record_num - 1) {
            // 当前索引在数据底部，并且有一部分数据在头部，需要二次读取
            read_num = db->hdr.max_record_num - start_index;
        } else {
            read_num = num;
        }

        read_len = read_num * db->hdr.record_len;

        if (seek_and_read(db, buf, read_offset, read_len) != read_len) {
            SF_LOG("read data failed.");
            return -1;
        }

        num -= read_num;
        start_index += read_num;
        if (start_index >= db->hdr.max_record_num) start_index = 0;
        buf += read_len;
    }
    return read_total;
}

/**
 * @brief    读数据库信息
 * @param    db    数据库对象
 * @param    info  数据库信息结构体
 * @return   成功 0 失败 -1
 */
int sfdb_read_info(sfdb_t *db, sfdb_info_t *info) {
    if (db->state != SFDB_STATE_OPENED) {
        SF_LOG("db %s not opened.", db->path);
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
    if (db->state != SFDB_STATE_OPENED) {
        SF_LOG("db %s not opened.", db->path);
        return -1;
    }

    db->hdr.record_index = 0;
    db->hdr.record_count = 0;

    /* 更新头 */
    if (seek_and_write(db, (uint8_t *)&db->hdr, 0, sizeof(db->hdr)) != sizeof(db->hdr)) {
        SF_LOG("update hdr failed.");
        return -1;
    }

    return 0;
}

/**
 * @brief    删除数据库
 * @param    db   数据库对象
 * @return   成功：0 失败：-1
 */
int sfdb_delete(sfdb_t *db) { return unlink(db->path); }

/**
 * @brief    同步数据库缓存到文件
 * @param    db  数据库对象
 * @return   成功：0 失败：-1
 */
int sfdb_sync(sfdb_t *db) { return db->fs->sy(db->fd); }
