#include "sysdefs.h"
#include "rt_yaml.h"
#include "conf_yaml_loader.h"
#include "conf.h"
#include "vrs_pkt.h"
#include "vpw_init.h"
#include "vrs_session.h"

#define __ma_port_default__     2001
#define __sguard_port_default__ 2000
#define __localhost_ipaddr__    "127.0.0.1"

extern int libserial_num();

typedef struct
{
    int sn;                                     /* process sequence num */
    pid_t pid;                                  /* process ID */

    int ma_sock;                                /* ma socket */
    int ma_port;                                /* ma port */
    atomic_t  m_flags;
    char *conf_path;
    int (*ma_register)(int, pid_t,int);              /* ma register function */
    void (*ma_cmd_func)(int, char*,int);             /* ma command process function */

    int sguard_sock;                            /* sguard socket */
    int sguard_port;                            /* sguard port */
#ifdef RT_TMR_ADVANCED
    uint32_t sguard_timer;                      /* timer id for sguard */
#else
    int sguard_timer;                           /* timer id for sguard */
#endif
    atomic_t  s_flags;
    int (*sguard_register)(int, pid_t,int);          /* sguard register function */
    void (*sguard_ka_func)(uint32_t,int,char**);         /* sguard keepalive function */
    int ka_interval;                            /* keepalive interval for sguard */
}ma_mgmt_st;


static int libIndex = 0;
static int link_state = 0;

#define DMS_CONFIG_FILE         "/usr/local/etc/dms.yaml"

#define MA_CMD_RELOAD_CONFIG          23
#define MA_CMD_GET_COUNTER            19
#define MA_CMD_DEBUG_ON               30
#define MA_CMD_DEBUG_OFF              31
#define MA_CMD_LOG_LEVEL              32
#define MA_CMD_SHOW_LEVEL             33
#define MA_CMD_DEBUG_MASK             34
#define MA_CMD_SHOW_DEBUGMASK         35
#define MA_CMD_COUNTER_STRUCT         39

#define MA_CMDTYPE_VRS    3
#define LOG_OP_IFACE   "link"

int load_ma_config(int reload);

struct yaml_filename_val_map
{
    const char *enum_name;
    int (*reload_config)(int);
};

struct yaml_filename_val_map yaml_filename_table[] = {
    {"dms.yaml",            &load_ma_config},
    {"vrs.yaml",            &load_public_config},
    {"vrsrules.conf",       &load_rule_and_model},
    {"vpw.yaml",            &load_private_vpw_config},
    {NULL,                  NULL},
};

struct yaml_filename_val_map *get_val_map()
{
    return yaml_filename_table;
}

static int vpw_reload_config(uint8_t *pkt_data, int pkt_datalen)
{
#define FILENAME_LEN 256
    uint16_t r_type;
    uint16_t r_len;
    char filename[FILENAME_LEN];
    int reload = 1;
    struct yaml_filename_val_map *yaml_table = get_val_map();

    if (NULL == pkt_data || 0 == pkt_datalen){
        goto finish;
    }

    while (pkt_datalen >= 4) {
        /** Get data length */
        r_type = NTOHS(*(uint16_t *)pkt_data);
        r_len = NTOHS(*(uint16_t *)(pkt_data + 2));
        pkt_data += 4;
        pkt_datalen -= 4;

        if (pkt_datalen < r_len || r_len > FILENAME_LEN || r_len <= 8) {
            goto finish;
        }
        if (r_type == 1){
            memcpy(filename, pkt_data, r_len);
            filename[r_len + 1] = '\0';
            for (; yaml_table->enum_name != NULL; yaml_table++) {
                if (!STRNCMP(filename, yaml_table->enum_name, strlen(yaml_table->enum_name))){
                    yaml_table->reload_config(reload);
                    break;
                }
            }
            if (!yaml_table->enum_name){
                rt_log_error(ERRNO_RANGE, "reload config file %s error", filename);
            }
        }else{
            goto finish;
        }

        pkt_data += r_len;
        pkt_datalen -= r_len;
    }

finish:
    return 0;
}

static __rt_always_inline__ void
vrs_stat_hton(st_count_t* total_count, st_count_t _total_count)
{
    total_count->voice_pkts = HTONLL(_total_count.voice_pkts);    ;
    total_count->other_pkts = HTONLL(_total_count.other_pkts);
    total_count->lost_pkts = HTONLL(_total_count.lost_pkts);
    total_count->over_pkts = HTONLL(_total_count.over_pkts);
    total_count->onlines = HTONLL(_total_count.onlines);
    total_count->end_num = HTONLL(_total_count.end_num);
    total_count->star_num = HTONLL(_total_count.star_num);
    total_count->stop_vdu_sig = HTONLL(_total_count.stop_vdu_sig);
}

