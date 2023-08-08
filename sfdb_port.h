#ifndef __SFDB_PORT_H
#define __SFDB_PORT_H

#if defined(__RTTHREAD__)
#include <rtthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#define SF_MEMCPY rt_memcpy
#define SF_MEMSET rt_memset

#define SF_LOG(format, ...) rt_kprintf("[SFDB]:" format "\r\n", ##__VA_ARGS__)

#else

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

#endif

#endif /* __SFDB_PORT_H */
