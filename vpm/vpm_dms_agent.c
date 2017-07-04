#include "sysdefs.h"
#include "rt_yaml.h"
#include "conf_yaml_loader.h"
#include "conf.h"
#include "vrs_pkt.h"
#include "vpm_init.h"
#include "vrs.h"

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
}vpm_ma_mgmt_st;

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

extern int load_vpm_ma_config(int reload);

static __rt_always_inline__ int VpmRegisterToSguard(int sguard_sock, pid_t pid, int sn)
{
    int ret = 0;
    int pkt_len = 0;
    char packet[__ma_pkt_size__] = {0};
    struct vpm_t *vpm = vrs_default_trapper()->vpm;

    pkt_len = fill_regsguard_packet((uint8_t *)&packet[0], pid, sn, vpm->id, NULL, 0);
    ret = rt_sock_send(sguard_sock, packet, pkt_len);
    if (ret <= 0){
        rt_log_error(ERRNO_MA_REGISTRY, "To Sguard: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static __rt_always_inline__ void
VpmKaliveToSguard(uint32_t __attribute__((__unused__)) uid,
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

static __rt_always_inline__ int VpmRegisterToMa(int ma_sock, pid_t pid, int sn)
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

struct yaml_filename_val_map
{
    const char *enum_name;
    int (*reload_config)(int);
};

struct yaml_filename_val_map yaml_filename_table[] = {
    {"dms.yaml",            &load_vpm_ma_config},
    {"vrs.yaml",            &load_public_config},
    {"vpm.yaml",            &load_private_vpm_config},
    {NULL,                  NULL},
};

struct yaml_filename_val_map *get_val_map()
{
    return yaml_filename_table;
}

static int vpm_reload_config(uint8_t *pkt_data, int pkt_datalen)
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


void vpm_log_format_print(int sock, char *msg)
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

static void vpm_log_append_link(int sock, char *format, RTLogLevel level)
{
    RTLogInitData *sc_lid = NULL;

    sc_lid = rt_logging_get_sclid();
    if (sc_lid != NULL && !link_state && sock > 0){
        rt_logging_int_link((char *)&sock, format, level, sc_lid, vpm_log_format_print);
        link_state = 1;
    }
}

static void vpm_log_delete_link(int __attribute__((__unused__))sock, const char *iface_name)
{
    RTLogInitData *sc_lid = NULL;

    sc_lid = rt_logging_get_sclid();
    if (sc_lid != NULL){
        rt_logging_delete_opifctx(iface_name, sc_lid);
        link_state = 0;
    }
}

static int vpm_log_set_level(uint8_t *pkt_data, uint16_t pkt_datalen, const char *iface_name)
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

static int vpm_log_get_level(char *rsp, uint16_t num, const char *iface_name)
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

void VpmMaCommandProc(int sock, char *req, int req_len)
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
            vpm_reload_config(pkt_data, pkt_datalen);
            goto finish;
        case MA_CMD_GET_COUNTER:
         sprintf (cmd_desc, "%s(%d)", "Get-Counter", MA_CMD_GET_COUNTER);
            goto finish;
        case MA_CMD_COUNTER_STRUCT:
            sprintf (cmd_desc, "%s(%d)", "Get-Counter-Struct", MA_CMD_COUNTER_STRUCT);
            goto finish;
        case MA_CMD_DEBUG_ON:
            sprintf (cmd_desc, "%s(%d)", "Debug-ON", MA_CMD_DEBUG_ON);
            vpm_log_append_link(sock, "[%i] %t - (%f:%l) <%d> (%n) -- ", RT_LOG_DEBUG);
            goto finish;
        case MA_CMD_DEBUG_OFF:
        sprintf (cmd_desc, "%s(%d)", "Debug-OFF", MA_CMD_DEBUG_OFF);
            vpm_log_delete_link(sock, LOG_OP_IFACE);
            goto finish;
        case MA_CMD_LOG_LEVEL:
         sprintf (cmd_desc, "%s(%d)", "Debug-LEVEL", MA_CMD_LOG_LEVEL);
            vpm_log_set_level(pkt_data, pkt_datalen, LOG_OP_IFACE);
            goto finish;
        case MA_CMD_DEBUG_MASK:
         sprintf (cmd_desc, "%s(%d)", "Debug-MASK", MA_CMD_DEBUG_MASK);
            goto finish;
        case MA_CMD_SHOW_LEVEL:
         sprintf (cmd_desc, "%s(%d)", "Debug-SHOWLEVEL", MA_CMD_SHOW_LEVEL);
            rsp_len = vpm_log_get_level(rsp, NTOHS(header->num), LOG_OP_IFACE);
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

void *vpm_ma_do_proc(void *args)
{
    int ret, done = 0;
    struct timeval timeout;
    fd_set f_set;
    char *request = NULL;
    vpm_ma_mgmt_st *vpm_mgmt = (vpm_ma_mgmt_st *)args;

    request = (char *)kmalloc(__ma_pkt_size__, MPF_CLR, -1);
    if (!request){
        return NULL;
    }

    FOREVER {
        do{
            vpm_mgmt->ma_sock = rt_clnt_sock(libIndex, __localhost_ipaddr__, vpm_mgmt->ma_port, AF_INET);
            sleep(3);
        }while(vpm_mgmt->ma_sock < 0);

        if (0 != vpm_mgmt->ma_register (vpm_mgmt->ma_sock, vpm_mgmt->pid, vpm_mgmt->sn)){
            continue;
        }

        done = 0;
        do{
            if (atomic_read(&vpm_mgmt->m_flags)){
                close(vpm_mgmt->ma_sock);
                vpm_mgmt->ma_sock = -1;
                atomic_set(&vpm_mgmt->m_flags, 0);
                break;
            }

            FD_ZERO(&f_set);
            FD_SET(vpm_mgmt->ma_sock, &f_set);
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            ret = select(vpm_mgmt->ma_sock + 1, &f_set, NULL, NULL, &timeout);
            if (ret < 0 && !ERRNO_EQUAL(EINTR)){
                rt_log_error(ERRNO_SOCK_SELECT, "select: %s", strerror(errno));
                break;
            }else{
                if (ret > 0 &&
                    FD_ISSET(vpm_mgmt->ma_sock, &f_set)){
                        int rsize = rt_sock_recv(vpm_mgmt->ma_sock, request, __ma_pkt_size__);
                        if (rsize <= 0){
                            rt_log_warning(ERRNO_SOCK_RECV, "rsize=%d, sock=%d, %s.", rsize, vpm_mgmt->ma_sock,
                            (rsize == 0) ? "Connection closed" : strerror(errno));
                            break;
                        }
                        vpm_mgmt->ma_cmd_func(vpm_mgmt->ma_sock, request, rsize);
                }
            }
        }while(!done);

        close(vpm_mgmt->ma_sock);
        vpm_mgmt->ma_sock = -1;
    }
    task_deregistry_id(pthread_self());

    return NULL;
}

void *vpm_sguard_do_proc(void *args)
{
    int ret, done = 0;
    fd_set f_set;
    struct timeval timeout;
    char request[__ma_pkt_size__] = {0};

    vpm_ma_mgmt_st *vpm_mgmt = (vpm_ma_mgmt_st *)args;

    int *p = &vpm_mgmt->sguard_sock;

#ifdef RT_TMR_ADVANCED
    vpm_mgmt->sguard_timer = tmr_create(BackEndSystem, "sguard keepalive", TMR_PERIODIC,
                                        vpm_mgmt->sguard_ka_func, 1, (char **)&p, vpm_mgmt->ka_interval);
#else
    vpm_mgmt->sguard_timer = rt_timer_create(BackEndSystem, TYPE_LOOP);
#endif

    FOREVER{
        do{
#ifdef RT_TMR_ADVANCED
            tmr_stop(vpm_mgmt->sguard_timer);
#else
            rt_timer_stop(vpm_mgmt->sguard_timer);
#endif
            vpm_mgmt->sguard_sock = rt_clnt_sock(libIndex, __localhost_ipaddr__, vpm_mgmt->sguard_port, AF_INET);
            sleep(3);
        }while(vpm_mgmt->sguard_sock < 0);

        if (0 != vpm_mgmt->sguard_register (vpm_mgmt->sguard_sock, vpm_mgmt->pid, vpm_mgmt->sn)){
            continue;
        }
#ifdef RT_TMR_ADVANCED
        tmr_start(vpm_mgmt->sguard_timer);
#else
        rt_timer_start(vpm_mgmt->sguard_timer, vpm_mgmt->ka_interval, vpm_mgmt->sguard_ka_func, 1, (char **)&p);
#endif
        done = 0;
        do{
            if (atomic_read(&vpm_mgmt->s_flags)){
                close(vpm_mgmt->sguard_sock);
                vpm_mgmt->sguard_sock = -1;
                atomic_set(&vpm_mgmt->s_flags, 0);
                break;
            }

            FD_ZERO(&f_set);
            FD_SET(vpm_mgmt->sguard_sock, &f_set);
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            ret = select(vpm_mgmt->sguard_sock + 1, &f_set, NULL, NULL, &timeout);
            if (ret < 0 && !ERRNO_EQUAL(EINTR)){
                rt_log_error(ERRNO_SOCK_SELECT, "select: %s", strerror(errno));
                break;
            }else{
                if (ret > 0 &&
                    FD_ISSET(vpm_mgmt->sguard_sock, &f_set)){
                    int rsize = rt_sock_recv(vpm_mgmt->sguard_sock, request, __ma_pkt_size__);
                    if (rsize <= 0){
                        rt_log_warning(ERRNO_SOCK_RECV, "rsize=%d, sock=%d, %s.", rsize, vpm_mgmt->sguard_sock,
                        (rsize == 0) ? "Connection closed" : strerror(errno));
                        break;
                    }
                }
            }
        }while(!done);

        close(vpm_mgmt->sguard_sock);
        vpm_mgmt->sguard_sock = -1;
    }
    task_deregistry_id(pthread_self());

    return NULL;
}

static vpm_ma_mgmt_st vpm_mgmt_st = {
    .sn = -1,
    .ma_sock = -1,
    .ma_port = -1,
    .ma_register = &VpmRegisterToMa,
    .ma_cmd_func = &VpmMaCommandProc,

    .sguard_sock = -1,
    .sguard_port = -1,
    .sguard_timer = -1,
    .ka_interval = 1,
    .sguard_register = &VpmRegisterToSguard,
    .sguard_ka_func = &VpmKaliveToSguard,
};

/* read sguard port && ma port */
int load_vpm_ma_config(int reload)
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
    if (ma_port != vpm_mgmt_st.ma_port && reload){
        atomic_set(&(vpm_mgmt_st.m_flags), 1);
    }
    vpm_mgmt_st.ma_port = ma_port;

    ConfYamlReadInt("ma.sguardport", &sguard_port);
    if ((sguard_port < 1024) || (sguard_port > 65535))
        sguard_port = __sguard_port_default__;
    if (sguard_port != vpm_mgmt_st.sguard_port && reload)
        atomic_set(&(vpm_mgmt_st.s_flags), 1);
    vpm_mgmt_st.sguard_port = sguard_port;

    rt_log_info("VPM Management Agent:");
    rt_log_info("  Port: %d", vpm_mgmt_st.ma_port);
    rt_log_info("  Sguard port %d", vpm_mgmt_st.sguard_port);

    xret = 0;
finish:
    return xret;
}

static struct rt_task_t vpm_manage_task =
{
    .module = THIS,
    .name = "vpm manage task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vpm_mgmt_st,
    .routine = vpm_ma_do_proc,
};

static struct rt_task_t vpm_sguard_task =
{
    .module = THIS,
    .name = "vpm sguard task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vpm_mgmt_st,
    .routine = vpm_sguard_do_proc,
};

void vpm_dms_init()
{
    int reload = 0;
    int sn = 0;
    vpm_ma_mgmt_st *ma_mgmt = &vpm_mgmt_st;
    struct vpm_t *vpm = vrs_default_trapper()->vpm;

    sn = vpm->serial_num;
    if ((sn != -1) && !load_vpm_ma_config(reload))
    {
       ma_mgmt->sn = sn;
       ma_mgmt->pid = getpid();

       task_registry(&vpm_manage_task);
       task_registry(&vpm_sguard_task);
    }
}

