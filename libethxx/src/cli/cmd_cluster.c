#include "sysdefs.h"
#include "command.h"
#include "cluster_probe.h"
#include "cluster_decoder.h"

#define IPV4_PREFIX_MAX 19  /* A.B.C.D/M */

/* probe or decoder connect status */
enum status{
    UNDEFINE,
    CONNECT,
    DISCONNECT,
    MAX_STATUS
};

static char *status[MAX_STATUS] = {
    "undefine",
    "connect",
    "disconnect"
};

int set_probe_ip_comment(struct cmd_element *self, struct vty *vty, int argc, const char *argv[])
{
    int ret = 0;
    const char *ipaddr;
    const char *port;
	const char *mask;
	char ip_prefix[IPV4_PREFIX_MAX];

    ipaddr = argv[0];
    if (argc == 2)
    {
        port = argv[1];
        /* default mask is 24 */
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/24", ipaddr);
    }else if (argc == 3){
        mask = argv[1];
        port = argv[2];
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/%s", ipaddr, mask);
    }else{
        return CMD_ERR_NO_MATCH;
    }

    ret = probe_cluster_add(ip_prefix, port);
    if (ret < 0)
    {
        return CMD_ERR_NOTHING_TODO;
    }

    return CMD_SUCCESS;
}

int set_decoder_ip_comment(struct cmd_element *self, struct vty *vty, int argc, const char *argv[])
{
    int ret = 0;
    const char *ipaddr;
    const char *port;
	const char *mask;
	char ip_prefix[IPV4_PREFIX_MAX];

    ipaddr = argv[0];
    if (argc == 2)
    {
        port = argv[1];
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/24", ipaddr);
    }else if (argc == 3){
        mask = argv[1];
        port = argv[2];
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/%s", ipaddr, mask);
    }else{
        return CMD_ERR_NO_MATCH;
    }

    ret = decoder_cluster_add(ip_prefix, port);

    if (ret < 0)
    {
        return CMD_ERR_NOTHING_TODO;
    }
    return CMD_SUCCESS;

}

DEFUN(set_ip_connect_probe, set_ip_connect_probe_cmd,
      "set probe ip-address A.B.C.D  port <0-65535> ",
      "set configure\n"
      "设置配置\n"
      "probe cluster\n"
      "probe集群\n"
      "ip-address\n"
      "IP地址\n"
      "IP address (eg.192.168.40.2)\n"
      "网络地址 (eg.192.168.40.2)\n"
      "port to connect\n"
      "连接的端口号\n"
      "port range\n"
      "端口号范围\n")
{
    return set_probe_ip_comment(self, vty, argc, argv);
}


DEFUN(set_ip_m_connect_probe, set_ip_m_connect_probe_cmd,
      "set probe ip-address A.B.C.D <0-32> port <0-65535> ",
      "set configure\n"
      "设置配置\n"
      "probe cluster\n"
      "probe集群\n"
      "ip-address\n"
      "IP地址\n"
      "IP address (eg.192.168.40.2)\n"
      "网络地址 (eg.192.168.40.2)\n"
      "mask,  default 24\n"
      "掩码，默认24\n"
      "port to connect\n"
      "连接的端口号\n"
      "port range\n"
      "端口号范围\n")
{
    return set_probe_ip_comment(self, vty, argc, argv);
}

DEFUN(no_set_ip_connect_probe, no_set_ip_connect_probe_cmd,
      "no set probe ip-address A.B.C.D port <0-65535>",
      NO_STR
      NO_CSTR
      "set configure\n"
      "设置配置\n"
      "probe cluster\n"
      "probe集群\n"
      "ip-address\n"
      "IP地址\n"
      "IP address (eg.192.168.40.2)\n"
      "网络地址 (eg.192.168.40.2)\n"
      "port to connect\n"
      "连接的端口号\n"
      "port range\n"
      "端口号范围\n")
{
    const char *ipaddr;
    const char *port;
    const char *mask;
	char ip_prefix[IPV4_PREFIX_MAX];
    int ret = 0;

    ipaddr = argv[0];
    if (argc == 2)
    {
        port = argv[1];
        /* default mask is 24 */
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/24", ipaddr);
    }else if (argc == 3){
        mask = argv[1];
        port = argv[2];
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/%s", ipaddr, mask);
    }else{
        return CMD_ERR_NO_MATCH;
    }
    ret = probe_cluset_del(ip_prefix, port);
    if (ret < 0)
    {
        return CMD_ERR_NOTHING_TODO;
    }

    return CMD_SUCCESS;
}

