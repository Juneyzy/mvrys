#ifndef __YAML_COMMON_H__
#define __YAML_COMMON_H__

#include <assert.h>
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
#include <sys/types.h> /* for gettid(2) */
#include <sched.h>     /* for sched_setaffinity(2) */
#include <pcre.h>
#include <syslog.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/signal.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <dirent.h>

#include <yaml.h>
#include "yaml_utils_optimize.h"
#include "yaml_utils_str.h"
#include "yaml_utils_fs.h"

#endif
