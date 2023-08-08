#ifndef __SFDB_H
#define __SFDB_H
#include "sfdb_port.h"

#define SFDB_OVERWRITE (1 << 0)  // Overwrite db if config not match
#define SFDB_SYNC      (1 << 1)  // Sync to disk after each write

#define SFDB_READ_ASC  0
#define SFDB_READ_DESC 1

#define SFDB_O_READ  (1 << 0)
#define SFDB_O_WRITE (1 << 1)
#define SFDB_O_CREATE (1 << 2)

#ifndef SF_LOG
#define SF_LOG(...)
#endif

/*  FORMAT DETAILS
**
**  OFFSET      SIZE        DESCRIPTION
**      0        8          MAGIC STRING "SFDBxxx\0"
**      8        1          RESV
**      9        1          RESV
**     10        1          RESV
**     11        1          RESV
**     12        4          current record index
**     16        4          current record count
**     20        4          max record number
**     24        4          record length
*/
typedef struct _db_hdr {
    char magic[8];
    uint8_t resv01[4];
    uint32_t record_index;
    uint32_t record_count;
    uint32_t max_record_num;
    uint32_t record_len;
} sfdb_hdr_t;

struct _sfdb;
typedef struct _db_fs {
    int (*op)(struct _sfdb *db, const char *path, int flags);  // Open file
    int (*cl)(void *fd);                                       // Close file
    int (*sy)(void *fd);                                       // Sync file
    size_t (*rd)(void *fd, void *buf, size_t len);             // Read file
    size_t (*wr)(void *fd, const void *buf, size_t len);       // Write file
    size_t (*sk)(void *fd, size_t offset);                     // Set file position
    int (*rm)(const char *path);                               // Delete file
} sfdb_fs_t;

extern sfdb_fs_t sfdb_fs;

enum { SFDB_STATE_CLOSED = 0, SFDB_STATE_OPENED };
typedef struct _sfdb {
    uint8_t state;
    const char *path;
    sfdb_hdr_t hdr;
    sfdb_fs_t *fs;
    void *fd;
    uint32_t flags;
} sfdb_t;

typedef struct _sfdb_info {
    uint32_t record_index;
    uint32_t record_count;
    uint32_t max_record_num;
    uint32_t record_len;
} sfdb_info_t;

typedef struct _sfdb_cfg {
    const char *path;
    uint32_t max_record_num;
    uint32_t record_len;
    int flags;
} sfdb_cfg_t;

int sfdb_open(sfdb_t *db, sfdb_cfg_t *cfg);
int sfdb_append(sfdb_t *db, uint8_t *buf, uint16_t sz);
int sfdb_read(sfdb_t *db, uint8_t *buf, uint32_t buf_sz, uint32_t offset, uint32_t num, uint8_t order);
int sfdb_read_info(sfdb_t *db, sfdb_info_t *info);
int sfdb_close(sfdb_t *db);
int sfdb_reset(sfdb_t *db);
int sfdb_delete(sfdb_t *db);
int sfdb_sync(sfdb_t *db);
#endif /* __SFDB_H */
