
#include "sysdefs.h"
#include "conf.h"
#include "web_srv.h"
#include "web_json.h"
#include "bes-command.h"
#include "capture.h"

#define default_errno() ERRNO_INVALID_ARGU

struct webui_mgmt_t {
    int fd;
    char *ip;
    int port;
    void (*ui_cluster_fn)();
    void *local_dc; /** Local Decoder Cluster */
};

static struct webui_mgmt_t *XUIM = NULL;

static inline int
ui_config_from_yaml(struct webui_mgmt_t *m)
{
    int xret = -1;
    assert (m);
    ConfNode *interface = ConfGetNode("interface");
    ConfNode *iobj = NULL;

    TAILQ_FOREACH(iobj, &interface->head, next){
        if(!STRCMP(iobj->name, "ui")){
            ConfNode *obj = NULL;
            char *listen_port = NULL;
            char *ipaddress = NULL;
            TAILQ_FOREACH(obj, &iobj->head, next){
                if(ConfGetChildValue(obj, "ipaddress", &ipaddress)){
                    m->ip = strdup(ipaddress);
                    if(!m->ip){
                        rt_log_error(ERRNO_UI_YAML_ARGU, "Invalid ipaddress\n");
                        goto finish;
                    }
                    xret = 0;
                }
                if(ConfGetChildValue(obj, "port", &listen_port)){
                    m->port = tcp_udp_port_parse(listen_port);
                    if(m->port < 0){
                        rt_log_error(ERRNO_UI_YAML_ARGU, "Invalid port\n");
                        goto finish;
                    }
                    xret = 0;
                }
            }
        }
    }
finish:
    return xret;
}

static inline int 
ui_config()
{
    int xret = -1;
    
    XUIM = __TMalloc (1, struct webui_mgmt_t);
    if (!XUIM){
        rt_log_error (ERRNO_MEM_ALLOC, "Can not alloc memory for ui\n");
        goto finish;
    }
    memset (XUIM, 0, sizeof (struct webui_mgmt_t));

    xret = ui_config_from_yaml(XUIM);

    printf("\r\nInterface for UI Preview\n");
    printf("%30s:%60s\n", "The Ipaddress", XUIM->ip);
    printf("%30s:%60d\n", "The Port", XUIM->port);
    
    
finish:
    return xret;
}
    
ssize_t json_send(int sock, const char *data, size_t size, 
    int(*rebuild)(const char *iv, size_t is, char **ov, size_t *os))
{
    char *odata = NULL;
    size_t osize = 0;
    int xret = -1;
   
    if (rebuild){
        xret = rebuild(data, size, &odata, &osize);
        if (xret < 0)
            goto finish;

        xret = rt_sock_send(sock, odata, osize);
        printf("--%d-- %p %d=?%d\n", xret, odata, (int)size, (int)osize);
        free(odata);
    }

finish:
    return xret;
}

static int
ui_do_request(int fd,
    const char *qstr,
    ssize_t __attribute__((__unused__))qsize)
{
    const char *jstr = NULL;
    json_hdr jhdr;
    json_object *head, *body, *injson;

    memset(&jhdr, 0, sizeof(jhdr));
    jhdr.error = XSUCCESS;

    body = json_object_new_object();
    head = json_object_new_object();

    web_json_parser(qstr, &jhdr, &injson);
    switch(jhdr.cmd){
        case UI_CMD_START:
        case UI_CMD_RESTART:
        case UI_CMD_STOP:
            json_captor_ctrl(&jhdr, body);
            goto response;
        case UI_CMD_STATISTICS_RT_GET:
            json_rt_throughput_query(&jhdr, body);
            goto response;
        case UI_CMD_STATISTICS_PROTOCOL_GET:
            json_protocol_throughput_query(&jhdr, body);
            goto response;
        case UI_CMD_STATISTICS_MESSAGE_GET:
            json_message_throughput_query(&jhdr, body);
            goto response;
        case UI_CMD_STATISTICS_SUMMARY_GET:
            json_summary_throughput_query(&jhdr, body);
            goto response;			
        case UI_CMD_STATISTICS_EXCEPTION_GET:
		json_exception_throughput_query(&jhdr, body);
            goto response;
        case UI_CMD_IP5TUPLE_GET:
            goto response;
        case UI_CMD_RULE:
            json_clue_proc(qstr, qsize, &jhdr, body, injson);
            goto response;
        default:
            goto cmd_error;
    }

cmd_error:
    jhdr.error = (-default_errno());
response:
    json_object_put(injson);
    web_json_head_add(head, &jhdr, body);
    jstr = json_object_to_json_string(head);
    json_send(fd, jstr, strlen(jstr), web_json_data_rebuild);
    json_object_put(body);
    json_object_put(head);

    return 0;
}

