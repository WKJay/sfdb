/***************************************************************
 * @file           sfdb_port.c
 * @brief
 * @author         WKJay
 * @Version
 * @Date           2023-07-20
 ***************************************************************/
#include "sfdb.h"

#if defined(__RTTHREAD__)
static int fs_open(sfdb_t *db, const char *path, int flags) {
    int oflags = O_RDWR;
    if (flags & SFDB_O_CREATE) oflags |= O_CREAT;
    db->fd = (void *)open(path, oflags);

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
#elif defined(__unix__)
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

#endif
