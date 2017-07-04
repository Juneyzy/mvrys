/*************************************************************************
    > File Name: client.c
    > Author: xhp
    > Mail: xiehuipeng@semptian.com 
    > Created Time: 2015-12-09
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <error.h>

#define BUFSIZE 2048

static int local_sockserv()
{
    int local_server;
    int ret = -1, optval = 1;
    struct sockaddr_un local_addr;
    char *sock_path = "/var/run/oracl";
    //char *sock_path = "/home/xhp/tmp/oracle.sock";
    memset(&local_addr, 0, sizeof(struct sockaddr_un));

    local_addr.sun_family = AF_UNIX;
    strncpy(local_addr.sun_path, sock_path, strlen(sock_path)+1);
    //strcpy(local_addr.sun_path, sock_path);
    ret = access(sock_path, 0);
    if ( ret == 0)
    {
        remove(sock_path);
    }

    local_server = socket(AF_UNIX, SOCK_STREAM, 0);
    setsockopt(local_server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    ret = bind(local_server, (struct sockaddr *)&local_addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        perror("bind failed");
        return -1;
    }
    listen(local_server, 20);
    return local_server;
}

static int connect_to_oraser()//int *server_fd)
{
    int server_fd = -1;
    struct sockaddr_in ser_addr;
    short port = 44556;//44444;
    char *ser_ip = "192.168.40.210";

    socklen_t ser_addr_len = sizeof(struct sockaddr_in);
    memset(&ser_addr, 0, ser_addr_len);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    ser_addr.sin_addr.s_addr = inet_addr(ser_ip);

    if (connect(server_fd, (struct sockaddr *)&ser_addr, ser_addr_len) == 0)
    {
        return server_fd;
    }else{
        return -1;
    }

}

void *sync_oracle_client(void *arg)
{
    int server_fd, tmp_fd;
    int local_server;
    int ret;
    int length, send_len, recv_len;
    char *cmd = (char *)malloc(BUFSIZE);
    char *buf = (char *)malloc(BUFSIZE);
    struct sockaddr_un cli_addr;
    fd_set read_fds, write_fds;

    arg = arg;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    server_fd = connect_to_oraser();
    if (server_fd < 0)
    {
        printf("connect to oracle server failed\r\n");
        return NULL;
    }

    local_server = local_sockserv();
    if (local_server < 0)
    {
        printf("create local socket server failed\r\n");
        return NULL;
    }

    FD_SET(local_server, &read_fds);
    //FD_SET(local_server, &write_fds);

    for (;;)
    {

        ret = select(local_server+1, &read_fds, NULL, NULL, 0);
        if (ret <= 0)
        {
            perror("select error occur\r\n");
            break;
        }

        if (FD_ISSET(local_server, &read_fds))
        {
            tmp_fd = accept(local_server, (struct sockaddr *)&cli_addr, (socklen_t *)&length);

            length = 0;
            while (0 != (recv_len = recv(tmp_fd, buf, BUFSIZE, 0)))
            {
                if (length+recv_len > BUFSIZE)
                {
                    printf("cmd is too long than buffer\r\n");
                    close(tmp_fd);
                    continue;
                }
                memcpy(&cmd[length], buf, recv_len);
                length += recv_len;
            }
            close(tmp_fd);
            if (length <= 0)
            {
                perror("local perror closedr");
                continue;
            }

            send_len = 0;
            while(send_len < length)
            {
                send_len += send(server_fd, cmd, length-send_len, 0);
            }

            recv_len = recv(server_fd, buf, BUFSIZE, 0);
            if (recv_len <= 0)
            {
                perror("peer close connect or have error:");
                break;
            }

            buf[recv_len] = '\0';
            length = atoi(buf);
            if (length != send_len)
            {
                printf("send %d, buf server recv %d\r\n", send_len, length);
            }
        }

    }

    close(server_fd);
    close(local_server);

    return NULL;
}

/*
int main(int argc, char *argv[])
{
    sync_oracle_client(NULL);
}
*/

