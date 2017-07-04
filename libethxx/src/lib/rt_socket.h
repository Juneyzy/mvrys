/*
*   socket.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The socket interface encapsulation
*   Personal.Q
*/


#ifndef __RT_SOCKET_H__
#define __RT_SOCKET_H__

enum rt_sock_errno{
    SOCK_CONTINUE,
    SOCK_CLOSE
};

#define SELECT_READ         1
#define SELECT_WRITE        2

extern int rt_sock_getpeername(int sock,
    void *saddr);
extern int rt_sock_chk_status (int sock);

extern int rt_clnt_sock(int , char *, unsigned short , int );
extern int rt_serv_accept(int , int);
extern int rt_serv_sock(int , unsigned short , int );
extern void rt_sock_shdown(int , fd_set *);
extern void rt_sock_close(int *sock, fd_set * set);
extern ssize_t rt_sock_send(int , void *, size_t);
extern ssize_t rt_sock_send_timedout(int , void *, size_t , int32_t );
extern ssize_t rt_sock_send_blk_timedout(int , void *, size_t , int32_t );
extern ssize_t rt_sock_recv(int , void *, size_t);
enum rt_sock_errno
rt_sock_recv_timedout(const char *prompt __attribute__((__unused__)),
    int sock,
    void *buf,
    size_t size,
    int32_t usec,
    int (*proc_fn)(int, char *, ssize_t, int, void **, void *),
    void (*timedout_fn)(),
    int argc,
    void **argv,
    void *outputs_params);

extern ssize_t rt_sock_recv_blk_timedout(int , void *, size_t , int32_t );

extern int is_sock_ready(int , int, int *);

extern int rt_lsock_server (char *sock_file);
extern int	 rt_lsock_client (char *sock_file);

#endif
