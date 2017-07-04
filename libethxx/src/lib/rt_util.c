#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <linux/sockios.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <sys/types.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <dirent.h>
#include "rt_logging.h"
#include "rt_string.h"
#include "rt_sync.h"
#include "rt_common.h"

int isalldigit(const char *str)
{
    int i;
    if (!str) return 0;
    for (i = 0; i < (int) STRLEN(str); i++)
    {
        if (!isdigit(str[i]))
            return 0;
    }
    return 1;
}

int integer_parser(const char *instr, int min, int max)
{
    int xp = 0;
    int xret = -1;
    
    if (likely(instr)) {
	    xp = atoi(instr);
	    if (!isalldigit(instr) || (xp > max) || (xp < min)){
	        goto finish;
	    }
	    xret = xp;
    }
finish:
    return xret;
}

int integer64_parser(const char *instr, int64_t min, int64_t max)
{
    int xp = 0;
    int xret = -1;
    
    if (likely(instr)) {
	    /** change this caller */
	    xp = atoi(instr);
	    if (!isalldigit(instr) || (xp > max) || (xp < min)){
	        goto finish;
	    }
	    xret = xp;
    }
finish:
    return xret;
}

void do_system(const char *cmd)
{
    int xret = -1;

    if (likely(cmd)) {
	    xret = system(cmd);
	    if (likely(xret < 0)){
	        ;//rt_log_error(ERRNO_FATAL, "%s", strerror(errno));
	    }
    }
}


int rt_kv_pair(char *str, char **k, char **v)
{
    char seps[]   = " \t\r\n=:";
    *k = strtok(str, seps);
    if (*k != NULL)
    {
        *v = strtok(NULL, seps);
        if (*v != NULL)
        {
            return  0;
        }
    }
    return -1;
}
 
void rt_system_preview()
{
    struct passwd *pw;
    pw = getpwuid(getuid());
    char computer[256];
    struct utsname uts;
 
    if (gethostname(computer, 255) != 0 || uname(&uts) < 0){
        fprintf(stderr, "Unable to get the host information.\n");
        return ;
    }
    printf("\r\nSystem Preview\n");
    printf("%30s:%60s\n", "The Login User", getlogin());
    if (pw)
        printf("%30s:%60s\n", "The Runtime User", pw -> pw_name);

    printf("%30s:%60s\n", "The Host", uts.nodename);
    printf("%30s:%60s\n", "The OS", uts.sysname);
    printf("%30s:%60s\n", "The Arch", uts.machine);
    printf("%30s:%60s\n", "The Version", uts.version);
    printf("%30s:%60s\n", "The Release", uts.release);
    printf("\r\n\n");
 
}

#define BUF_SIZE 1024
void rt_get_pname_by_pid(pid_t pid, char *task_name) 
{
    char proc_pid_path[BUF_SIZE];
    char buf[BUF_SIZE];
    sprintf(proc_pid_path, "/proc/%d/status", pid);
    FILE* fp = fopen(proc_pid_path, "r");
    if(NULL != fp){
        if( fgets(buf, BUF_SIZE-1, fp)== NULL ){
            fclose(fp);
        }
        fclose(fp);
        sscanf(buf, "%*s %s", task_name);
    }
}

int nic_address(char *dev, char *ip4,  size_t __attribute__((__unused__))ip4s,
                        char *ip4m, size_t __attribute__((__unused__))ip4ms,
                        uint8_t *mac, size_t macs)
{
    struct ifreq ifreq;
    int sock;
    struct sockaddr_in   *sin;
    socklen_t sin_len = sizeof(struct sockaddr_in);
    struct sockaddr   sa;
    int xret = -1;
   
    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        rt_log_error(ERRNO_SOCK_ERR, 
                        "%s", strerror(errno));
        return xret;
    }

    memset (&ifreq, 0, sizeof(struct ifreq));
    strncpy(ifreq.ifr_name, dev, sizeof(ifreq.ifr_name));

    xret = ioctl (sock, SIOCGIFHWADDR, &ifreq);
    if (xret < 0){
        rt_log_error(ERRNO_SOCK_ERR, 
                        "%s", strerror(errno));
        goto finish;
    }

    memcpy(&sa, &ifreq.ifr_addr, sin_len);

    if(mac)
        memcpy(mac, sa.sa_data, macs);
    
    xret = ioctl (sock, SIOCGIFADDR, &ifreq);
    if (xret < 0){
        rt_log_notice ("%s", strerror(errno));
        //goto finish;
    }   

    sin = (struct sockaddr_in *)&ifreq.ifr_addr;

    if(ip4)
        strcpy(ip4, inet_ntoa(sin->sin_addr));

    xret = ioctl (sock, SIOCGIFNETMASK, &ifreq);
    if (xret < 0){
        rt_log_error(ERRNO_SOCK_ERR, 
                        "%s", strerror(errno));
        goto finish;
    }   

    sin = (struct sockaddr_in *)&ifreq.ifr_netmask;

    if(ip4m)
        strcpy(ip4m, inet_ntoa(sin->sin_addr));
    

finish:
    close(sock);
    return 0;
}

#define ETHTOOL_GLINK        0x0000000a /* Get link status (ethtool_value) */

enum linkstatus_t { IFSTATUS_UP, IFSTATUS_DOWN, IFSTATUS_ERR } linkstatus_t;

/* for passing single values */
struct ethtool_value
{
    uint32_t    cmd;
    uint32_t    data;
};
#if 1
static enum linkstatus_t interface_detect_beat_ethtool(int fd, char *iface)
{
    struct ifreq ifr;
    struct ethtool_value edata;
   
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name)-1);

    edata.cmd = ETHTOOL_GLINK;
    ifr.ifr_data = (caddr_t) &edata;

    if (ioctl(fd, SIOCETHTOOL, &ifr) == -1) {
        rt_log_info( 
                        "%s", strerror(errno));
        return IFSTATUS_ERR;
    }

    return edata.data ? IFSTATUS_UP : IFSTATUS_DOWN;
}
#endif

int nic_linkdetected(char *interface)
{
    enum linkstatus_t status;
    int fd;
    int up = 0;

    if(unlikely(!interface)){
        up = -1;
        goto finish;
    }
    
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        rt_log_error(ERRNO_SOCK_ERR, 
                        "%s", strerror(errno));
        goto finish;
    }

    status = interface_detect_beat_ethtool(fd, interface);
    switch (status) {
        case IFSTATUS_UP:
            //printf("%s : link up\n", interface);
            up = 1;
            break;
        
        case IFSTATUS_DOWN:
            //printf("%s : link down\n", interface);
            break;
        
        default:
            //printf("Detect Error\n");
            up = -1;
            break;
    }

    close(fd);

finish:
    return up;
}


