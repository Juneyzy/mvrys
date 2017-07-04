/*
*   socket.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The socket interface encapsulation
*   Personal.Q
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/uio.h>
#include <stddef.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <assert.h>
#include <sys/un.h>

#include "rt_common.h"
#include "rt_logging.h"
#include "sockunion.h"
#include "rt_socket.h"

#ifndef ERRNO_EQUAL
#define ERRNO_EQUAL(e) (errno == e)
#endif

#define sock_retry() (\
    (ERRNO_EQUAL(EAGAIN) /** file locked*/) || \
    (ERRNO_EQUAL(EWOULDBLOCK)) ||\
    (ERRNO_EQUAL(EINTR /** interrupt by signal */)))

int rt_clnt_sock(int __attribute__((__unused__))libIndex,
    char *serv_ip,
    unsigned short serv_port,
    int family)
{
    int s = -1, sock = -1;
    union sockunion su;

    memset(&su, 0, sizeof(union sockunion));
    su.sa.sa_family = family;
    if ((s = sockunion_stream_socket(&su)) < 0)
        goto finish;

    sockopt_reuseaddr(s);
    sockopt_reuseport(s);
    sockopt_timeout(s);

    if (su.sa.sa_family == AF_INET){
        su.sin.sin_family = su.sa.sa_family;
        su.sin.sin_port = htons(serv_port);
        inet_aton(serv_ip, (struct in_addr *)&su.sin.sin_addr.s_addr);
    }

    if (connect_success != sockunion_connect(s, &su, serv_port, 0)){
        //rt_log_error(ERRNO_SOCK_ERR, "Can't connect to (%s:%d, fd=%d) : %s\n",
        //            serv_ip, serv_port, s, strerror(errno));
        close(s);
        goto finish;
    }
    sock = s;
finish:
    return sock;
}


int rt_serv_sock(int  __attribute__((__unused__))libIndex,
    uint16_t port,
    int family)
{
    int s = -1;
    int ret, accept_sock = -1;
    union sockunion su;
    memset(&su, 0, sizeof(union sockunion));
    su.sa.sa_family = family;
    /* Prepare TCP socket for receiving connections */
    /* Make new socket. */

    s = sockunion_stream_socket(&su);
    if (s < 0)
        goto finish;

    /* This is server, so reuse address. */
    sockopt_reuseaddr(s);
    sockopt_reuseport(s);
    sockopt_nodelay(s);

    /* Bind socket to universal address and given port. */
    ret = sockunion_bind(s, &su, port, NULL);
    if (ret < 0){
        close(s);     /* Avoid sd leak. */
        goto finish;
    }

    /* Listen socket under queue 3. */
    ret = sockunion_listen(s, 0);
    if (ret < 0){
        close(s);     /* Avoid sd leak. */
        goto finish;
    }
    accept_sock = s;
finish:
    return accept_sock;
}

int rt_serv_accept(int __attribute__((__unused__))libIndex,
    int accept_sock)
{
    union sockunion su;
    int sock;
    memset(&su, 0, sizeof(union sockunion));
    /* We can handle IPv4 or IPv6 socket. */
    sock = sockunion_accept(accept_sock, &su);
    if (sock < 0) {
        /*zlog_warn (libIndex, "can't accept vty socket : %s", strerror (errno));*/
        perror("accept");
        return -1;
    }
    sockopt_nodelay(sock);
    return sock;
}