static int vpw_get_counter(char *rsp, int sn)
{
    int rsp_len = 0;
    st_count_t total_count, _total_count;

    memset(&total_count, 0, sizeof(st_count_t));
    memset(&total_count, 0, sizeof(st_count_t));

    vrs_stat_read(&_total_count);
    vrs_stat_hton(&total_count, _total_count);
    rsp_len = fill_ma_counter ((uint8_t *)rsp, sn,  MA_CMDTYPE_VRS, (uint8_t *)&total_count, sizeof(st_count_t));

    return rsp_len;
}

static int vpw_get_counter_element(char *rsp,
                                  uint16_t num)
{
    int rsp_len = 0;
    struct count_struct count_element[8] = {
        {"voice_pkts",   sizeof(uint64_t)},
        {"other_pkts",   sizeof(uint64_t)},
        {"lost_pkts",    sizeof(uint64_t)},
        {"over_pkts",    sizeof(uint64_t)},
        {"onlines",      sizeof(uint64_t)},
        {"star_num",     sizeof(uint64_t)},
        {"end_num",      sizeof(uint64_t)},
        {"stop_vdu_sig", sizeof(uint64_t)},
    };

    rsp_len = fill_ma_count_info_struct((unsigned char *)rsp, num, "vpw", "vpw", 2, count_element);
    return rsp_len;
}

void log_format_print(int sock, char *msg)
{
    char rsp[__ma_pkt_size__] = {0};
    int rsp_len = 0, wl = 0;

    rt_log_info("[vpw-work]log msg: %s", msg);
    rsp_len = fill_ma_log_packet((uint8_t *)rsp, msg);
    wl = rt_sock_send(sock, rsp, rsp_len);
    if (wl < 0 && (wl != rsp_len)){
        rt_log_info("[vpw-vork]Log msg to ma: %s\n", strerror(errno));
    }
}

static void rt_log_append_link(int sock, char *format, RTLogLevel level)
{
    RTLogInitData *sc_lid = NULL;

    sc_lid = rt_logging_get_sclid();
    if (sc_lid != NULL && !link_state && sock > 0){
        rt_logging_int_link((char *)&sock, format, level, sc_lid, log_format_print);
        link_state = 1;
    }
}

static void rt_log_delete_link(int __attribute__((__unused__))sock, const char *iface_name)
{
    RTLogInitData *sc_lid = NULL;

    sc_lid = rt_logging_get_sclid();
    if (sc_lid != NULL){
        rt_logging_delete_opifctx(iface_name, sc_lid);
        link_state = 0;
    }
}

static int rt_log_set_level(uint8_t *pkt_data, uint16_t pkt_datalen, const char *iface_name)
{

    RTLogInitData *sc_lid = NULL;
    RTLogLevel log_level = RT_LOG_NOTSET;
    int xret = -1;

    if( pkt_data == NULL || pkt_datalen != sizeof(int)){
        rt_log_error(ERRNO_INVALID_ARGU, "Invalid log level received by ma ");
        return xret;
    }
    log_level = *((int *)pkt_data);

    sc_lid = rt_logging_get_sclid();
    if (sc_lid != NULL){
        xret = rt_logging_set_opifctx_level(iface_name, sc_lid, log_level);
        if (xret){
            rt_log_error(ERRNO_INVALID_VAL, "Set log level(%d) failed", log_level);
        }
    }

    return xret;
}

static int rt_log_get_level(char *rsp, uint16_t num, const char *iface_name)
{

    RTLogInitData *sc_lid = NULL;
    RTLogLevel log_level = RT_LOG_NOTSET;
    int rsp_len = -1;

    sc_lid = rt_logging_get_sclid();
    if (sc_lid != NULL){
        log_level = rt_logging_get_opifctx_level(iface_name, sc_lid);
    }
    rsp_len = fill_loglevel_packet(rsp, num, log_level);

    return rsp_len;
}

