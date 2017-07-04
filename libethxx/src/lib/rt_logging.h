#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <stdio.h>
#include <stdint.h>
#include <pcre.h>
#include "rt_errno.h"

/**
 * \brief ENV vars that can be used to set the properties for the logging module
 */
#define RT_LOG_ENV_LOG_LEVEL        "RT_LOG_LEVEL"
#define RT_LOG_ENV_LOG_OP_IFACE     "RT_LOG_OP_IFACE"
#define RT_LOG_ENV_LOG_FILE         "RT_LOG_FILE"
#define RT_LOG_ENV_LOG_FACILITY     "RT_LOG_FACILITY"
#define RT_LOG_ENV_LOG_FORMAT       "RT_LOG_FORMAT"
#define RT_LOG_ENV_LOG_OP_FILTER    "RT_LOG_OP_FILTER"

/**
 * \brief The various log levels
 */
typedef enum {
    RT_LOG_NOTSET = -1,
    RT_LOG_NONE = 0,
    RT_LOG_EMERGENCY,
    RT_LOG_ALERT,
    RT_LOG_CRITICAL,
    RT_LOG_ERROR,
    RT_LOG_WARNING,
    RT_LOG_NOTICE,
    RT_LOG_INFO,
    RT_LOG_DEBUG,
    RT_LOG_LEVEL_MAX,
} RTLogLevel;

/**
 * \brief The various output interfaces supported
 */
typedef enum {
    RT_LOG_OP_IFACE_CONSOLE,
    RT_LOG_OP_IFACE_FILE,
    RT_LOG_OP_IFACE_SYSLOG,
    RT_LOG_OP_IFACE_LINK,
    RT_LOG_OP_IFACE_MAX,
} RTLogOPIface;

/* The default log_format, if it is not supplied by the user */
#ifdef RELEASE
#define RT_LOG_DEF_LOG_FORMAT "%t - <%d> - "
#else
//#define RT_LOG_DEF_LOG_FORMAT "[%i] %t - (%f:%l) <%d> (%n) -- "
#define RT_LOG_DEF_LOG_FORMAT "[%i] (%f:%l) <%d> -"
#endif

/* The maximum length of the log message */
#define RT_LOG_MAX_LOG_MSG_LEN 2048

/* The maximum length of the log format */
#define RT_LOG_MAX_LOG_FORMAT_LEN 128

/* The default log level, if it is not supplied by the user */
#define RT_LOG_DEF_LOG_LEVEL RT_LOG_INFO

/* The default output interface to be used */
//#define RT_LOG_DEF_LOG_OP_IFACE RT_LOG_OP_IFACE_CONSOLE
#define RT_LOG_DEF_LOG_OP_IFACE RT_LOG_OP_IFACE_SYSLOG

/* The default log file to be used */
#define RT_LOG_DEF_LOG_FILE "sc_ids_log.log"

/* The default syslog facility to be used */
#define RT_LOG_DEF_SYSLOG_FACILITY_STR "local0"
#define RT_LOG_DEF_SYSLOG_FACILITY LOG_LOCAL0

/**
 * \brief Structure to be used when log_level override support would be provided
 *        by the logging module
 */
typedef struct RTLogOPBuffer_ {
    char msg[RT_LOG_MAX_LOG_MSG_LEN];
    char *temp;
    const char *log_format;
} RTLogOPBuffer;

/**
 * \brief The output interface context for the logging module
 */
typedef void (*rt_transmit_callback) (int , char *);

typedef struct RTLogOPIfaceCtx_ {
    RTLogOPIface iface;

    /* the output file to be used if the interface is RT_LOG_IFACE_FILE */
    char file[256];
    /* the output file descriptor for the above file */
    FILE * file_d;

    /** add for send*/
    int sock;
    rt_transmit_callback transmit_callback;

    /* the facility code if the interface is RT_LOG_IFACE_SYSLOG */
    int facility;

    /* override for the global_log_format(currently not used) */
    char *log_format;

    /* override for the global_log_level */
    RTLogLevel log_level;

    struct RTLogOPIfaceCtx_ *next;
} RTLogOPIfaceCtx;

/**
 * \brief Structure containing init data, that would be passed to
 *        RTInitDebugModule()
 */
