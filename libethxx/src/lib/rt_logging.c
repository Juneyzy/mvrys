#include <syslog.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <linux/limits.h>
#include <sys/time.h>

#include "rt_common.h"
#include "rt_logging.h"
#include "rt_errno.h"
#include "rt_enum.h"
#include "rt_logging_filters.h"
#include "rt_time.h"
#include "rt_file.h"
#include "rt_util.h"
#include "rt_string.h"
#include "rt_stdlib.h"

#define DEFAULT_LOG_DIR "/var/log/suricata"

/* holds the string-enum mapping for the syslog facility in RTLogOPIfaceCtx */
rt_enum_val_map sc_syslog_facility_map[] = {
    { "auth",           LOG_AUTH },
    { "authpriv",       LOG_AUTHPRIV },
    { "cron",           LOG_CRON },
    { "daemon",         LOG_DAEMON },
    { "ftp",            LOG_FTP },
    { "kern",           LOG_KERN },
    { "lpr",            LOG_LPR },
    { "mail",           LOG_MAIL },
    { "news",           LOG_NEWS },
    { "security",       LOG_AUTH },
    { "syslog",         LOG_SYSLOG },
    { "user",           LOG_USER },
    { "uucp",           LOG_UUCP },
    { "local0",         LOG_LOCAL0 },
    { "local1",         LOG_LOCAL1 },
    { "local2",         LOG_LOCAL2 },
    { "local3",         LOG_LOCAL3 },
    { "local4",         LOG_LOCAL4 },
    { "local5",         LOG_LOCAL5 },
    { "local6",         LOG_LOCAL6 },
    { "local7",         LOG_LOCAL7 },
    { NULL,             -1         }
};

/** \brief returns the syslog facility enum map */
rt_enum_val_map *RTSyslogGetFacilityMap(void) {
    return sc_syslog_facility_map;
}

rt_enum_val_map sc_syslog_level_map[ ] = {
    { "Emergency",      LOG_EMERG },
    { "Alert",          LOG_ALERT },
    { "Critical",       LOG_CRIT },
    { "Error",          LOG_ERR },
    { "Warning",        LOG_WARNING },
    { "Notice",         LOG_NOTICE },
    { "Info",           LOG_INFO },
    { "Debug",          LOG_DEBUG },
    { NULL,             -1 }
};

/** \brief returns the syslog facility enum map */
rt_enum_val_map *RTSyslogGetLogLevelMap(void) {
    return sc_syslog_level_map;
}


/* holds the string-enum mapping for the enums held in the table RTLogLevel */
rt_enum_val_map rt_loglevel_map[ ] = {
    { "Not set",        RT_LOG_NOTSET},
    { "None",           RT_LOG_NONE },
    { "Emergency",      RT_LOG_EMERGENCY },
    { "Alert",          RT_LOG_ALERT },
    { "Critical",       RT_LOG_CRITICAL },
    { "Error",          RT_LOG_ERROR },
    { "Warning",        RT_LOG_WARNING },
    { "Notice",         RT_LOG_NOTICE },
    { "Info",           RT_LOG_INFO },
    { "Debug",          RT_LOG_DEBUG },
    { NULL,             -1 }
};

/* holds the string-enum mapping for the enums held in the table RTLogOPIface */
rt_enum_val_map rt_log_op_iface_map[ ] = {
    { "Console",        RT_LOG_OP_IFACE_CONSOLE },
    { "File",           RT_LOG_OP_IFACE_FILE },
    { "Syslog",         RT_LOG_OP_IFACE_SYSLOG },
    { "Link",           RT_LOG_OP_IFACE_LINK},
    { NULL,             -1 }
};

#if defined (OS_WIN32)
/**
 * \brief Used for synchronous output on WIN32
 */
static rt_mutex rt_logging_stream_lock = NULL;
#endif /* OS_WIN32 */

/**
 * \brief Holds the config state for the logging module
 */
static RTLogConfig *rt_logging_config = NULL;

/**
 * \brief Returns the full path given a file and configured log dir
 */
static char *rt_logging_get_filename(const char *, const char *);

/**
 * \brief Holds the global log level.  Is the same as rt_logging_config->log_level
 */
RTLogLevel rt_logging_global_loglevel;

/**
 * \brief Used to indicate whether the logging module has been init or not
 */
int rt_logging_module_initialized = 0;

/**
 * \brief Used to indicate whether the logging module has been cleaned or not
 */
int rt_logging_module_cleaned = 0;


static __rt_always_inline__ void *l_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

/**
 * \brief Maps the RT logging level to the syslog logging level
 *
 * \param The RT logging level that has to be mapped to the syslog_log_level
 *
 * \retval syslog_log_level The mapped syslog_api_log_level, for the logging
 *                          module api's internal log_level
 */
static __rt_always_inline__ int rt_logging_map_loglevel_to_sysloglevel(int log_level)
{
    int syslog_log_level = 0;

    switch (log_level) {
        case RT_LOG_EMERGENCY:
            syslog_log_level = LOG_EMERG;
            break;
        case RT_LOG_ALERT:
            syslog_log_level = LOG_ALERT;
            break;
        case RT_LOG_CRITICAL:
            syslog_log_level = LOG_CRIT;
            break;
        case RT_LOG_ERROR:
            syslog_log_level = LOG_ERR;
            break;
        case RT_LOG_WARNING:
            syslog_log_level = LOG_WARNING;
            break;
        case RT_LOG_NOTICE:
            syslog_log_level = LOG_NOTICE;
            break;
        case RT_LOG_INFO:
            syslog_log_level = LOG_INFO;
            break;
        case RT_LOG_DEBUG:
            syslog_log_level = LOG_DEBUG;
            break;
        default:
            syslog_log_level = LOG_EMERG;
            break;
    }

    return syslog_log_level;
}

/**
 * \brief Output function that logs a character string out to a file descriptor
 *
 * \param fd  Pointer to the file descriptor
 * \param msg Pointer to the character string that should be logged
 */
static __rt_always_inline__ void rt_logging_printo_stream(FILE *fd, char *msg)
{
#if defined (OS_WIN32)
	rt_mutex_lock(&rt_logging_stream_lock);
#endif /* OS_WIN32 */

    if (fprintf(fd, "%s", msg) < 0)
        printf("Error writing to stream using fprintf\n");

    fflush(fd);

#if defined (OS_WIN32)
	rt_mutex_unlock(&rt_logging_stream_lock);
#endif /* OS_WIN32 */

    return;
}

static __rt_always_inline__ void
rt_logging_printo_link(int sock, rt_transmit_callback transmit_callback, char *msg)
{
    if (sock > 0 && msg != NULL){
        transmit_callback(sock, msg);
    }
}

/**
 * \brief Output function that logs a character string throught the syslog iface
 *
 * \param syslog_log_level Holds the syslog_log_level that the message should be
 *                         logged as
 * \param msg              Pointer to the char string, that should be logged
 *
 * \todo syslog is thread-safe according to POSIX manual and glibc code, but we
 *       we will have to look into non POSIX compliant boxes like freeBSD
 */
static __rt_always_inline__ void rt_logging_printo_syslog(int syslog_log_level, const char *msg)
{
    //static struct syslog_data data = SYSLOG_DATA_INIT;
    //syslog_r(syslog_log_level, NULL, "%s", msg);

    syslog(syslog_log_level, "%s", msg);

    return;
}

/**
 * \brief Outputs the message sent as the argument
 *
 * \param msg       Pointer to the message that has to be logged
 * \param log_level The log_level of the message that has to be logged
 */