void VpwMaCommandProc(int sock, char *req, int req_len)
{
    uint16_t header_len = sizeof(struct pkt_header);
    uint8_t *pkt_data;
    uint16_t pkt_datalen;
    int rsp_len = 0;
    char rsp[__ma_pkt_size__] = {0};
    int wl = 0;
    struct pkt_header *header = NULL;
    char cmd_desc[32] = {0};
    
    if (req_len < (int)sizeof(struct pkt_header)
        || req == NULL){
        rt_log_error (ERRNO_MA_INTERACTIVE, "Header invalid\n");
        goto finish;
    }

    header = (struct pkt_header *)req;
    pkt_data = (uint8_t *)req + header_len;
    pkt_datalen = NTOHS(header->len) - header_len;

    switch(NTOHS(header->cmd))
    {
        case MA_CMD_RELOAD_CONFIG:
            sprintf (cmd_desc, "%s(%d)", "Reload", MA_CMD_RELOAD_CONFIG);
            vpw_reload_config(pkt_data, pkt_datalen);
            goto finish;
        case MA_CMD_GET_COUNTER:
         sprintf (cmd_desc, "%s(%d)", "Get-Counter", MA_CMD_GET_COUNTER);
            rsp_len = vpw_get_counter(rsp, NTOHS(header->num));
            goto response;
        case MA_CMD_COUNTER_STRUCT:
            sprintf (cmd_desc, "%s(%d)", "Get-Counter-Struct", MA_CMD_COUNTER_STRUCT);
            rsp_len = vpw_get_counter_element(rsp, NTOHS(header->num));
            goto response;
        case MA_CMD_DEBUG_ON:
            sprintf (cmd_desc, "%s(%d)", "Debug-ON", MA_CMD_DEBUG_ON);
            rt_log_append_link(sock, "[%i] %t - (%f:%l) <%d> (%n) -- ", RT_LOG_DEBUG);
            goto finish;
        case MA_CMD_DEBUG_OFF:
         sprintf (cmd_desc, "%s(%d)", "Debug-OFF", MA_CMD_DEBUG_OFF);
            rt_log_delete_link(sock, LOG_OP_IFACE);
            goto finish;
        case MA_CMD_LOG_LEVEL:
         sprintf (cmd_desc, "%s(%d)", "Debug-LEVEL", MA_CMD_LOG_LEVEL);
            rt_log_set_level(pkt_data, pkt_datalen, LOG_OP_IFACE);
            goto finish;
        case MA_CMD_DEBUG_MASK:
         sprintf (cmd_desc, "%s(%d)", "Debug-MASK", MA_CMD_DEBUG_MASK);
            goto finish;
        case MA_CMD_SHOW_LEVEL:
         sprintf (cmd_desc, "%s(%d)", "Debug-SHOWLEVE", MA_CMD_SHOW_LEVEL);
            rsp_len = rt_log_get_level(rsp, NTOHS(header->num), LOG_OP_IFACE);
            goto response;
        case MA_CMD_SHOW_DEBUGMASK:
        sprintf (cmd_desc, "%s(%d)", "Debug-SHOWMASK", MA_CMD_SHOW_DEBUGMASK);
            goto response;
        default:
        sprintf (cmd_desc, "%s(%d)", "Unknown", NTOHS(header->cmd));
            break;
    }

response:
    wl = rt_sock_send(sock, rsp, rsp_len);
    if (wl > 0 && (wl == rsp_len)){
        rt_log_debug("Send response to ma success");
    }

finish:
    rt_log_info("Receive Command \"%s\" from MA", cmd_desc);
    return;
}