typedef struct RTLogInitData_ {
    /* startup message */
    char *startup_message;

    /* the log level */
    RTLogLevel global_log_level;

    /* the log format */
    char *global_log_format;

    /* output filter */
    char *op_filter;

    /* list of output interfaces to be used */
    RTLogOPIfaceCtx *op_ifaces;
    /* no of op ifaces */
    uint8_t op_ifaces_cnt;
} RTLogInitData;

/**
 * \brief Holds the config state used by the logging api
 */
typedef struct RTLogConfig_ {
    char *startup_message;
    RTLogLevel log_level;
    char *log_format;

    char *op_filter;
    /* compiled pcre filter expression */
    pcre *op_filter_regex;
    pcre_extra *op_filter_regex_study;

    /* op ifaces used */
    RTLogOPIfaceCtx *op_ifaces;
    /* no of op ifaces */
    uint8_t op_ifaces_cnt;
} RTLogConfig;

/* The different log format specifiers supported by the API */
#define RT_LOG_FMT_TIME             't' /* Timestamp in standard format */
#define RT_LOG_FMT_PID              'p' /* PID */
#define RT_LOG_FMT_TID              'i' /* Thread ID */
#define RT_LOG_FMT_TM               'm' /* Thread module name */
#define RT_LOG_FMT_LOG_LEVEL        'd' /* Log level */
#define RT_LOG_FMT_FILE_NAME        'f' /* File name */
#define RT_LOG_FMT_LINE             'l' /* Line number */
#define RT_LOG_FMT_FUNCTION         'n' /* Function */

/* The log format prefix for the format specifiers */
#define RT_LOG_FMT_PREFIX           '%'

extern RTLogLevel rt_logging_global_loglevel;

extern int rt_logging_module_initialized;

extern int rt_logging_module_cleaned;

extern void rt_logging_init_console(char *format, RTLogLevel level, RTLogInitData* sc_lid);

extern void rt_logging_init_file(char *filename, char *dir, char *format,
                                 RTLogLevel level, RTLogInitData* sc_lid);

extern void rt_logging_init_syslog(char *facility, char *format,
                                   RTLogLevel level, RTLogInitData* sc_lid);

extern void rt_logging_int_link(char *facility, char *format, RTLogLevel level,
                                RTLogInitData* sc_lid, rt_transmit_callback transmit_callback);

extern void rt_logging_append_opifctx(RTLogOPIfaceCtx *iface_ctx, RTLogInitData *sc_lid);

extern void rt_logging_init(RTLogInitData *sc_lid);

extern void rt_logging_deinit();

extern RTLogInitData *rt_logging_alloc(void);

extern RTLogInitData *rt_logging_get_sclid(void);

extern RTLogInitData *rt_logging_sclid_init(void);

extern void rt_logging_delete_opifctx(const char *iface_namee, RTLogInitData *sc_lid);

extern RTLogOPIfaceCtx *rt_logging_get_opifctx(const char *iface_name, RTLogInitData *sc_lid);

extern rt_errno rt_logging_format_message(RTLogLevel log_level, char **msg, const char *file,
                     unsigned line, const char *function);

extern int rt_logging_set_opifctx_level(const char *iface_name, RTLogInitData *sc_lid, RTLogLevel log_level);

extern RTLogLevel rt_logging_get_opifctx_level(const char *iface_name, RTLogInitData *sc_lid);

extern RTLogLevel rt_logging_get_default_level();

extern void rt_logging_print(RTLogLevel log_level, char *msg);



#define rt_log(x, ...)         do {                                       \
                                  char _sc_log_msg[RT_LOG_MAX_LOG_MSG_LEN] = ""; \
                                  char *_sc_log_temp = _sc_log_msg;      \
                                  if ( !(                                \
                                      (rt_logging_global_loglevel >= x) &&  \
                                       rt_logging_format_message(x, &_sc_log_temp,    \
                                                    __FILE__,            \
                                                    __LINE__,            \
                                                    __FUNCTION__)        \
                                       == XSUCCESS) )                       \
                                  { } else {                             \
                                      snprintf(_sc_log_temp,             \
                                               (RT_LOG_MAX_LOG_MSG_LEN - \
                                                (_sc_log_temp - _sc_log_msg)), \
                                               __VA_ARGS__);             \
                                      rt_logging_print(x, _sc_log_msg); \
                                  }                                      \
                              } while(0)

