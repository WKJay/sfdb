# Simple File DataBase - 简单文件数据库

## Simple and Fast ⚡

一个 **简单** 的文件型数据库，使用 **简单**，移植 **简单**，功能 **简单**，原理 **简单**，一切都很 **简单**，一切都很 **快**。

适用于 **固定长度** 的 **记录型** 数据存储，类似于时序数据库，可用于存储 **历史记录**、**报警记录**、**日志** 等。

- 使用文件进行存储
- 简单的数据写入接口
- 支持顺序与倒序查询
- 支持从任意条数开始查询
- 支持清空（重置）数据库

**由于设计为存储记录型数据，所以不支持数据修改，随机插入，随机删除等功能。**

# 起步

## 打开数据库

###  创建数据库操作对象

定义 `sfdb_t` 结构的数据库句柄以及 `sfdb_cfg_t` 结构的配置句柄。配置信息如下：

```c

typedef struct _sfdb_cfg {
    const char *path;           // 数据库文件路径
    uint32_t max_record_num;    // 最大记录数
    uint32_t record_len;        // 记录长度
    int flags;                  // 打开数据库的标志位
} sfdb_cfg_t;

```

配置参数中的 `flags` 为打开数据库的标志位，可选值如下：

|参数|说明|
|-|-|
|SFDB_OVERWRITE|以覆盖逻辑打开,此时配置的文件路径若已有文件且非数据库，或与当前设置的最大记录数与记录长度不同，则将清除原有文件并重新建立数据库|
|SFDB_SYNC|打开自动同步（每次写入都会将数据写入存储介质）|

### 打开

使用 `int sfdb_open(sfdb_t *db, sfdb_cfg_t *cfg) ` 接口打开数据库，若数据库不存在，则会自动创建，若启动了覆盖逻辑，则在非有效数据库或数据库配置不匹配时将清除原有文件并重新建立数据库。

### 示例

```c

#define TEST_FILE_PATH  "/sdcard/test.sdb"
#define MAX_RECORD_NUM  10000
#define RECORD_LEN      32
#define TEST_APPEND_NUM 10100
    
sfdb_t sfdb;
sfdb_cfg_t cfg = {0};

cfg.path = TEST_FILE_PATH;
cfg.flags = SFDB_SYNC | SFDB_OVERWRITE;
cfg.record_len = RECORD_LEN;
cfg.max_record_num = MAX_RECORD_NUM;

sfdb_open(&sfdb, &cfg)；

```

## 写入数据

使用 `int sfdb_append(sfdb_t *db, uint8_t *buf, uint16_t sz)` 接口写入数据。`sz` 必须与配置中的 `record_len` 相同，否则会返回错误。

## 关闭数据库

使用 `int sfdb_close(sfdb_t *db)` 接口关闭数据库。

## 查询数据

使用 `int sfdb_read(sfdb_t *db, uint8_t *buf, uint32_t buf_sz, uint32_t offset, uint32_t num, uint8_t order)` 接口查询数据。sfdb 会从 `offset` 开始查询 `num` 条数据，查询结果将会存储在 `buf` 中，`buf_sz` 为 `buf` 的大小，`order` 为查询顺序，可选值如下：

|参数|说明|
|-|-|
|SFDB_READ_ASC|顺序读取（数据从旧到新）|
|SFDB_READ_DESC|倒序读取（数据从新到旧）|

# 接口列表

## 打开数据库

`int sfdb_open(sfdb_t *db, sfdb_cfg_t *cfg)`

若数据库不存在，则会自动创建，若存在并启动了覆盖逻辑，则在非有效数据库或数据库配置不匹配时将清除原有文件并重新建立数据库。

### 参数
|参数|说明|
|-|-|
|db|数据库对象|
|cfg|数据库配置参数|

### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

## 写入数据

`int sfdb_append(sfdb_t *db, uint8_t *buf, uint16_t sz)`

### 参数
|参数|说明|
|-|-|
|db|数据库对象|
|buf|待存储的数据|
|sz|带存储的数据大小|
### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

## 读取数据

`int sfdb_read(sfdb_t *db, uint8_t *buf, uint32_t buf_sz, uint32_t offset, uint32_t num, uint8_t order)`

### 参数
|参数|说明|
|-|-|
|db|数据库对象|
|buf|数据缓存地址|
|buf_sz|缓存地址大小|
|offset|读取偏移|
|num|读取数量|
|order|读取顺序|

### 读取顺序
|参数|说明|
|-|-|
|SFDB_READ_ASC|顺序读取（数据从旧到新）|
|SFDB_READ_DESC|倒序读取（数据从新到旧）|

**注意：从读取性能方面考虑，当以倒序读取时存入 `buf` 的数据仍然是顺序的，使用者需要手动处理 `buf` 中的数据顺序。**

**当数据 1-100 依次存入时，若以倒序从 `offset` 为 0 的位置读取 10 条数据，存入 `buf` 中，`buf` 中的数据顺序为 91 92 93 94 95 96 97 98 99 100 ，而非 100 99 98 97 96 95 94 93 92 91。**

**而在应用层可以通过索引倒转的形式实现数据顺序的倒转，如下：**