static __rt_always_inline__ int VpwRegisterToMa(int ma_sock, pid_t pid, int sn)
{
    int ret = 0;
    int pkt_len = 0;
    char packet[__ma_pkt_size__] = {0};
    pkt_len = fill_regma_packet((uint8_t *)&packet[0], pid, sn);
    ret = rt_sock_send(ma_sock, packet, pkt_len);
    if (ret <= 0){
        rt_log_error(ERRNO_MA_REGISTRY, "To ma: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
static __rt_always_inline__ int VpwRegisterToSguard(int sguard_sock, pid_t pid, int sn)
{
    int ret = 0;
    int pkt_len = 0;
    char packet[__ma_pkt_size__] = {0};
    struct vpw_t *vpw = vrs_default_trapper()->vpw;

    pkt_len = fill_regsguard_packet((uint8_t *)&packet[0], pid, sn, vpw->id, NULL, 0);
    ret = rt_sock_send(sguard_sock, packet, pkt_len);
    if (ret <= 0){
        rt_log_error(ERRNO_MA_REGISTRY, "To Sguard: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
static __rt_always_inline__ void
VpwKaliveToSguard(uint32_t __attribute__((__unused__)) uid,
                        int  __attribute__((__unused__)) argc,
                        char __attribute__((__unused__)) **argv )
{
    int ret = 0, sguard_sock = -1;
    int pkt_len = 0;
    char packet[__ma_pkt_size__] = {0};

    sguard_sock = *((int *)argv[0]);
    pkt_len = fill_alive_packet((uint8_t *)&packet[0], 1);

    ret = rt_sock_send(sguard_sock, packet, pkt_len);
    if (ret <= 0){
        rt_log_error(ERRNO_MA_KEEPALIVE, "To Sguard: %s\n", strerror(errno));
    }
    return;
}

static __rt_always_inline__ int
ma_config_from_yaml(ma_mgmt_st *m)
{
    int xret = -1;
    assert (m);
    ConfNode *interface = ConfGetNode("interface");
    ConfNode *iobj = NULL;

    TAILQ_FOREACH(iobj, &interface->head, next){
        if(!STRCMP(iobj->name, "ma")){
            ConfNode *obj = NULL;
            char *conf_path = NULL;
            TAILQ_FOREACH(obj, &iobj->head, next){
                if(ConfGetChildValue(obj, "conf-path", &conf_path)){
                    m->conf_path = strdup(conf_path);
                    xret = 0;
                    goto finish;
                }
            }
        }
    }
finish:
    return xret;
}

void *ma_do_proc(void *args)
{
    int ret, done = 0;
    struct timeval timeout;
    fd_set f_set;
    char *request = NULL;
    ma_mgmt_st *ma_mgmt = (ma_mgmt_st *)args;

    request = (char *)kmalloc(__ma_pkt_size__, MPF_CLR, -1);
    if (!request){
        return NULL;
    }

    FOREVER {
        do{
            ma_mgmt->ma_sock = rt_clnt_sock(libIndex, __localhost_ipaddr__, ma_mgmt->ma_port, AF_INET);
            sleep(3);
        }while(ma_mgmt->ma_sock < 0);

        if (0 != ma_mgmt->ma_register (ma_mgmt->ma_sock, ma_mgmt->pid, ma_mgmt->sn)){
            continue;
        }

        done = 0;
        do{
            if (atomic_read(&ma_mgmt->m_flags)){
                close(ma_mgmt->ma_sock);
                ma_mgmt->ma_sock = -1;
                atomic_set(&ma_mgmt->m_flags, 0);
                break;
            }

            FD_ZERO(&f_set);
            FD_SET(ma_mgmt->ma_sock, &f_set);
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            ret = select(ma_mgmt->ma_sock + 1, &f_set, NULL, NULL, &timeout);
            if (ret < 0 && !ERRNO_EQUAL(EINTR)){
                rt_log_error(ERRNO_SOCK_SELECT, "select: %s", strerror(errno));
                break;
            }else{
                if (ret > 0 &&
                    FD_ISSET(ma_mgmt->ma_sock, &f_set)){
                        int rsize = rt_sock_recv(ma_mgmt->ma_sock, request, __ma_pkt_size__);
                        if (rsize <= 0){
                            rt_log_warning(ERRNO_SOCK_RECV, "rsize=%d, sock=%d, %s.", rsize, ma_mgmt->ma_sock,
                            (rsize == 0) ? "Connection closed" : strerror(errno));
                            break;
                        }
                        ma_mgmt->ma_cmd_func(ma_mgmt->ma_sock, request, rsize);
                }
            }
        }while(!done);

        close(ma_mgmt->ma_sock);
        ma_mgmt->ma_sock = -1;
    }
    task_deregistry_id(pthread_self());

    return NULL;
}

void *sguard_do_proc(void *args)
{
    int ret, done = 0;
    fd_set f_set;
    struct timeval timeout;
    char request[__ma_pkt_size__] = {0};

    ma_mgmt_st *ma_mgmt = (ma_mgmt_st *)args;

    int *p = &ma_mgmt->sguard_sock;

#ifdef RT_TMR_ADVANCED
    ma_mgmt->sguard_timer = tmr_create(BackEndSystem, "sguard keepalive", TMR_PERIODIC,
                                       ma_mgmt->sguard_ka_func, 1, (char **)&p, ma_mgmt->ka_interval);
#else
    ma_mgmt->sguard_timer = rt_timer_create(BackEndSystem, TYPE_LOOP);
#endif

    FOREVER{
        do{
#ifdef RT_TMR_ADVANCED
            tmr_stop(ma_mgmt->sguard_timer);
#else
            rt_timer_stop(ma_mgmt->sguard_timer);
#endif
            ma_mgmt->sguard_sock = rt_clnt_sock(libIndex, __localhost_ipaddr__, ma_mgmt->sguard_port, AF_INET);
            sleep(3);
        }while(ma_mgmt->sguard_sock < 0);

        if (0 != ma_mgmt->sguard_register (ma_mgmt->sguard_sock, ma_mgmt->pid, ma_mgmt->sn)){
            continue;
        }
#ifdef RT_TMR_ADVANCED
        tmr_start(ma_mgmt->sguard_timer);
#else
        rt_timer_start(ma_mgmt->sguard_timer, ma_mgmt->ka_interval, ma_mgmt->sguard_ka_func, 1, (char **)&p);
#endif
        done = 0;
        do{
            if (atomic_read(&ma_mgmt->s_flags)){
                close(ma_mgmt->sguard_sock);
                ma_mgmt->sguard_sock = -1;
                atomic_set(&ma_mgmt->s_flags, 0);
                break;
            }

            FD_ZERO(&f_set);
            FD_SET(ma_mgmt->sguard_sock, &f_set);
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            ret = select(ma_mgmt->sguard_sock + 1, &f_set, NULL, NULL, &timeout);
            if (ret < 0 && !ERRNO_EQUAL(EINTR)){
                rt_log_error(ERRNO_SOCK_SELECT, "select: %s", strerror(errno));
                break;
            }else{
                if (ret > 0 &&
                    FD_ISSET(ma_mgmt->sguard_sock, &f_set)){
                    int rsize = rt_sock_recv(ma_mgmt->sguard_sock, request, __ma_pkt_size__);
                    if (rsize <= 0){
                        rt_log_warning(ERRNO_SOCK_RECV, "rsize=%d, sock=%d, %s.", rsize, ma_mgmt->sguard_sock,
                        (rsize == 0) ? "Connection closed" : strerror(errno));
                        break;
                    }
                }
            }
        }while(!done);

        close(ma_mgmt->sguard_sock);
        ma_mgmt->sguard_sock = -1;
    }
    task_deregistry_id(pthread_self());

    return NULL;
}

static ma_mgmt_st pst_ma_mgmt = {
    .sn = -1,
    .ma_sock = -1,
    .ma_port = -1,
    .ma_register = &VpwRegisterToMa,
    .ma_cmd_func = &VpwMaCommandProc,

    .sguard_sock = -1,
    .sguard_port = -1,
    .sguard_timer = -1,
    .ka_interval = 1,
    .sguard_register = &VpwRegisterToSguard,
    .sguard_ka_func = &VpwKaliveToSguard,
};

/* read sguard port && ma port */
int load_ma_config(int reload)
{
    int xret = -1;
    int ma_port = 0, sguard_port = 0;

    yaml_init();
    /* ma allocate file */
    if (ConfYamlLoadFile(DMS_CONFIG_FILE)){
        rt_log_error(ERRNO_MA_YAML_NO_FILE, "%s\n", DMS_CONFIG_FILE);
        goto finish;
    }

    /* get ma listen port */
    ConfYamlReadInt("ma.maport", &ma_port);
    if ((ma_port < 1024) || (ma_port > 65535))
        ma_port = __ma_port_default__;
    if (ma_port != pst_ma_mgmt.ma_port && reload){
        atomic_set(&(pst_ma_mgmt.m_flags), 1);
    }
    pst_ma_mgmt.ma_port = ma_port;

    ConfYamlReadInt("ma.sguardport", &sguard_port);
    if ((sguard_port < 1024) || (sguard_port > 65535))
        sguard_port = __sguard_port_default__;
    if (sguard_port != pst_ma_mgmt.sguard_port && reload)
        atomic_set(&(pst_ma_mgmt.s_flags), 1);
    pst_ma_mgmt.sguard_port = sguard_port;

    rt_log_info("Management Agent:");
    rt_log_info("  Port: %d", pst_ma_mgmt.ma_port);
    rt_log_info("  Sguard port %d", pst_ma_mgmt.sguard_port);

    xret = 0;
finish:
    return xret;
}

static struct rt_task_t vpw_manage_task =
{
    .module = THIS,
    .name = "vpw manage task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &pst_ma_mgmt,
    .routine = ma_do_proc,
};

static struct rt_task_t vpw_sguard_task =
{
    .module = THIS,
    .name = "vpw sguard task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &pst_ma_mgmt,
    .routine = sguard_do_proc,
};

void vpw_dms_init()
{
    int reload = 0;
    int sn = 0;
    ma_mgmt_st *ma_mgmt = &pst_ma_mgmt;
    struct vpw_t *vpw = vrs_default_trapper()->vpw;

    sn = vpw->serial_num;
    if ((sn != -1) && !load_ma_config(reload))
    {
        ma_mgmt->sn = sn;
        ma_mgmt->pid = getpid();

        task_registry(&vpw_manage_task);
        task_registry(&vpw_sguard_task);
    }
}