#define rt_log_err(x, err, ...) do {                                       \
                                  char _sc_log_err_msg[RT_LOG_MAX_LOG_MSG_LEN] = ""; \
                                  char *_sc_log_err_temp = _sc_log_err_msg; \
                                  if ( !(                                \
                                      (rt_logging_global_loglevel >= x) &&  \
                                       rt_logging_format_message(x, &_sc_log_err_temp,\
                                                    __FILE__,            \
                                                    __LINE__,            \
                                                    __FUNCTION__)        \
                                       == XSUCCESS) )                       \
                                  { } else {                             \
                                      _sc_log_err_temp =                 \
                                                _sc_log_err_temp +       \
                                                snprintf(_sc_log_err_temp, \
                                               (RT_LOG_MAX_LOG_MSG_LEN - \
                                                (_sc_log_err_temp - _sc_log_err_msg)), \
                                               "[ERRCODE: %s(%d)] - ",   \
                                               rt_errno2string(err),     \
                                               err);                     \
                                      if ((_sc_log_err_temp - _sc_log_err_msg) > \
                                          RT_LOG_MAX_LOG_MSG_LEN) {      \
                                          printf("Warning: Log message exceeded message length limit of %d\n",\
                                                 RT_LOG_MAX_LOG_MSG_LEN); \
                                          _sc_log_err_temp = _sc_log_err_msg + \
                                              RT_LOG_MAX_LOG_MSG_LEN;    \
                                      } else {                          \
                                          snprintf(_sc_log_err_temp,    \
                                                   (RT_LOG_MAX_LOG_MSG_LEN - \
                                                    (_sc_log_err_temp - _sc_log_err_msg)), \
                                                   __VA_ARGS__);        \
                                      }                                 \
                                      rt_logging_print(x, _sc_log_err_msg); \
                                  }                                      \
                              } while(0)

/**
 * \brief Macro used to log INFORMATIONAL messages.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define rt_log_info(...) rt_log(RT_LOG_INFO, __VA_ARGS__)

/**
 * \brief Macro used to log NOTICE messages.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define rt_log_notice(...) rt_log(RT_LOG_NOTICE, __VA_ARGS__)

/**
 * \brief Macro used to log WARNING messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  warning message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define rt_log_warning(err_code, ...) rt_log_err(RT_LOG_WARNING, err_code, \
                                          __VA_ARGS__)
/**
 * \brief Macro used to log ERROR messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  error message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define rt_log_error(err_code, ...) rt_log_err(RT_LOG_ERROR, err_code, \
                                        __VA_ARGS__)
/**
 * \brief Macro used to log CRITICAL messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  critical message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define rt_log_critical(err_code, ...) rt_log_err(RT_LOG_CRITICAL, err_code, \
                                           __VA_ARGS__)
/**
 * \brief Macro used to log ALERT messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  alert message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define rt_log_alert(err_code, ...) rt_log_err(RT_LOG_ALERT, err_code, \
                                        __VA_ARGS__)
/**
 * \brief Macro used to log EMERGENCY messages.
 *
 * \retval err_code Error code that has to be logged along with the
 *                  emergency message
 * \retval ...      Takes as argument(s), a printf style format message
 */
#define rt_log_emerg(err_code, ...) rt_log_err(RT_LOG_EMERGENCY, err_code, \
                                          __VA_ARGS__)
#define DEBUG

/* Avoid the overhead of using the debugging subsystem, in production mode */
#ifndef DEBUG

#define rt_log_debug(...)                 do { } while (0)

/* Please use it only for debugging purposes */
#else


/**
 * \brief Macro used to log DEBUG messages. Comes under the debugging subsystem,
 *        and hence will be enabled only in the presence of the DEBUG macro.
 *
 * \retval ... Takes as argument(s), a printf style format message
 */
#define rt_log_debug(...)       rt_log(RT_LOG_DEBUG, __VA_ARGS__)

#endif /* DEBUG */

extern RTLogLevel rt_logging_parse_level(char *level);
extern int rt_logging_mkfile(char *file, size_t size);

extern void rt_logging_reinit_file (char *logdir);

extern void rt_logging_open (char *loglevel, char *logform, char *logdir);

#endif /* __UTIL_DEBUG_H__ */