```c
ret = sfdb_read(&sfdb, data_buf, data_sz, offset, number, order);
for (int i = 0; i < ret; i++) {
    if (order == SFDB_READ_ASC) {
        print_index = i;
    } else {
        print_index = ret - i - 1;
    }
    SF_LOG("%-5d:%s", offset + i + 1, (char *)&data_buf[print_index * sfdb.hdr.record_len]);
}
```

### 返回值
|返回值|说明|
|-|-|
|>=0|读取的有效条数|
|-1|失败|



## 读取数据库基本信息

`int sfdb_read_info(sfdb_t *db, sfdb_info_t *info`

### 参数
|参数|说明|
|-|-|
|db|数据库对象|
|info|数据库信息对象|

### 数据库信息对象
|参数|说明|
|-|-|
|record_index|当前记录索引|
|record_count|当前总记录数|
|max_record_num|数据库最大记录数|
|record_len|单条记录长度|


### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

## 关闭数据库

`int sfdb_close(sfdb_t *db)`

### 参数
|参数|说明|
|-|-|
|db|数据库对象|

### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

## 清空数据库

`int sfdb_reset(sfdb_t *db)`

清空数据库所有数据并从头开始记录

### 参数
|参数|说明|
|-|-|
|db|数据库对象|

### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

## 删除数据库

`int sfdb_delete(sfdb_t *db)`

该接口将会删除数据库文件

### 参数
|参数|说明|
|-|-|
|db|数据库对象|

### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

## 手动同步 

`int sfdb_sync(sfdb_t *db)`

每次调用该接口将触发文件系统的sync操作，将数据写入存储介质。

### 参数
|参数|说明|
|-|-|
|db|数据库对象|

### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

# 移植

移植仅需参考 `sfdb_port.c` 实现 `sfdb_fs_t` 里的所有接口，以及在 `sfdb_port.h` 中包含所需要的头文件以及实现 `SF_MEMCPY `,`SF_MEMSET`,`SF_LOG`即可。

## sfdb_fs_t 需要实现的接口列表

### 打开文件
`int (*op)(struct _sfdb *db, const char *path, int flags)`

#### flags

|参数|说明|
|-|-|
|SFDB_O_READ|以可读方式打开|
|SFDB_O_WRITE|以可写方式打开|
|SFDB_O_CREATE|文件不存在则创建新文件|

#### 返回值
|返回值|说明|
|-|-|
|>=0|打开的文件描述符|
|-1|失败|

### 关闭文件
`int (*cl)(void *fd)`

#### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

### 同步文件
`int (*sy)(void *fd)`

#### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|

### 读数据
`size_t (*rd)(void *fd, void *buf, size_t len)`

#### 返回值
|返回值|说明|
|-|-|
|>=0|读取到的数据长度|
|-1|失败|

### 写数据
`size_t (*wr)(void *fd, const void *buf, size_t len)`

#### 返回值
|返回值|说明|
|-|-|
|>=0|实际写入的数据长度|
|-1|失败|

### 移动文件指针
`size_t (*sk)(void *fd, size_t offset)`

#### 返回值
|返回值|说明|
|-|-|
|>=0|当前的文件指针位置|
|-1|失败|

### 删除文件
`int (*rm)(const char *path)`

#### 返回值
|返回值|说明|
|-|-|
|0|成功|
|-1|失败|



## 移植示例

```c
// sfdb_port.c

static int fs_open(sfdb_t *db, const char *path, int flags) {
    int oflags = O_RDWR;
    if (flags & SFDB_O_CREATE) oflags |= O_CREAT;
    db->fd = (void *)open(path, oflags, S_IRUSR | S_IWUSR);

    if ((int)db->fd < 0) {
        return -1;
    } else {
        return 0;
    }
}
static int fs_close(void *fd) {
    if (fd >= 0) {
        close((int)fd);
        return 0;
    } else {
        SF_LOG("invalid fd %d, close failed", (int)fd);
        return -1;
    }
}
static int fs_sync(void *fd) { return fsync((int)fd); }
static size_t fs_read(void *fd, void *buf, size_t len) { return read((int)fd, buf, len); }
static size_t fs_write(void *fd, const void *buf, size_t len) { return write((int)fd, buf, len); }
static size_t fs_seek(void *fd, size_t offset) {
    int ret = 0;
    ret = lseek((int)fd, offset, SEEK_SET);
    if (ret < 0) return 0;

    return ret;
}
static int fs_remove(const char *path) { return unlink(path); }

sfdb_fs_t sfdb_fs = {
    .op = fs_open,
    .cl = fs_close,
    .sy = fs_sync,
    .rd = fs_read,
    .wr = fs_write,
    .sk = fs_seek,
    .rm = fs_remove,
};
```

```c
// sfdb_port.h

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define SF_MEMCPY memcpy
#define SF_MEMSET memset

#define SF_LOG(format, ...) printf("[SFDB]:" format "\r\n", ##__VA_ARGS__)
```

# 使用样例
```c
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
```
# 支持

![支持](./docs/_assets/wechat_support.png)

如果 SFDB 解决了你的问题，不妨扫描上面二维码请我喝杯咖啡吧 😄