void rt_logging_print(RTLogLevel log_level, char *msg)
{
    char *temp = msg;
    int len = strlen(msg);
    RTLogOPIfaceCtx *op_iface_ctx = NULL;

#define MAX_SUBSTRINGS 30
    int ov[MAX_SUBSTRINGS];

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call rt_logging_init() "
               "first before using the debug API\n");
        return;
    }

    /* We need to add a \n for our messages, before logging them.  If the
     * messages have hit the 1023 length limit, strip the message to
     * accomodate the \n */
    if (len == RT_LOG_MAX_LOG_MSG_LEN - 1)
        len = RT_LOG_MAX_LOG_MSG_LEN - 2;

    temp[len] = '\n';
    temp[len + 1] = '\0';

    if (rt_logging_config->op_filter_regex != NULL) {
        if (pcre_exec(rt_logging_config->op_filter_regex,
                      rt_logging_config->op_filter_regex_study,
                      msg, strlen(msg), 0, 0, ov, MAX_SUBSTRINGS) < 0)
            return;
    }

    op_iface_ctx = rt_logging_config->op_ifaces;
	//printf("sssssss %lx,log_level %d  if-log_level %d\n",(uint64_t)op_iface_ctx,log_level,op_iface_ctx->log_level);
    while (op_iface_ctx != NULL) {
        if (log_level != RT_LOG_NOTSET && log_level > op_iface_ctx->log_level) {
            op_iface_ctx = op_iface_ctx->next;
            continue;
        }

        switch (op_iface_ctx->iface) {
            case RT_LOG_OP_IFACE_CONSOLE:
                rt_logging_printo_stream((log_level == RT_LOG_ERROR)? stderr: stdout,
                                   msg);
                break;
            case RT_LOG_OP_IFACE_FILE:
                rt_logging_printo_stream(op_iface_ctx->file_d, msg);
                break;
            case RT_LOG_OP_IFACE_SYSLOG:
                rt_logging_printo_syslog(rt_logging_map_loglevel_to_sysloglevel(log_level),
                                   msg);
                break;
            case RT_LOG_OP_IFACE_LINK:
                rt_logging_printo_link(op_iface_ctx->sock, op_iface_ctx->transmit_callback, msg);
                break;
            default:
                break;
        }
        op_iface_ctx = op_iface_ctx->next;
    }

    return;
}

RTLogLevel rt_logging_parse_level(char *level)
{
    RTLogLevel loglevel = RT_LOG_NOTSET;

    if(!STRCMP(level, "emerg")){
        loglevel = RT_LOG_EMERGENCY;
    }
    if(!STRCMP(level, "alert")){
        loglevel = RT_LOG_ALERT;
    }
    if(!STRCMP(level, "crit")){
        loglevel = RT_LOG_CRITICAL;
    }
    if(!STRCMP(level, "err")){
        loglevel = RT_LOG_ERROR;
    }
    if(!STRCMP(level, "warning")){
        loglevel = RT_LOG_WARNING;
    }
    if(!STRCMP(level, "notice")){
        loglevel = RT_LOG_NOTICE;
    }
    if(!STRCMP(level, "info")){
        loglevel = RT_LOG_INFO;
    }
    if(!STRCMP(level, "debug")){
        loglevel = RT_LOG_DEBUG;
    }

    return loglevel;
}

int rt_logging_mkfile(char *file, size_t size)
{
    char tm[64] = {0};
    char pname[64]= {0};
    
    assert(file);

    if (size < 32)
        return -1;
    
    rt_curr_tms2str(EVAL_TM_STYLE, tm, 63);
    rt_get_pname_by_pid(getpid(), &pname[0]);
    snprintf(file, 255, "%s-%s-%d-%d-%s.log", pname, getpwuid(getuid())->pw_name, getpwuid(getuid())->pw_uid, getpwuid(getuid())->pw_gid, tm);
    
    return 0;
}

/**
 * \brief Adds the global log_format to the outgoing buffer
 *
 * \param log_level log_level of the message that has to be logged
 * \param msg       Buffer containing the outgoing message
 * \param file      File_name from where the message originated
 * \param function  Function_name from where the message originated
 * \param line      Line_no from where the messaged originated
 *
 * \retval RT_OK on success; else an error code
 */
rt_errno rt_logging_format_message(RTLogLevel log_level, char **msg, const char *file,
                     unsigned line, const char *function)
{
    char *temp = *msg;
    const char *s = NULL;

    struct timeval tval;
    struct tm *tms = NULL;

    /* no of characters_written(cw) by snprintf */
    int cw = 0;

    if (rt_logging_module_initialized != 1) {
#ifdef DEBUG
        printf("Logging module not initialized.  Call rt_logging_init(), "
               "before using the logging API\n");
#endif
        return ERRNO_LOGGING_INIT;
    }

    if (rt_logging_fg_filters_present == 1) {
        if (RTLogMatchFGFilterWL(file, function, line) != 1) {
            return ERRNO_LOGGING_FG_FILTER_MATCH;
        }

        if (RTLogMatchFGFilterBL(file, function, line) != 1) {
            return ERRNO_LOGGING_FG_FILTER_MATCH;
        }
    }

    if (rt_logging_fd_filters_present == 1 && RTLogMatchFDFilter(function) != 1) {
        return ERRNO_LOGGING_FG_FILTER_MATCH;
    }

    char *temp_fmt = strdup(rt_logging_config->log_format);
    if (temp_fmt == NULL) {
        return ERRNO_MEM_ALLOC;
    }
    char *temp_fmt_h = temp_fmt;
    char *substr = temp_fmt;
	while ( (temp_fmt = index(temp_fmt, RT_LOG_FMT_PREFIX)) ) {
        if ((temp - *msg) > RT_LOG_MAX_LOG_MSG_LEN) {
            printf("Warning: Log message exceeded message length limit of %d\n",
                   RT_LOG_MAX_LOG_MSG_LEN);
            *msg = *msg + RT_LOG_MAX_LOG_MSG_LEN;
            if (temp_fmt_h != NULL)
                free(temp_fmt_h);
            return XSUCCESS;
        }
        switch(temp_fmt[1]) {
            case RT_LOG_FMT_TIME:
                temp_fmt[0] = '\0';

                gettimeofday(&tval, NULL);
                struct tm local_tm;
                tms = rt_localtime(tval.tv_sec, &local_tm);

                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%d/%d/%04d -- %02d:%02d:%02d",
                              substr, tms->tm_mday, tms->tm_mon + 1,
                              tms->tm_year + 1900, tms->tm_hour, tms->tm_min,
                              tms->tm_sec);
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_PID:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%u", substr, getpid());
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_TID:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%u", substr, getpid());
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_TM:
                temp_fmt[0] = '\0';
                //ThreadVars *tv = TmThreadsGetCallingThread();
                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s", substr);  //, ((tv != NULL)? tv->name: "UNKNOWN TM"));
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_LOG_LEVEL:
                temp_fmt[0] = '\0';
                s = rt_enum_v2n(log_level, rt_loglevel_map);
                if (s != NULL)
                    cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                                  "%s%s", substr, s);
                else
                    cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                                  "%s%s", substr, "INVALID");
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_FILE_NAME:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%s", substr, file);
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_LINE:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%d", substr, line);
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

            case RT_LOG_FMT_FUNCTION:
                temp_fmt[0] = '\0';
                cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg),
                              "%s%s", substr, function);
                if (cw < 0)
                    goto error;
                temp += cw;
                temp_fmt++;
                substr = temp_fmt;
                substr++;
                break;

        }
        temp_fmt++;
	}
    if ((temp - *msg) > RT_LOG_MAX_LOG_MSG_LEN) {
        printf("Warning: Log message exceeded message length limit of %d\n",
               RT_LOG_MAX_LOG_MSG_LEN);
        *msg = *msg + RT_LOG_MAX_LOG_MSG_LEN;
        if (temp_fmt_h != NULL)
            free(temp_fmt_h);
        return XSUCCESS;
    }
    cw = snprintf(temp, RT_LOG_MAX_LOG_MSG_LEN - (temp - *msg), "%s", substr);
    if (cw < 0)
        goto error;
    temp += cw;
    if ((temp - *msg) > RT_LOG_MAX_LOG_MSG_LEN) {
        printf("Warning: Log message exceeded message length limit of %d\n",
               RT_LOG_MAX_LOG_MSG_LEN);
        *msg = *msg + RT_LOG_MAX_LOG_MSG_LEN;
        if (temp_fmt_h != NULL)
            free(temp_fmt_h);
        return XSUCCESS;
    }

    *msg = temp;

    free(temp_fmt_h);

    return XSUCCESS;

 error:
    if (temp_fmt != NULL)
        free(temp_fmt_h);
    return ERRNO_WARNING;
}