DEFUN(show_probe_connect_stat, show_probe_connect_stat_cmd,
      "show probe connect status",
      SHOW_STR
      SHOW_CSTR
      "probe\n"
      "probe\n"
      "connect status\n"
      "连接状态\n"
      "status\n"
      "状态")
{
    struct pc_hostlist *hl;
    struct pc_host *host;
    enum status index = UNDEFINE;
    char *ipaddr = NULL;
    unsigned short port;

    hl = local_probe_cluster();

    vty_out(vty, "IP\t\tport\t\tstatus\r\n");

    probelist_lock(hl);
    probe_cluster_foreach_host(hl, host){

        if (host->sock < 0)
        {
            index = DISCONNECT;
        }else{
            index = CONNECT;
        }

        port = host->port;

        ipaddr = inet_ntoa(host->su.sin.sin_addr);

        vty_out(vty, "%s\t%-5hu\t\t%s\r\n", ipaddr, port, status[index]);
    }
    probelist_unlock(hl);

    return CMD_SUCCESS;
}


DEFUN(set_ip_m_connect_dcluster, set_ip_m_connect_dcluster_cmd,
      "set decoder ip-address A.B.C.D <0-32> port <0-65535>",
      "set configure\n"
      "设置配置\n"
      "decoder cluster\n"
      "decofer集群\n"
      "ip-address\n"
      "IP地址\n"
      "IP address (eg.192.168.40.2)\n"
      "网络地址 (eg.192.168.40.2)\n"
      "mask, default 24\n"
      "掩码，默认为24\n"
      "port to connect\n"
      "连接的端口号\n"
      "port range\n"
      "端口号范围\n")
{
    return set_decoder_ip_comment(self, vty, argc, argv);
}

DEFUN(set_ip_connect_dcluster, set_ip_connect_dcluster_cmd,
      "set decoder ip-address A.B.C.D port <0-65535>",
      "set configure\n"
      "设置配置\n"
      "decoder cluster\n"
      "decoder集群\n"
      "ip-address\n"
      "IP地址\n"
      "IP address (eg.192.168.40.2)\n"
      "网络地址 (eg.192.168.40.2)\n"
      "port to connect\n"
      "连接的端口号\n"
      "port range\n"
      "端口号范围\n")
{
    return set_decoder_ip_comment(self, vty, argc, argv);
}

DEFUN(no_set_ip_connect_dcluster, no_set_ip_connect_dcluster_cmd,
      "no set decoder ip-address A.B.C.D port <0-65535>",
      NO_STR
      NO_CSTR
      "set configure\n"
      "设置配置\n"
      "decoder cluster\n"
      "decoder集群\n"
      "ip-address\n"
      "IP地址\n"
      "IP address (eg.192.168.40.2)\n"
      "网络地址 (eg.192.168.40.2)\n"
      "port to connect\n"
      "连接的端口号\n"
      "port range\n"
      "端口号范围\n")
{
    const char *ipaddr;
    const char *port;
	const char *mask;
	char ip_prefix[IPV4_PREFIX_MAX];
    int ret = 0;

    ipaddr = argv[0];
    if (argc == 2)
    {
        port = argv[1];
        /* default mask is 24 */
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/24", ipaddr);
    }else if (argc == 3){
        mask = argv[1];
        port = argv[2];
        SNPRINTF(ip_prefix, IPV4_PREFIX_MAX, "%s/%s", ipaddr, mask);
    }else{
        return CMD_ERR_NO_MATCH;
    }

    ret = decoder_cluster_del(ipaddr, port);

    if (ret < 0)
    {
        return CMD_ERR_NOTHING_TODO;
    }

    return CMD_SUCCESS;
}

DEFUN(show_dc_cluster_connect_status, show_dc_cluster_connect_status_cmd,
      "show decoder connect status",
      SHOW_STR
      SHOW_CSTR
      "decoder\n"
      "decoder\n"
      "connect status\n"
      "连接状态\n"
      "status\n"
      "状态")
{
    return CMD_SUCCESS;
}

void cluster_cmdline_initialize(void)
{
	install_element (CONFIG_NODE, &set_ip_connect_probe_cmd);
	install_element (CONFIG_NODE, &set_ip_m_connect_probe_cmd);
	install_element (CONFIG_NODE, &no_set_ip_connect_probe_cmd);
	install_element (CONFIG_NODE, &show_probe_connect_stat_cmd);

    if (DT_MASTER == decoder_get_local()->type)
    {
        install_element(CONFIG_NODE, &set_ip_connect_dcluster_cmd);
        install_element(CONFIG_NODE, &set_ip_m_connect_dcluster_cmd);
        install_element(CONFIG_NODE, &no_set_ip_connect_dcluster_cmd);
        install_element(CONFIG_NODE, &show_dc_cluster_connect_status_cmd);
    }
}