static void *ui_serv_task(void *args)
{
#define BUF_SIZE    2048

    struct sockaddr_in sock_addr;
    socklen_t sock_addr_len = sizeof(sock_addr);
    int sock = *(int *)args;
    fd_set f_set;
    struct timeval timeout;
    int ret;
    char *buffer = NULL;
    ssize_t buf_size = 0;

    getpeername(sock, (struct sockaddr *) &sock_addr, &sock_addr_len);

    buffer = __TMalloc(BUF_SIZE, char);
    assert(buffer);
    
    memset (buffer, 0, BUF_SIZE);
    buf_size = 0;

    FOREVER
    {
        FD_ZERO(&f_set);
        FD_SET(sock, &f_set);

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        ret = select(sock + 1, &f_set, NULL, NULL, &timeout);
        if (ret <= 0) {
            if (ret == -1 && ERRNO_EQUAL(EINTR))
                continue;

            if (ret == 0){
                //sprintf (buffer, "%s", "timeout");
                continue;
            }
            
            goto finish;
        }

        if (FD_ISSET(sock, &f_set)) {
            buf_size = rt_sock_recv(sock, buffer, BUF_SIZE - 1);
            if (buf_size <= 0) {
                if (buf_size < 0){
                    sprintf (buffer, "%s", "error");
                    goto finish;
                }

                if (buf_size == 0)
                goto finish;
            }
#if 1
            ui_do_request(sock, buffer, buf_size);
            memset (buffer, 0, BUF_SIZE);
            buf_size = 0;
#else
    /** loopback */
            if (buf_size)
                rt_sock_send(sock, buffer, buf_size);
#endif
        }
    }

finish:
    rt_log_info("peer=%s:%d socket=%d, %s(%s), shutdown it.\n",
        inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock, buffer, strerror(errno));
    rt_sock_shdown(sock, &f_set);
    sock = -1;
    free(buffer);
    task_deregistry_id(pthread_self());
    return NULL;
}


static void *ui_acpt_task(void *args)
{
    struct sockaddr_in sock_addr;
    socklen_t sock_addr_len = sizeof(sock_addr);
    int accept_sock = *(int *)args;
    int sock;
    fd_set f_set;
    int ret;
    struct timeval timeout;
    char desc[128] = {0};

    accept_sock = rt_serv_sock(0, XUIM->port, AF_INET);
    FOREVER
    {
        FD_ZERO(&f_set);
        FD_SET(accept_sock, &f_set);

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;

        ret = select(accept_sock + 1, &f_set, NULL, NULL, &timeout);
        if (ret <= 0) {
            if ((ret == -1 && ERRNO_EQUAL(EINTR)) ||
                ret == 0)
                continue;

            sprintf (desc, "%s", "select");
            goto finish;
        }
        else {
            if (FD_ISSET(accept_sock, &f_set)) {
                sock = rt_serv_accept(0, accept_sock);
                if (sock <= 0) {
                    sprintf (desc, "%s", "error");
                    goto finish;
                 }

                memset (desc, 0, 128);
                BUG_ON(getpeername(sock, (struct sockaddr *) &sock_addr, &sock_addr_len));
                sprintf (desc, "%s:%d socket:%d",
                    inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock);
                task_spawn(desc, 0, NULL, ui_serv_task, &sock);
                task_detail_foreach();
                continue;
            }
        }
    }

finish:
    rt_log_info("accept_sock=%d, %s(%s), shutdown it.\n",
        accept_sock, desc, strerror(errno));
    rt_sock_shdown(accept_sock, &f_set);
    accept_sock = -1;
    return NULL;
}

static struct rt_task_t user_interface_task =
{
    .module = THIS,
    .name = "User Interface Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = ui_acpt_task,
};

void ui_init_local()
{
    if (ui_config())
        return;

    /** Initialization Rules chains*/

    task_registry(&user_interface_task);
    
    //enum dc_type type = decoder_type();
    rt_ethernet_init();
    web_serv_init();
}