void rt_sock_shdown(int sock,
    fd_set *set)
{
    if (set)
        FD_CLR(sock, set);
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

void rt_sock_close(int *sock, fd_set * set)
{
	if (likely (*sock > 0)) {
		if (set)
			FD_CLR(*sock, set);
		shutdown(*sock, SHUT_RDWR);
		close(*sock);
		*sock = -1;
	}
}

int rt_sock_getpeername(int sock,
    void *saddr)
{
    int xret = -1;
    struct sockaddr_in *sin = (struct sockaddr_in *)saddr;
    socklen_t sl;

    assert (sin);

    sl = sizeof(struct sockaddr_in);
    if (getpeername(sock, (struct sockaddr *)sin, &sl)){
        rt_log_error(ERRNO_SOCK_GET_PEER, "sock=%d, %s\n", sock, strerror(errno));
        goto finish;
    }
    xret = 0;

finish:
    return xret;
}

int rt_sock_chk_status (int sock)
{
	int optval;
	socklen_t optlen;
	optlen = sizeof(int);
	int ret = getsockopt (sock, SOL_SOCKET, SO_ERROR, (void*)&optval, &optlen);
	return (!ret && !optval);
}

/** Check socket ready after delay us, 0-timedout, 1:sock_ready, -1:sock_select error or fd is not set in &SET */
int is_sock_ready(int  sock,
    int usec, int *timedout)
{
    int ret = -1;
    fd_set set;
    struct timeval tv;

    assert (timedout);

    if(usec == 0)
        usec = 60000;

    tv.tv_sec = usec/(1000 * 1000);
    tv.tv_usec = usec%1000*1000;

    FD_ZERO(&set);
    FD_SET(sock, &set);

    ret = select(sock+1, &set, NULL, NULL, &tv);

    while(ret < 0)
    {
        if (ERRNO_EQUAL(EINTR))
            continue;
        rt_log_error(ERRNO_SOCK_SELECT, "%s\n", strerror(errno));
        goto finish;
    }

    if (ret == 0){
        *timedout = 1;
        goto finish;
     }

    if (ret > 0){
        /** here is a bug ##tsihang. */
        ret = FD_ISSET(sock, &set);
    }

finish:
    return ret;
}

/** Check socket ready after delay us, 0-timedout, 1:sock_ready, -1:sock_select error or fd is not set in &SET */
int is_sock_ready_rd_wr(int  sock,
    int usec, int *timedout, int *flg)
{
    int ret = -1;
    fd_set set;
    fd_set wrset;
    struct timeval tv;

    if (sock < 0)
        goto finish;

    assert (timedout);

    if(usec == 0)
        usec = 60000;

    tv.tv_sec = usec/(1000 * 1000);
    tv.tv_usec = usec%1000*1000;

    FD_ZERO(&set);
    FD_SET(sock, &set);
    FD_ZERO(&wrset);
    FD_SET(sock, &wrset);

    ret = select(sock+1, &set, &wrset, NULL, &tv);

    while(ret < 0)
    {
        if (ERRNO_EQUAL(EINTR))
            continue;
        rt_log_error(ERRNO_SOCK_SELECT, "%s\n", strerror(errno));
        goto finish;
    }

    if (ret == 0){
        *timedout = 1;
        goto finish;
     }

    if (ret > 0){
        /** here is a bug ##tsihang. */
        ret = FD_ISSET(sock, &set);
        if (1 == ret)
            *flg |= 1<<SELECT_READ;
        ret = FD_ISSET(sock, &wrset);
        if (1 == ret)
            *flg |= 1<<SELECT_WRITE;
    }

finish:
    return ret;
}

ssize_t rt_sock_recv(int sock,
    void *buf, size_t size)
{
    ssize_t n = -1;

    while ((int)(n = recv(sock, buf, size, 0)) == -1)
    {
        if (sock_retry())
           continue;

        break;
    }

    return n;
}

enum rt_sock_errno
rt_sock_recv_timedout(IN const char __attribute__((__unused__)) *prompt,
    IN int sock,
    IN void *buf,
    IN size_t size,
    IN int32_t usec,
    IN int (*proc_fn)(int, char *, ssize_t, int, void **, void *),
    IN void (*timedout_fn)(),
    IN int argc,
    IN void **argv,
    OUT void *outputs_params)
{
    ssize_t xret = 0;
    int timedout = 0;
    enum rt_sock_errno rn = SOCK_CONTINUE;

    xret = is_sock_ready(sock, usec, &timedout);
    if(xret > 0) {

        xret = rt_sock_recv(sock, buf, size);

        if (xret <= 0){
            rn = SOCK_CLOSE;
            goto finish; /** peer closed or error */
        }

        if(proc_fn)
            proc_fn(sock, buf, xret, argc, argv, outputs_params);

    }
    else{
        if (xret == 0){
            if(timedout &&
                timedout_fn){
                    timedout_fn();
                    goto finish;
            }
            /** fd is not set FD_ISSET */
            goto finish;
        }
        else
        {
            /** select error */
            rn = SOCK_CLOSE;
        }
    }
finish:
    return rn;
}


THE_INTERFACE_HAS_NOT_BEEN_TESTED
ssize_t rt_sock_recv_blk_timedout(int sock,
    void *buf, size_t size, int32_t usec)
{
    ssize_t ret;
    ssize_t io = 0;
    int timedout = 0;
    while(io < (ssize_t)size)
    {
        if(is_sock_ready(sock, usec, &timedout) > 0)
        {
            ret = read(sock, buf + io, size - io);
            if(ret > 0)
            {
                io += ret;
                continue;
            }
        }
        break;
    }

    return io;
}

ssize_t rt_sock_send(int sock,
    void *buf, size_t size)
{
    ssize_t n = -1;

    while ((int)(n = send(sock, buf, size, 0)) == -1)
    {
        if (sock_retry())
            continue;

        break;
    }
    return n;
}

THE_INTERFACE_HAS_NOT_BEEN_TESTED
ssize_t rt_sock_send_timedout(int sock,
    void *buf, size_t size, int32_t usec)
{
    ssize_t xret = 0;
    int timedout = 0;

    xret = is_sock_ready(sock, usec, &timedout);
    if(xret > 0) {
        xret = write(sock, buf, size);
    }

    return xret;
}

THE_INTERFACE_HAS_NOT_BEEN_TESTED
ssize_t rt_sock_send_blk_timedout(int sock,
    void *buf, size_t size, int32_t usec)
{
    ssize_t ret;
    ssize_t io = 0;
    int timedout = 0;

    while(io < (ssize_t)size)
    {
        if(is_sock_ready(sock, usec, &timedout) > 0)
        {
            ret = write(sock, buf + io, size - io);
            if(ret > 0)
            {
                io += ret;
                continue;
            }
        }
        break;
    }

    return io;
}


int	rt_lsock_server (char *sock_file)
{
	int s, len;
	struct sockaddr_un local;

	if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
		rt_log_error (ERRNO_FATAL,
			"%s", strerror(errno));
		return -1;
	}

	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, sock_file);
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);

	if (bind(s, (struct sockaddr *)&local, len) == -1) {
		rt_log_error (ERRNO_FATAL,
			"%s", strerror(errno));
		rt_sock_close (&s, NULL);
		return -1;
	}

	if (listen(s, 5) == -1) {
		rt_log_error (ERRNO_FATAL,
			"%s", strerror(errno));
		rt_sock_close (&s, NULL);
	}

	return s;
}

int	rt_lsock_client (char *sock_file)
{
	int s, len, xerror;
	struct sockaddr_un remote;

	if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
		rt_log_error (ERRNO_FATAL,
			"%s", strerror(errno));
		return -1;
	}

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, sock_file);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);

	xerror = connect(s, (struct sockaddr *)&remote, len);
	if (xerror < 0) {
		rt_log_error (ERRNO_FATAL, "%s", strerror(errno));
		rt_sock_close (&s, NULL);
	}

	return s;
}