/**
 * \brief Returns whether debug messages are enabled to be logged or not
 *
 * \retval 1 if debug messages are enabled to be logged
 * \retval 0 if debug messages are not enabled to be logged
 */
int rt_logging_debug_enabled(void)
{
#ifdef DEBUG
    if (rt_logging_global_loglevel == RT_LOG_DEBUG)
        return 1;
    else
        return 0;
#else
    return 0;
#endif
}

/**
 * \brief Allocates an output buffer for an output interface.  Used when we
 *        want the op_interface log_format to override the global_log_format.
 *        Currently not used.
 *
 * \retval buffer Pointer to the newly created output_buffer
 */
RTLogOPBuffer *rt_logging_alloc_opbuffer(void)
{
    RTLogOPBuffer *buffer = NULL;
    RTLogOPIfaceCtx *op_iface_ctx = NULL;
    int i = 0;

    if ( (buffer = l_kmalloc(rt_logging_config->op_ifaces_cnt *
                          sizeof(RTLogOPBuffer))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in rt_logging_alloc_opbuffer. Exiting...");
        exit(EXIT_FAILURE);
    }

    op_iface_ctx = rt_logging_config->op_ifaces;
    for (i = 0;
         i < rt_logging_config->op_ifaces_cnt;
         i++, op_iface_ctx = op_iface_ctx->next) {
        buffer[i].log_format = op_iface_ctx->log_format;
        buffer[i].temp = buffer[i].msg;
    }

    return buffer;
}

/*----------------------The logging module initialization code--------------- */

/**
 * \brief Returns a new output_interface_context
 *
 * \retval iface_ctx Pointer to a newly allocated output_interface_context
 * \initonly
 */
static __rt_always_inline__ RTLogOPIfaceCtx *rt_logging_alloc_opifctx(void)
{
    RTLogOPIfaceCtx *iface_ctx = NULL;

    if ( (iface_ctx = l_kmalloc(sizeof(RTLogOPIfaceCtx))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogallocLogOPIfaceCtx. Exiting...");
        exit(EXIT_FAILURE);
    }

    return iface_ctx;
}

/**
 * \brief Initializes the file output interface
 *
 * \param file       Path to the file used for logging purposes
 * \param log_format Pointer to the log_format for this op interface, that
 *                   overrides the global_log_format
 * \param log_level  Override of the global_log_level by this interface
 *
 * \retval iface_ctx Pointer to the file output interface context created
 * \initonly
 */
static __rt_always_inline__ RTLogOPIfaceCtx *rt_logging_init_file_opifctx(char *file,
                                                    __attribute__((unused))const char *log_format,
                                                    int log_level)
{
    RTLogOPIfaceCtx *iface_ctx = rt_logging_alloc_opifctx();
    char fmode[8] = {0};

    if (iface_ctx == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in rt_logging_init_file_opifctx. Exiting...");
        exit(EXIT_FAILURE);
    }

    if (file == NULL) {    // || log_format == NULL) {
        goto error;
    }

    iface_ctx->iface = RT_LOG_OP_IFACE_FILE;

    if(rt_file_exsit(file)){
        strcpy(fmode, "a+");
    }
    else{
        strcpy(fmode, "w+");
    }

    if ( (iface_ctx->file_d = fopen(file, fmode)) == NULL) {
        printf("fopen[%s]: %s\n", file, strerror(errno));
        goto error;
    }
#if 0
    if ((iface_ctx->file = strdup(file)) == NULL) {
        goto error;
    }
#else
	memset (iface_ctx->file, 0, strlen (iface_ctx->file));
	memcpy (iface_ctx->file, file, strlen(file));
#endif

/**
    if ((iface_ctx->log_format = strdup(log_format)) == NULL) {
        goto error;
    }
*/
    iface_ctx->log_level = log_level;

    return iface_ctx;

error:
/**
    if (iface_ctx->file != NULL) {
        free((char *)iface_ctx->file);
        iface_ctx->file = NULL;
    }
 */
    if (iface_ctx->log_format != NULL) {
        free((char *)iface_ctx->log_format);
        iface_ctx->log_format = NULL;
    }
    if (iface_ctx->file_d != NULL) {
        fclose(iface_ctx->file_d);
        iface_ctx->file_d = NULL;
    }
    return NULL;
}

/**
 * \brief Initializes the console output interface and deals with possible
 *        env var overrides.
 *
 * \param log_format Pointer to the log_format for this op interface, that
 *                   overrides the global_log_format
 * \param log_level  Override of the global_log_level by this interface
 *
 * \retval iface_ctx Pointer to the console output interface context created
 * \initonly
 */
static __rt_always_inline__ RTLogOPIfaceCtx *rt_logging_init_console_opifctx(const char *log_format,
                                                       RTLogLevel log_level)
{
    RTLogOPIfaceCtx *iface_ctx = rt_logging_alloc_opifctx();

    if (iface_ctx == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in rt_logging_init_console_opifctx. Exiting...");
        exit(EXIT_FAILURE);
    }

    iface_ctx->iface = RT_LOG_OP_IFACE_CONSOLE;

    /* console log format is overridden by envvars */
    const char *tmp_log_format = log_format;
    const char *s = getenv(RT_LOG_ENV_LOG_FORMAT);
    if (s != NULL) {
#if 0
        printf("Overriding setting for \"console.format\" because of env "
                "var RT_LOG_FORMAT=\"%s\".\n", s);
#endif
        tmp_log_format = s;
    }

    if (tmp_log_format != NULL &&
        (iface_ctx->log_format = strdup(tmp_log_format)) == NULL) {
        printf("Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    /* console log level is overridden by envvars */
    RTLogLevel tmp_log_level = log_level;
    s = getenv(RT_LOG_ENV_LOG_LEVEL);
    if (s != NULL) {
        RTLogLevel l = rt_enum_n2v(s, rt_loglevel_map);
        if (l > RT_LOG_NOTSET && l < RT_LOG_LEVEL_MAX) {
#if 0
            printf("Overriding setting for \"console.level\" because of env "
                    "var RT_LOG_LEVEL=\"%s\".\n", s);
#endif
            tmp_log_level = l;
        }
    }
    iface_ctx->log_level = tmp_log_level;

    return iface_ctx;
}

static __rt_always_inline__ RTLogOPIfaceCtx*
rt_logging_init_link_opifctx(char *file, const char *log_format,
                             RTLogLevel log_level, rt_transmit_callback transmit_callback)
{
    RTLogOPIfaceCtx *iface_ctx = rt_logging_alloc_opifctx();
    int sock = *((int *)file);

    if (iface_ctx == NULL || sock < 0) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in rt_logging_init_console_opifctx. Exiting...");
        exit(EXIT_FAILURE);
    }

    iface_ctx->sock = sock;
    iface_ctx->transmit_callback = transmit_callback;
    iface_ctx->iface = RT_LOG_OP_IFACE_LINK;

    /* console log format is overridden by envvars */
    const char *tmp_log_format = log_format;
    const char *s = getenv(RT_LOG_ENV_LOG_FORMAT);
    if (s != NULL) {
#if 0
        printf("Overriding setting for \"console.format\" because of env "
                "var RT_LOG_FORMAT=\"%s\".\n", s);
#endif
        tmp_log_format = s;
    }

    if (tmp_log_format != NULL &&
        (iface_ctx->log_format = strdup(tmp_log_format)) == NULL) {
        printf("Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    /* console log level is overridden by envvars */
    RTLogLevel tmp_log_level = log_level;
    s = getenv(RT_LOG_ENV_LOG_LEVEL);
    if (s != NULL) {
        RTLogLevel l = rt_enum_n2v(s, rt_loglevel_map);
        if (l > RT_LOG_NOTSET && l < RT_LOG_LEVEL_MAX) {
#if 0
            printf("Overriding setting for \"console.level\" because of env "
                    "var RT_LOG_LEVEL=\"%s\".\n", s);
#endif
            tmp_log_level = l;
        }
    }
    iface_ctx->log_level = tmp_log_level;

    return iface_ctx;
}

/**
 * \brief Initializes the syslog output interface
 *
 * \param facility   The facility code for syslog
 * \param log_format Pointer to the log_format for this op interface, that
 *                   overrides the global_log_format
 * \param log_level  Override of the global_log_level by this interface
 *
 * \retval iface_ctx Pointer to the syslog output interface context created
 */
static __rt_always_inline__ RTLogOPIfaceCtx *rt_logging_init_syslog_opifctx(int facility,
                                                      const char *log_format,
                                                      RTLogLevel log_level)
{
    RTLogOPIfaceCtx *iface_ctx = rt_logging_alloc_opifctx();

    if ( iface_ctx == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in rt_logging_init_syslog_opifctx. Exiting...");
        exit(EXIT_FAILURE);
    }

    iface_ctx->iface = RT_LOG_OP_IFACE_SYSLOG;

    if (facility == -1)
        facility = RT_LOG_DEF_SYSLOG_FACILITY;
    iface_ctx->facility = facility;

    if (log_format != NULL &&
        (iface_ctx->log_format = strdup(log_format)) == NULL) {
        printf("Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    iface_ctx->log_level = log_level;

    openlog(NULL, LOG_NDELAY, iface_ctx->facility);

    return iface_ctx;
}

/**
 * \brief Frees the output_interface context supplied as an argument
 *
 * \param iface_ctx Pointer to the op_interface_context to be freed
 */
static __rt_always_inline__ void rt_logging_free_opifctx(RTLogOPIfaceCtx *iface_ctx)
{
    RTLogOPIfaceCtx *temp = NULL;

    while (iface_ctx != NULL) {
        temp = iface_ctx;

        if (iface_ctx->file_d != NULL)
            fclose(iface_ctx->file_d);

        if (iface_ctx->log_format != NULL)
            free((void *)iface_ctx->log_format);

        if (iface_ctx->iface == RT_LOG_OP_IFACE_SYSLOG) {
            closelog();
        }

        iface_ctx = iface_ctx->next;

        free(temp);
    }

    return;
}

/**
 * \brief Internal function used to set the logging module global_log_level
 *        during the initialization phase
 *
 * \param sc_lid The initialization data supplied.
 * \param sc_lc  The logging module context which has to be updated.
 */
static __rt_always_inline__ void rt_logging_set_loglevel(RTLogInitData *sc_lid, RTLogConfig *sc_lc)
{
    RTLogLevel log_level = RT_LOG_NOTSET;
    const char *s = NULL;

    /* envvar overrides config */
    s = getenv(RT_LOG_ENV_LOG_LEVEL);
    if (s != NULL) {
        log_level = rt_enum_n2v(s, rt_loglevel_map);
    } else if (sc_lid != NULL) {
        log_level = sc_lid->global_log_level;
    }

    /* deal with the global_log_level to be used */
    if (log_level > RT_LOG_NOTSET && log_level < RT_LOG_LEVEL_MAX)
        sc_lc->log_level = log_level;
    else {
        sc_lc->log_level = RT_LOG_DEF_LOG_LEVEL;
#ifndef UNITTESTS
        if (sc_lid != NULL) {
            printf("Warning: Invalid/No global_log_level assigned by user.  Falling "
                   "back on the default_log_level \"%s\"\n",
                   rt_enum_v2n(sc_lc->log_level, rt_loglevel_map));
        }
#endif
    }

    /* we also set it to a global var, as it is easier to access it */
    rt_logging_global_loglevel = sc_lc->log_level;

    return;
}

/**
 * \brief Internal function used to set the logging module global_log_format
 *        during the initialization phase
 *
 * \param sc_lid The initialization data supplied.
 * \param sc_lc  The logging module context which has to be updated.
 */
static __rt_always_inline__ void rt_logging_set_format(RTLogInitData *sc_lid, RTLogConfig *sc_lc)
{
    const char *format = NULL;

    /* envvar overrides config */
    format = getenv(RT_LOG_ENV_LOG_FORMAT);
    if (format == NULL) {
        if (sc_lid != NULL) {
            format = sc_lid->global_log_format;
        }
    }

    /* deal with the global log format to be used */
    if (format == NULL || strlen(format) > RT_LOG_MAX_LOG_FORMAT_LEN) {
        format = RT_LOG_DEF_LOG_FORMAT;
#ifndef UNITTESTS
        if (sc_lid != NULL) {
            printf("Warning: Invalid/No global_log_format supplied by user or format "
                   "length exceeded limit of \"%d\" characters.  Falling back on "
                   "default log_format \"%s\"\n", RT_LOG_MAX_LOG_FORMAT_LEN,
                   format);
        }
#endif
    }

    if (format != NULL && (sc_lc->log_format = strdup(format)) == NULL) {
        printf("Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    return;
}

/**
 * \brief Internal function used to set the logging module global_op_ifaces
 *        during the initialization phase
 *
 * \param sc_lid The initialization data supplied.
 * \param sc_lc  The logging module context which has to be updated.
 */
static __rt_always_inline__ void rt_logging_set_opif(RTLogInitData *sc_lid, RTLogConfig *sc_lc)
{
    RTLogOPIfaceCtx *op_ifaces_ctx = NULL;
    int op_iface = 0;
    char *s = NULL;

    if (sc_lid != NULL && sc_lid->op_ifaces != NULL) {
        sc_lc->op_ifaces = sc_lid->op_ifaces;
        /** motify for append*/
        //sc_lid->op_ifaces = NULL;
        sc_lc->op_ifaces_cnt = sc_lid->op_ifaces_cnt;
    } else {
        s = getenv(RT_LOG_ENV_LOG_OP_IFACE);
        if (s != NULL) {
            op_iface = rt_enum_n2v(s, rt_log_op_iface_map);

            if(op_iface < 0 || op_iface >= RT_LOG_OP_IFACE_MAX) {
                op_iface = RT_LOG_DEF_LOG_OP_IFACE;
#ifndef UNITTESTS
                printf("Warning: Invalid output interface supplied by user.  "
                       "Falling back on default_output_interface \"%s\"\n",
                       rt_enum_v2n(op_iface, rt_log_op_iface_map));
#endif
            }
        }
        else {
            op_iface = RT_LOG_DEF_LOG_OP_IFACE;
#ifndef UNITTESTS
            if (sc_lid != NULL) {
                printf("Warning: Output_interface not supplied by user.  Falling "
                       "back on default_output_interface \"%s\"\n",
                       rt_enum_v2n(op_iface, rt_log_op_iface_map));
            }
#endif
        }

        sc_lid = rt_logging_sclid_init();
        switch (op_iface) {
            case RT_LOG_OP_IFACE_CONSOLE:
                op_ifaces_ctx = rt_logging_init_console_opifctx(NULL, RT_LOG_LEVEL_MAX);
                break;
            case RT_LOG_OP_IFACE_FILE:
                s = getenv(RT_LOG_ENV_LOG_FILE);
                if (s == NULL)
                    s = rt_logging_get_filename(NULL, RT_LOG_DEF_LOG_FILE);

                op_ifaces_ctx = rt_logging_init_file_opifctx(s, NULL, RT_LOG_LEVEL_MAX);
                break;
            case RT_LOG_OP_IFACE_SYSLOG:
                s = getenv(RT_LOG_ENV_LOG_FACILITY);
                if (s == NULL)
                    s = RT_LOG_DEF_SYSLOG_FACILITY_STR;

                op_ifaces_ctx = rt_logging_init_syslog_opifctx(rt_enum_n2v(s, RTSyslogGetFacilityMap()), NULL, RT_LOG_INFO);
                break;
        }
        if (sc_lid != NULL){
            rt_logging_append_opifctx(op_ifaces_ctx, sc_lid);
        }
        sc_lc->op_ifaces = op_ifaces_ctx;
        sc_lc->op_ifaces_cnt++;
    }

    return;
}

/**
 * \brief Internal function used to set the logging module op_filter
 *        during the initialization phase
 *
 * \param sc_lid The initialization data supplied.
 * \param sc_lc  The logging module context which has to be updated.
 */
static __rt_always_inline__ void rt_logging_set_op_filter(RTLogInitData *sc_lid, RTLogConfig *sc_lc)
{
    const char *filter = NULL;

    int opts = 0;
    const char *ep;
    int eo = 0;

    /* envvar overrides */
    filter = getenv(RT_LOG_ENV_LOG_OP_FILTER);
    if (filter == NULL) {
        if (sc_lid != NULL) {
            filter = sc_lid->op_filter;
        }
    }

    if (filter != NULL && strcmp(filter, "") != 0) {
        sc_lc->op_filter = strdup(filter);
        sc_lc->op_filter_regex = pcre_compile(filter, opts, &ep, &eo, NULL);
        if (sc_lc->op_filter_regex == NULL) {
            printf("pcre compile of \"%s\" failed at offset %d : %s\n", filter,
                   eo, ep);
            return;
        }

        sc_lc->op_filter_regex_study = pcre_study(sc_lc->op_filter_regex, 0,
                                                  &ep);
        if (ep != NULL) {
            printf("pcre study failed: %s\n", ep);
            return;
        }
    }

    return;
}

/**
 * \brief Returns a pointer to a new RTLogInitData.  This is a public interface
 *        intended to be used after the logging paramters are read from the
 *        conf file
 *
 * \retval sc_lid Pointer to the newly created RTLogInitData
 * \initonly
 */
RTLogInitData *rt_logging_alloc(void)
{
    RTLogInitData *sc_lid = NULL;

    if ( (sc_lid = l_kmalloc(sizeof(RTLogInitData))) == NULL)
        return NULL;

    return sc_lid;
}

RTLogInitData *sc_lid = NULL;

RTLogInitData *rt_logging_sclid_init(void)
{
    if ( (sc_lid = l_kmalloc(sizeof(RTLogInitData))) == NULL)
        return NULL;

    return sc_lid;
}

RTLogInitData *rt_logging_get_sclid(void)
{
    return sc_lid;
}

/**
 * \brief Frees a RTLogInitData
 *
 * \param sc_lid Pointer to the RTLogInitData to be freed
 *//*
void RTLogFreeLogInitData(RTLogInitData *sc_lid)
{
    if (sc_lid != NULL) {
        if (sc_lid->startup_message != NULL)
            free(sc_lid->startup_message);
        if (sc_lid->global_log_format != NULL)
            free(sc_lid->global_log_format);
        if (sc_lid->op_filter != NULL)
            free(sc_lid->op_filter);

        rt_logging_free_opifctx(sc_lid->op_ifaces);
    }

    return;
}
*/
/**
 * \brief Frees the logging module context
 */
static __rt_always_inline__ void rt_logging_free_config(RTLogConfig *sc_lc)
{
    if (sc_lc != NULL) {
        if (sc_lc->startup_message != NULL)
            free(sc_lc->startup_message);
        if (sc_lc->log_format != NULL)
            free(sc_lc->log_format);
        if (sc_lc->op_filter != NULL)
            free(sc_lc->op_filter);

        rt_logging_free_opifctx(sc_lc->op_ifaces);
        free(sc_lc);
    }

    return;
}

/**
 * \brief Appends an output_interface to the output_interface list sent in head
 *
 * \param iface_ctx Pointer to the output_interface that has to be added to head
 * \param head      Pointer to the output_interface list
 */
void rt_logging_append_opifctx(RTLogOPIfaceCtx *iface_ctx, RTLogInitData *sc_lid)
{
    RTLogOPIfaceCtx *temp = NULL, *prev = NULL;
    RTLogOPIfaceCtx **head = &sc_lid->op_ifaces;

    if (iface_ctx == NULL) {
#ifdef DEBUG
        printf("Argument(s) to rt_logging_append_opifctx() NULL\n");
#endif
        return;
    }

    temp = *head;
    while (temp != NULL) {
        prev = temp;
        temp = temp->next;
    }

    if (prev == NULL){
        *head = iface_ctx;
    }
    else{
        prev->next = iface_ctx;
    }
    sc_lid->op_ifaces_cnt++;

    return;
}

void rt_logging_delete_opifctx(const char *iface_name, RTLogInitData *sc_lid)
{
    unsigned int iface = rt_enum_n2v(iface_name, rt_log_op_iface_map);
    RTLogOPIfaceCtx *temp = NULL, *prev = NULL;
    RTLogOPIfaceCtx **head = &sc_lid->op_ifaces;

    if (iface > RT_LOG_OP_IFACE_MAX) {
#ifdef DEBUG
        printf("Argument(s) to rt_logging_append_opifctx() NULL\n");
#endif
        return;
    }

    temp = *head;
    if (NULL == temp){
#ifdef DEBUG
        printf("Linked list header is NULL\n");
#endif
        return;
    }

    temp = *head;
    while (temp->iface != iface && temp->next != NULL)
    {
        prev = temp;
        temp = temp->next;
    }

    if (iface == temp->iface){
        if (temp == *head){
            *head = temp->next;
            rt_logging_config->op_ifaces = *head;
        }else{
            prev->next = temp->next;
        }
        free(temp);
        temp = NULL;
        sc_lid->op_ifaces_cnt--;
    }

    return;
}

RTLogOPIfaceCtx *rt_logging_get_opifctx(const char *iface_name, RTLogInitData *sc_lid)
{
    unsigned int iface = rt_enum_n2v(iface_name, rt_log_op_iface_map);
    RTLogOPIfaceCtx *temp = NULL;
    RTLogOPIfaceCtx **head = &sc_lid->op_ifaces;
    RTLogOPIfaceCtx *ifacectx = NULL;
		
    if (iface > RT_LOG_OP_IFACE_MAX) {
#ifdef DEBUG
        printf("Argument(s) to rt_logging_append_opifctx() NULL\n");
#endif
        return NULL;
    }

    temp = *head;
    if (NULL == temp){
#ifdef DEBUG
        printf("Linked list header is NULL\n");
#endif
        return NULL;
    }

    temp = *head;
    while (temp->iface != iface && temp->next != NULL)
        temp = temp->next;

    if (iface == temp->iface) {
        ifacectx = temp;
    }

    return ifacectx;
}

int rt_logging_set_opifctx_level(const char *iface_name, RTLogInitData *sc_lid, RTLogLevel log_level)
{
    unsigned int iface = rt_enum_n2v(iface_name, rt_log_op_iface_map);
    RTLogOPIfaceCtx *temp = NULL;
    RTLogOPIfaceCtx **head = &sc_lid->op_ifaces;
    int xret = -1;

    if (iface > RT_LOG_OP_IFACE_MAX) {
#ifdef DEBUG
        printf("Argument(s) to rt_logging_append_opifctx() NULL\n");
#endif
        return xret;
    }
    temp = *head;

    if (log_level > RT_LOG_NOTSET && log_level < RT_LOG_LEVEL_MAX){
        while (NULL != temp)
        {
            if (iface == temp->iface)
            {
                temp->log_level = log_level;
                xret = 0;
                break;
            }
            temp = temp->next;
        }
    }

    return xret;
}

RTLogLevel rt_logging_get_opifctx_level(const char *iface_name, RTLogInitData *sc_lid)
{
    unsigned int iface = rt_enum_n2v(iface_name, rt_log_op_iface_map);
    RTLogOPIfaceCtx *temp = NULL;
    RTLogOPIfaceCtx **head = &sc_lid->op_ifaces;
    RTLogLevel log_level = RT_LOG_NOTSET;

    if (iface > RT_LOG_OP_IFACE_MAX) {
#ifdef DEBUG
        printf("Argument(s) to rt_logging_append_opifctx() NULL\n");
#endif
        return log_level;
    }
    temp = *head;

    if (log_level > RT_LOG_NOTSET && log_level < RT_LOG_LEVEL_MAX){
        while (NULL != temp)
        {
            if (iface == temp->iface)
            {
                log_level = temp->log_level;
                break;
            }
            temp = temp->next;
        }
    }

    return log_level;
}

RTLogLevel rt_logging_get_default_level()
{
    return rt_logging_global_loglevel;
}


/**
 * \brief Creates a new output interface based on the arguments sent.  The kind
 *        of output interface to be created is decided by the iface_name arg.
 *        If iface_name is "file", the arg argument will hold the filename to be
 *        used for logging purposes.  If iface_name is "syslog", the arg
 *        argument holds the facility code.  If iface_name is "console", arg is
 *        NULL.
 *
 * \param iface_name Interface name.  Can be "console", "file" or "syslog"
 * \param log_format Override for the global_log_format
 * \param log_level  Override for the global_log_level
 * \param log_level  Parameter required by a particular interface.  Explained in
 *                   the function description
 *
 * \retval iface_ctx Pointer to the newly created output interface
 */
RTLogOPIfaceCtx *rt_logging_init_opifctx(const char *iface_name,
                                         const char *log_format,
                                         int log_level, char *arg,
                                         rt_transmit_callback transmit_callback)
{
    int iface = rt_enum_n2v(iface_name, rt_log_op_iface_map);

    if (log_level < RT_LOG_NONE || log_level > RT_LOG_DEBUG) {
#ifndef UNITTESTS
        printf("Warning: Supplied log_level_override for op_interface \"%s\" "
               "is invalid.  Defaulting to not specifing an override\n",
               iface_name);
#endif
        log_level = RT_LOG_NOTSET;
    }

    switch (iface) {
        case RT_LOG_OP_IFACE_CONSOLE:
            return rt_logging_init_console_opifctx(log_format, log_level);
        case RT_LOG_OP_IFACE_FILE:
            return rt_logging_init_file_opifctx(arg, log_format, log_level);
        case RT_LOG_OP_IFACE_SYSLOG:
            return rt_logging_init_syslog_opifctx(rt_enum_n2v(arg, RTSyslogGetFacilityMap()), log_format, log_level);
        case RT_LOG_OP_IFACE_LINK:
            return rt_logging_init_link_opifctx(arg, log_format, log_level, transmit_callback);
        default:
#ifdef DEBUG
            printf("Output Interface \"%s\" not supported by the logging module",
                   iface_name);
#endif
            return NULL;
    }
}

/**
 * \brief Initializes the logging module.
 *
 * \param sc_lid The initialization data for the logging module.  If sc_lid is
 *               NULL, we would stick to the default configuration for the
 *               logging subsystem.
 * \initonly
 */
void rt_logging_init(RTLogInitData *sc_lid)
{
    /* De-initialize the logging context, if it has already init by the
     * environment variables at the start of the engine */
    rt_logging_deinit();

#if defined (OS_WIN32)
    if (rt_mutex_init(&rt_logging_stream_lock, NULL) != 0) {
        rt_log_error(ERRNO_MQ_NO_MEMORY, "Failed to initialize log mutex.");
        exit(EXIT_FAILURE);
    }
#endif /* OS_WIN32 */

    /* rt_logging_config is a global variable */
    if ( (rt_logging_config = l_kmalloc(sizeof(RTLogConfig))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in rt_logging_init. Exiting...");
        exit(EXIT_FAILURE);
    }

    rt_logging_set_loglevel(sc_lid, rt_logging_config);
    rt_logging_set_format(sc_lid, rt_logging_config);
    rt_logging_set_opif(sc_lid, rt_logging_config);
    rt_logging_set_op_filter(sc_lid, rt_logging_config);

    rt_logging_module_initialized = 1;
    rt_logging_module_cleaned = 0;

    //RTOutputPrint(sc_did->startup_message);

    return;
}

void rt_logging_init_file(char *filename, char *dir, char *format,
                          RTLogLevel level, RTLogInitData *sc_lid)
{
    RTLogOPIfaceCtx *sc_iface_ctx = NULL;

    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    char *logfile = rt_logging_get_filename(dir, filename);
    sc_lid->startup_message = "File";
    sc_lid->global_log_level = level;
    sc_lid->global_log_format = format;

    sc_iface_ctx = rt_logging_init_opifctx("File", format, level,
                                           logfile, NULL);
    rt_logging_append_opifctx(sc_iface_ctx, sc_lid);
}


void rt_logging_init_console( char *format, RTLogLevel level, RTLogInitData* sc_lid)
{
    RTLogOPIfaceCtx *sc_iface_ctx = NULL;

    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    sc_lid->startup_message = "Console";
    sc_lid->global_log_level = level;
    sc_lid->global_log_format = format;

    sc_iface_ctx = rt_logging_init_opifctx("Console", format, level,
                                           NULL, NULL);
    rt_logging_append_opifctx(sc_iface_ctx, sc_lid);
}

void rt_logging_init_syslog(char *facility, char *format, RTLogLevel level, RTLogInitData* sc_lid)
{
    RTLogOPIfaceCtx *sc_iface_ctx = NULL;

    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    sc_lid->startup_message = "Syslog";
    sc_lid->global_log_level = level;
    sc_lid->global_log_format = format;

    sc_iface_ctx = rt_logging_init_opifctx("Syslog", format, level,
                                           facility, NULL);
    rt_logging_append_opifctx(sc_iface_ctx, sc_lid);
}

void rt_logging_int_link(char *facility, char *format, RTLogLevel level,
                         RTLogInitData* sc_lid, rt_transmit_callback transmit_callback)
{
    RTLogOPIfaceCtx *sc_iface_ctx = NULL;

    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    sc_lid->startup_message = "link";
    sc_lid->global_log_level = level;
    sc_lid->global_log_format = format;

    sc_iface_ctx = rt_logging_init_opifctx("link", format, level,
                                           facility, transmit_callback);
    rt_logging_append_opifctx(sc_iface_ctx, sc_lid);
}

void rt_logging_reinit_file (char *logdir)
{
	RTLogInitData *sc_lid = NULL;
	RTLogOPIfaceCtx *op_ifaces;
	char file[256] = {0}, realpath_file[256] = {0};
	FILE *fp = NULL;
	
	sc_lid = rt_logging_get_sclid ();
	op_ifaces = rt_logging_get_opifctx ("File", sc_lid);

	if (likely (op_ifaces)) {
		fp = op_ifaces->file_d;
		rt_logging_mkfile (file, 255);
		sprintf (realpath_file, "%s/%s", logdir, file);
		
		if(!rt_file_exsit(realpath_file)){
			if ((fp = fopen (realpath_file, "w+")) == NULL) {
				rt_log_error (ERRNO_SG,
					"fopen[%s]: %s\n", realpath_file, strerror(errno));
				goto finish;
			}
			memset (op_ifaces->file, 0, strlen (op_ifaces->file));
			memcpy (op_ifaces->file, realpath_file, strlen(realpath_file));
			fclose (op_ifaces->file_d);
			op_ifaces->file_d = fp;
		}
	}
finish:
	return;
}

void rt_logging_open (char *loglevel, char *logform, char *logdir)
{
	RTLogInitData *sc_lid = NULL;
	char file[256] = {0};
	char *facility = "local5";
	RTLogLevel level = RT_LOG_NOTSET;
	
    	sc_lid = rt_logging_sclid_init ();
		 
	rt_logging_mkfile(file, 255);

	level = rt_logging_parse_level (loglevel);
	rt_logging_init_console (logform, level, sc_lid);
	rt_logging_init_file (file, logdir, logform, level, sc_lid);
	rt_logging_init_syslog (facility, logform, level, sc_lid);

	rt_logging_init(sc_lid);
}

#if 0
/** Added by tsihang */
void rt_logging_init_console(RTLogLevel level)
{
    level = level;
    int result = 1;

    /* unset any environment variables set for the logging module */
    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    rt_logging_init(NULL);

    if (rt_logging_config == NULL)
        return ;

    result &= (RT_LOG_DEF_LOG_LEVEL == rt_logging_config->log_level);
    result &= (rt_logging_config->op_ifaces != NULL &&
               RT_LOG_DEF_LOG_OP_IFACE == rt_logging_config->op_ifaces->iface);
    result &= (rt_logging_config->log_format != NULL &&
               strcmp(RT_LOG_DEF_LOG_FORMAT, rt_logging_config->log_format) == 0);

    rt_logging_deinit();

    setenv(RT_LOG_ENV_LOG_LEVEL, "Debug", 1);
    setenv(RT_LOG_ENV_LOG_OP_IFACE, "Console", 1);
    //setenv(RT_LOG_ENV_LOG_FORMAT, "%n- %l", 1);
    setenv(RT_LOG_ENV_LOG_FORMAT, RT_LOG_DEF_LOG_FORMAT, 1);

    rt_logging_init(NULL);

    result &= (RT_LOG_DEBUG == rt_logging_config->log_level);
    result &= (rt_logging_config->op_ifaces != NULL &&
               RT_LOG_OP_IFACE_CONSOLE == rt_logging_config->op_ifaces->iface);
    result &= (rt_logging_config->log_format != NULL &&
               !strcmp("%n- %l", rt_logging_config->log_format));

    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    //rt_logging_deinit();

    return ;
}
#endif

/**
 * \brief Initializes the logging module if the environment variables are set.
 *        Used at the start of the engine, for cases, where there is an error
 *        in the yaml parsing code, and we want to enable the logging module.
 */
void rt_logging_init_from_env(void)
{
    RTLogConfig *sc_lc = NULL;
    char *s = NULL;
    const char *filter = NULL;
    int opts = 0;
    const char *ep;
    int eo = 0;
    RTLogOPIfaceCtx *op_ifaces_ctx = NULL;
    int op_iface = 0;
    const char *format = NULL;
    RTLogLevel log_level = RT_LOG_NOTSET;

    /* rt_logging_config is a global variable */
    if ( (rt_logging_config = l_kmalloc(sizeof(RTLogConfig))) == NULL)
        return;
    
    sc_lc = rt_logging_config;

    /* Check if the user has set the op_iface env var.  Only if it is set,
     * we proceed with the initialization */
    s = getenv(RT_LOG_ENV_LOG_OP_IFACE);
    if (s != NULL) {
        op_iface = rt_enum_n2v(s, rt_log_op_iface_map);

        if(op_iface < 0 || op_iface >= RT_LOG_OP_IFACE_MAX) {
            op_iface = RT_LOG_DEF_LOG_OP_IFACE;
#ifndef UNITTESTS
            printf("Warning: Invalid output interface supplied by user.  "
                   "Falling back on default_output_interface \"%s\"\n",
                   rt_enum_v2n(op_iface, rt_log_op_iface_map));
#endif
        }
    } else {
        rt_logging_free_config(sc_lc);
        rt_logging_config = NULL;
        return;
    }

    switch (op_iface) {
        case RT_LOG_OP_IFACE_CONSOLE:
            op_ifaces_ctx = rt_logging_init_console_opifctx(NULL, -1);
            break;
        case RT_LOG_OP_IFACE_FILE:
            s = getenv(RT_LOG_ENV_LOG_FILE);
            if (s == NULL)
                s = rt_logging_get_filename(NULL, RT_LOG_DEF_LOG_FILE);
            op_ifaces_ctx = rt_logging_init_file_opifctx(s, NULL, -1);
            break;
        case RT_LOG_OP_IFACE_SYSLOG:
            s = getenv(RT_LOG_ENV_LOG_FACILITY);
            if (s == NULL)
                s = RT_LOG_DEF_SYSLOG_FACILITY_STR;

            op_ifaces_ctx = rt_logging_init_syslog_opifctx(rt_enum_n2v(s, RTSyslogGetFacilityMap()), NULL, -1);
            break;
    }
    sc_lc->op_ifaces = op_ifaces_ctx;


    /* Set the filter */
    filter = getenv(RT_LOG_ENV_LOG_OP_FILTER);
    if (filter != NULL && strcmp(filter, "") != 0) {
        sc_lc->op_filter_regex = pcre_compile(filter, opts, &ep, &eo, NULL);
        if (sc_lc->op_filter_regex == NULL) {
            printf("pcre compile of \"%s\" failed at offset %d : %s\n", filter,
                   eo, ep);
            return;
        }

        sc_lc->op_filter_regex_study = pcre_study(sc_lc->op_filter_regex, 0,
                                                  &ep);
        if (ep != NULL) {
            printf("pcre study failed: %s\n", ep);
            return;
        }
    }

    /* Set the log_format */
    format = getenv(RT_LOG_ENV_LOG_FORMAT);
    if (format == NULL || strlen(format) > RT_LOG_MAX_LOG_FORMAT_LEN) {
        format = RT_LOG_DEF_LOG_FORMAT;
#ifndef UNITTESTS
        printf("Warning: Invalid global_log_format supplied by user or format "
               "length exceeded limit of \"%d\" characters.  Falling back on "
               "default log_format \"%s\"\n", RT_LOG_MAX_LOG_FORMAT_LEN,
               format);
#endif
    }

    if (format != NULL &&
        (sc_lc->log_format = strdup(format)) == NULL) {
        printf("Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    /* Set the log_level */
    s = getenv(RT_LOG_ENV_LOG_LEVEL);
    if (s != NULL)
        log_level = rt_enum_n2v(s, rt_loglevel_map);

    if (log_level >= 0 && log_level < RT_LOG_LEVEL_MAX)
        sc_lc->log_level = log_level;
    else {
        sc_lc->log_level = RT_LOG_DEF_LOG_LEVEL;
#ifndef UNITTESTS
        printf("Warning: Invalid global_log_level assigned by user.  Falling "
               "back on default_log_level \"%s\"\n",
               rt_enum_v2n(sc_lc->log_level, rt_loglevel_map));
#endif
    }

    /* we also set it to a global var, as it is easier to access it */
    rt_logging_global_loglevel = sc_lc->log_level;

    rt_logging_module_initialized = 1;
    rt_logging_module_cleaned = 0;

    return;
}

/**
 * \brief Returns a full file path given a filename uses log dir specified in
 *        conf or DEFAULT_LOG_DIR
 *
 * \param filearg The relative filename for which we want a full path include
 *                log directory
 *
 * \retval log_filename The fullpath of the logfile to open
 */
static char *rt_logging_get_filename(const char* dir, const char *filearg)
{
    const char *log_dir;
    char *log_filename;

    if (dir != NULL){
        log_dir = dir;
    }else{
        log_dir = DEFAULT_LOG_DIR;
    }

    if (!rt_dir_exsit(log_dir)){
        char mkdir[128] = {0};
        char chmod[256] = {0};
        snprintf (mkdir, 128 - 1, "mkdir -p %s", log_dir);
        do_system (mkdir);
        snprintf (chmod, 128 - 1, "chmod 775 %s", log_dir);
        do_system(chmod);
    }

    log_filename = l_kmalloc(PATH_MAX);
    if (log_filename == NULL)
        return NULL;
    snprintf(log_filename, PATH_MAX, "%s/%s", log_dir, filearg);

    return log_filename;
}

/**
 * \brief De-Initializes the logging module
 */
void rt_logging_deinit(void)
{
    rt_logging_free_config(rt_logging_config);

    /* reset the global logging_module variables */
    rt_logging_global_loglevel = 0;
    rt_logging_module_initialized = 0;
    rt_logging_module_cleaned = 1;
    rt_logging_config = NULL;

    /* de-init the FD filters */
    RTLogReleaseFDFilters();
    /* de-init the FG filters */
    RTLogReleaseFGFilters();

#if defined (OS_WIN32)
	if (rt_logging_stream_lock != NULL) {
		rt_mutex_destroy(&rt_logging_stream_lock);
		rt_logging_stream_lock = NULL;
	}
#endif /* OS_WIN32 */

    return;
}

//------------------------------------Unit_Tests--------------------------------

/* The logging engine should be tested to the maximum extent possible, since
 * logging code would be used throughout the codebase, and hence we can't afford
 * to have a single bug here(not that you can afford to have a bug
 * elsewhere ;) ). Please report a bug, if you get a slightest hint of a bug
 * from the logging module.
 */

int RTLogTestInit01()
{
    int result = 1;

    /* unset any environment variables set for the logging module */
    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    rt_logging_init(NULL);

    if (rt_logging_config == NULL)
        return 0;

    result &= (RT_LOG_DEF_LOG_LEVEL == rt_logging_config->log_level);
    result &= (rt_logging_config->op_ifaces != NULL &&
               RT_LOG_DEF_LOG_OP_IFACE == rt_logging_config->op_ifaces->iface);
    result &= (rt_logging_config->log_format != NULL &&
               strcmp(RT_LOG_DEF_LOG_FORMAT, rt_logging_config->log_format) == 0);

    rt_logging_deinit();

    setenv(RT_LOG_ENV_LOG_LEVEL, "Debug", 1);
    setenv(RT_LOG_ENV_LOG_OP_IFACE, "Console", 1);
    setenv(RT_LOG_ENV_LOG_FORMAT, "%n- %l", 1);

    rt_logging_init(NULL);
    rt_log_info("xxxxxxxxxxxxxxxx\n");
    
    result &= (RT_LOG_DEBUG == rt_logging_config->log_level);
    result &= (rt_logging_config->op_ifaces != NULL &&
               RT_LOG_OP_IFACE_CONSOLE == rt_logging_config->op_ifaces->iface);
    result &= (rt_logging_config->log_format != NULL &&
               !strcmp("%n- %l", rt_logging_config->log_format));

    unsetenv(RT_LOG_ENV_LOG_LEVEL);
    unsetenv(RT_LOG_ENV_LOG_OP_IFACE);
    unsetenv(RT_LOG_ENV_LOG_FORMAT);

    rt_logging_deinit();

    return result;
}

int RTLogTestInit02()
{
    RTLogInitData *sc_lid = NULL;
    RTLogOPIfaceCtx *sc_iface_ctx = NULL;
    int result = 1;
    char *logfile = rt_logging_get_filename(NULL, "boo.txt");
    sc_lid = rt_logging_alloc();
    if (sc_lid == NULL)
        return 0;
    sc_lid->startup_message = "Test02";
    sc_lid->global_log_level = RT_LOG_DEBUG;
    sc_lid->op_filter = "boo";
    sc_iface_ctx = rt_logging_init_opifctx("file", "%m - %d", RT_LOG_ALERT,
                                       logfile, NULL);
    rt_logging_append_opifctx(sc_iface_ctx, sc_lid);
    sc_iface_ctx = rt_logging_init_opifctx("console", NULL, RT_LOG_ERROR,
                                       NULL, NULL);
    rt_logging_append_opifctx(sc_iface_ctx, sc_lid);

    rt_logging_init(sc_lid);

    if (rt_logging_config == NULL)
        return 0;

    result &= (RT_LOG_DEBUG == rt_logging_config->log_level);
    result &= (rt_logging_config->op_ifaces != NULL &&
               RT_LOG_OP_IFACE_FILE == rt_logging_config->op_ifaces->iface);
    result &= (rt_logging_config->op_ifaces != NULL &&
               rt_logging_config->op_ifaces->next != NULL &&
               RT_LOG_OP_IFACE_CONSOLE == rt_logging_config->op_ifaces->next->iface);
    result &= (rt_logging_config->log_format != NULL &&
               strcmp(RT_LOG_DEF_LOG_FORMAT, rt_logging_config->log_format) == 0);
    result &= (rt_logging_config->op_ifaces != NULL &&
               rt_logging_config->op_ifaces->log_format != NULL &&
               strcmp("%m - %d", rt_logging_config->op_ifaces->log_format) == 0);
    result &= (rt_logging_config->op_ifaces != NULL &&
               rt_logging_config->op_ifaces->next != NULL &&
               rt_logging_config->op_ifaces->next->log_format == NULL);

    rt_logging_deinit();

    sc_lid = rt_logging_alloc();
    if (sc_lid == NULL)
        return 0;
    sc_lid->startup_message = "Test02";
    sc_lid->global_log_level = RT_LOG_DEBUG;
    sc_lid->op_filter = "boo";
    sc_lid->global_log_format = "kaboo";

    rt_logging_init(sc_lid);

    if (rt_logging_config == NULL)
        return 0;

    result &= (RT_LOG_DEBUG == rt_logging_config->log_level);
    result &= (rt_logging_config->op_ifaces != NULL &&
               RT_LOG_OP_IFACE_CONSOLE == rt_logging_config->op_ifaces->iface);
    result &= (rt_logging_config->op_ifaces != NULL &&
               rt_logging_config->op_ifaces->next == NULL);
    result &= (rt_logging_config->log_format != NULL &&
               strcmp("kaboo", rt_logging_config->log_format) == 0);
    result &= (rt_logging_config->op_ifaces != NULL &&
               rt_logging_config->op_ifaces->log_format == NULL);
    result &= (rt_logging_config->op_ifaces != NULL &&
               rt_logging_config->op_ifaces->next == NULL);

    rt_logging_deinit();

    return result;
}

int RTLogTestInit03()
{
    int result = 1;

    rt_logging_init(NULL);

    RTLogAddFGFilterBL(NULL, "bamboo", -1);
    RTLogAddFGFilterBL(NULL, "soo", -1);
    RTLogAddFGFilterBL(NULL, "dummy", -1);

    result &= (RTLogPrintFGFilters() == 3);

    RTLogAddFGFilterBL(NULL, "dummy1", -1);
    RTLogAddFGFilterBL(NULL, "dummy2", -1);

    result &= (RTLogPrintFGFilters() == 5);

    rt_logging_deinit();

    return result;
}

int RTLogTestInit04()
{
    int result = 1;

    rt_logging_init(NULL);

    RTLogAddFDFilter("bamboo");
    RTLogAddFDFilter("soo");
    RTLogAddFDFilter("foo");
    RTLogAddFDFilter("roo");

    result &= (RTLogPrintFDFilters() == 4);

    RTLogAddFDFilter("loo");
    RTLogAddFDFilter("soo");

    result &= (RTLogPrintFDFilters() == 5);

    RTLogRemoveFDFilter("bamboo");
    RTLogRemoveFDFilter("soo");
    RTLogRemoveFDFilter("foo");
    RTLogRemoveFDFilter("noo");

    result &= (RTLogPrintFDFilters() == 2);

    rt_logging_deinit();

    return result;
}

int RTLogTestInit05()
{
    int result = 1;

    rt_log_info("ssssssssssssssss\n");

    return result;
}


