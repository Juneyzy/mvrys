#ifndef __CPSS_DEFS_H__
#define __CPSS_DEFS_H__

#include <assert.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#define __USE_GNU
#include <sched.h>
#include <pcre.h>
#include <syslog.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/signal.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <dirent.h>
#include <stdbool.h>
#include <regex.h>
#include <getopt.h>
//#include <libnet.h>
#include <pcap.h>

extern int errno;
#define ERRNO_EQUAL(e) (errno == e)

#include "rt_config.h"
#include "rt_common.h"
#include "rt_atomic.h"
#include "rt_bug.h"
#include "rt_errno.h"
#include "rt_mempool.h"
#include "rt_stdlib.h"
#include "rt_string.h"
#include "rt_sync.h"
#include "rt_endian.h"
#include "rt_time.h"
#include "rt_tmr.h"
#include "rt_task.h"
#include "rt_stack.h"
#include "rt_socket.h"
#include "rt_list.h"
#include "rt_timer.h"
#include "rt_yaml.h"
#include "rt_file.h"
#include "rt_logging.h"
#include "rt_util.h"
#include "rt_signal.h"
#include "rt_mq.h"
#include "rt_hash.h"
#include "rt_pool.h"
#include "rt_thrdpool.h"

#endif

