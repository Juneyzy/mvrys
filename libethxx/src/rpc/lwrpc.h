/*
*   lwrpc.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Light Weight Remote Procedure Call
*   Personal.Q
*/

#ifndef __LWRPC_H__
#define __LWRPC_H__

#include "lwrpc-config.h"

#ifdef RPC_DEBUG
#ifndef RPC_DEBUG_ENA
//#define RPC_DEBUG_ENA
#endif
#define rpc_debug(fmt, ...) printf(""fmt"", ##__VA_ARGS__);
#endif

#define RPC_REQUEST                 1
#define RPC_RESPONSE                2
#define RPC_READ_BUF                2048 /*1024*/
#define RPC_DATA_BUF                800 /*1024*/
#define UndefinedRpcStatus          (-1)

enum variable_type
{
    TypeServInvalid,
    TypeServI8,
    TypeServI16,
    TypeServI32,
    TypeServI64,
    TypeServU8,
    TypeServU16,
    TypeServU32,
    TypeServU64,
    TypeServDouble,
    TypeServString
} ;

typedef struct RPC_HEAD
{
    unsigned int version: 8;
    unsigned int pkt_type: 8;
    unsigned int pkt_len: 16;

    unsigned int resv;

    unsigned int rpc_id: 16;
    unsigned int param_num: 16;
} RPC_HEAD_ST;

typedef RPC_HEAD_ST RPC_HEAD_SERV_ST;
typedef RPC_HEAD_ST RPC_HEAD_CLNT_ST;

typedef struct RPC_PARAM
{
    unsigned int type: 16;
    unsigned int length: 16;
    unsigned int value[0];
} RPC_PARAM_ST;

static inline void rpc_generate_request_head(unsigned char *request, int pkt_len, int rpc_id, int param_num)
{
    RPC_HEAD_ST *pHead = (RPC_HEAD_ST *) request;
    pHead->version = RPC_VERSION;
    pHead->pkt_type = RPC_REQUEST;
    pHead->pkt_len = HTONS (pkt_len);
    pHead->resv = 0;
    pHead->rpc_id = HTONS (rpc_id);
    pHead->param_num = HTONS (param_num);
}
static inline int rpc_generate_response_head(unsigned char *result, int rpc_id, int return_param_num, int return_bytes)
{
    RPC_HEAD_SERV_ST *temp = (RPC_HEAD_SERV_ST *) result;
    temp->version = RPC_VERSION;
    temp->pkt_type = RPC_RESPONSE;
    temp->pkt_len = HTONS(return_bytes);
    temp->resv = 0 ;
    temp->rpc_id = HTONS(rpc_id);
    temp->param_num = HTONS(return_param_num);
    return return_bytes;
}

static inline int rpc_generate_msgbody(char *packet, char *data, int len)
{
    memcpy(packet, data, len);
    return len;
}

static inline void rpc_generate_param(unsigned char *request, int *offset, int *pkt_len, int type, int len, unsigned char *value)
{
    RPC_PARAM_ST *pParam = (RPC_PARAM_ST *)(request + *offset);
    pParam->type = HTONS (type);
    pParam->length = HTONS(len);
    uint64_t val64 = 0;
    uint32_t val32 = 0;
    uint16_t val16 = 0;

    switch (type)
    {
        case TypeServI64:
        case TypeServU64:
            val64 = HTONLL(*(uint64_t*)value);
            memcpy(pParam->value, &val64, len - 4);
            break;
        case TypeServI32:
        case TypeServU32:
            val32 = HTONL(*(uint32_t*)value);
            memcpy(pParam->value, &val32, len - 4);
            break;
        case TypeServI16:
        case TypeServU16:
            val16 = HTONS(*(uint16_t*)value);
            memcpy(pParam->value, &val16, len - 4);
            break;    
        default :
            memcpy(pParam->value, value, len - 4);
            break;
    }
    
    *offset += len;
    *pkt_len += len;
}
static inline void rpc_generate_response(unsigned char *result, int len)
{
    unsigned char response[1024];
    RPC_HEAD_SERV_ST *pHead = (RPC_HEAD_SERV_ST *) response;
    pHead->version = RPC_VERSION;
    pHead->pkt_type = RPC_RESPONSE;
    pHead->pkt_len = HTONS(len + sizeof(RPC_HEAD_SERV_ST));
    memcpy(response + sizeof(RPC_HEAD_SERV_ST), result, len);
}

static inline int rpc_packet_chk(unsigned char *packet)
{
#if RPC_PKT_CHECK
    RPC_HEAD_ST *pHead = (RPC_HEAD_ST *) packet;
    if (pHead->version != RPC_VERSION ||
            ((pHead->pkt_type != RPC_REQUEST) && (pHead->pkt_type != RPC_RESPONSE)))
    {
        /** Wrong packet */
        return -1;
    }
#else
    packet = packet;
#endif
    return 0;
}

static inline int rpc_get_pkt_len(unsigned char *packet)
{
    RPC_HEAD_ST *pHead = (RPC_HEAD_ST *) packet;

    if (rpc_packet_chk(packet) < 0)
    {
        printf("\r\n packet version error or type error\n");
    }

    return pHead->pkt_len;
}
static inline int rpc_get_rpc_id(unsigned char *packet)
{
    RPC_HEAD_SERV_ST *pHead = (RPC_HEAD_SERV_ST *) packet;
    return NTOHS(pHead->rpc_id);
}

static inline int rpc_get_param_num(unsigned char *packet)
{
    RPC_HEAD_SERV_ST *pHead = (RPC_HEAD_SERV_ST *) packet;
    return NTOHS(pHead->param_num);
}

static inline void rpc_parse_param(unsigned char *response, int *offset, unsigned char *result)
{
    RPC_PARAM_ST *pParam = (RPC_PARAM_ST *)(response + *offset);
    /**
        printf("\r\n type = %u length = %u value = %u \r\n",
            NTOHS (pParam->type), NTOHS (pParam->length), *((unsigned int *)pParam->value));
    */    

    uint64_t val64 = 0;
    uint32_t val32 = 0;
    uint16_t val16 = 0;
    uint8_t *uc_value = (uint8_t *)&pParam->value[0];

    switch (NTOHS(pParam->type))
    {
        case TypeServI64:
        case TypeServU64:
            val64 = NTOHLL(*(uint64_t *)uc_value);
            memcpy(result, &val64, NTOHS (pParam->length) - 4);
            break;
        case TypeServI32:
        case TypeServU32:
            val32 = NTOHL(*(uint32_t *)uc_value);
            memcpy(result, &val32, NTOHS (pParam->length) - 4);
            break;
        case TypeServI16:
        case TypeServU16:
            val16 = NTOHS(*((uint16_t *)uc_value));
            memcpy(result, &val16, NTOHS (pParam->length) - 4);
            break;    
        default :
            memcpy(result, pParam->value, NTOHS (pParam->length) - 4);
            break;
    }
    
    *offset += NTOHS (pParam->length);
}
static inline int rpc_write_request(int sockfd, unsigned char *data , int len)
{
    int wl = 0, resend = 0;
    int slice;

    //printf("write request to sock=%d, nbytes=%d\n", sockfd, len);
    if (1)
    {
        if (sockfd < 0)
        {
            printf("<rpcWrite> sockfd error\r\n");
            return -1;
        }

        if (len < 800)
        {
            wl = write(sockfd, data, len);
            resend = 0;

            while ((wl != len) && (resend < 5))
            {
                printf("resend : w1=%d,len =%d\r\n", wl, len);
                taskDelay(2);

                if (wl < 0)
                { wl = 0; }

                wl += write(sockfd, &data[wl], len - wl);
                resend++;
            }

            if (wl != len)
            {
                close(sockfd);
                printf("send error : w1=%d,len =%d\r\n", wl, len);
            }

            /*printf("write %d bytes\r\n",wl);*/
        }
        else
        {
            for (slice = 0; slice < (int)(len / 800); slice++)
            {
                wl = write(sockfd, &data[slice * 800], 800);
                resend = 0;

                while ((wl != 800) && (resend < 5))
                {
                    printf("resend : wl=%d,len = 800\r\n", wl);
                    taskDelay(2);

                    if (wl < 0)
                    { wl = 0; }

                    wl += write(sockfd, &data[wl], 800 - wl);
                    resend++;
                }

                if (wl != 800)
                {
                    close(sockfd);
                    printf("send error : wl=%d,len =800\r\n", wl);
                }

                /*printf("write %d bytes\r\n",wl);*/
            }

            if ((len - (slice * 800)) > 0)
            {
                wl = write(sockfd, &data[slice * 800], len - (slice * 800));
                resend = 0;

                while ((wl != (len - (slice * 800))) && (resend < 5))
                {
                    printf("resend : wl=%d,len = %d\r\n", wl, len - (slice * 800));
                    taskDelay(2);

                    if (wl < 0)
                    { wl = 0; }

                    wl += write(sockfd, &data[wl], len - (slice * 800) - wl);
                    resend++;
                }

                if (wl != (len - (slice * 800)))
                {
                    close(sockfd);
                    printf("send error : wl=%d,len = %d\r\n", wl, (len - (slice * 800)));
                }

                /*printf("write %d bytes\r\n",wl);*/
            }
        }
    }
    else
    {
        if (sockfd < 0)
        {
            printf("<rpcWrite> sockfd error\r\n");
            return -1;
        }

        if (len > 0)
        {
            wl = write(sockfd, data, len);
            resend = 0;

            while ((wl != len) && (resend < 5))
            {
                printf("resend : w1=%d,len =%d\r\n", wl, len);
                taskDelay(2);

                if (wl < 0)
                { wl = 0; }

                wl += write(sockfd, &data[wl], len - wl);
                resend++;
            }

            if (wl != len)
            {
                close(sockfd);
                printf("send error : w1=%d,len =%d\r\n", wl, len);
            }

            /*printf("write %d bytes\r\n",wl);*/
        }
    }

    return 0;
}

static inline int rpc_read_response(int fd, unsigned char *response)
{
    int nbytes, pkt_len = 0, offset = 0;
    unsigned char buf[RPC_READ_BUF];
    int first_flag = 1;

    while (1)
    {
        /* Read raw data from socket */
        nbytes = read(fd, buf, RPC_READ_BUF - 1);
        buf[nbytes] = 0;

        if (nbytes <= 0)
        {
            printf("\r\n nbytes = %d \r\n", nbytes);
            return -1;
        }

        /* length field of packet is in the first packet */
        if (first_flag == 1)
        {
            pkt_len = rpc_get_pkt_len(buf);

            if (pkt_len > RPC_READ_BUF - 1)
            {
                printf("\r\n Requester: packet length is too long \r\n");
                return -1;
            }

            first_flag = 0;
        }

        memcpy(response + offset, buf, nbytes);

        if (nbytes + offset != pkt_len)
        {
            offset += nbytes;
            continue;
        }
        else
        { break; }
    }

    return pkt_len;
}


static inline int rpc_read_request(int rpcSock, unsigned char *request)
{
    int nbytes, pkt_len = 0, offset = 0;
    unsigned char buf[RPC_READ_BUF];
    int first_flag = 1;

    while (1)
    {
        /* Read raw data from socket */
        nbytes = read(rpcSock, buf, RPC_READ_BUF - 1);
        buf[nbytes] = 0;

        if (nbytes <= 0)
        {
            //printf("\r\n nbytes = %d \r\n",nbytes);
            return -1;
        }

        /* length field of packet is in the first packet */
        if (first_flag == 1)
        {
            pkt_len = rpc_get_pkt_len(buf);

            if (pkt_len > RPC_READ_BUF - 1)
            {
                printf("\r\n Responser: packet length is too long \r\n");
                return -1;
            }

            first_flag = 0;
        }

        memcpy(request + offset, buf, nbytes);

        if (nbytes + offset != pkt_len)
        {
            offset += nbytes;
            continue;
        }
        else
        {
            break;
        }
    }

    return pkt_len;
}

static inline int rpc_write_response(int rpcSock, unsigned char *response, int responseLen)
{
    int wl, slice, resend = 0;

    if (responseLen < 800)
    {
        wl = write(rpcSock, response, responseLen);
        resend = 0;

        while ((wl != responseLen) && (resend < 5))
        {
            printf("resend : w1=%d,len =%d\r\n", wl, responseLen);
            taskDelay(2);

            if (wl < 0)
            { wl = 0; }

            wl += write(rpcSock, &response[wl], responseLen - wl);
            resend++;
        }

        if (wl != responseLen)
        {
            printf("send error : w1=%d,responseLen =%d\r\n", wl, responseLen);
        }

        /*printf("write %d bytes\r\n",wl);*/
    }
    else
    {
        for (slice = 0; slice < (int)(responseLen / 800); slice++)
        {
            wl = write(rpcSock, &response[slice * 800], 800);
            resend = 0;

            while ((wl != 800) && (resend < 5))
            {
                printf("resend : wl=%d,len = 800\r\n", wl);
                taskDelay(2);

                if (wl < 0)
                { wl = 0; }

                wl += write(rpcSock, &response[wl], 800 - wl);
                resend++;
            }

            if (wl != 800)
            {
                printf("send error : wl=%d,len =800\r\n", wl);
            }

            /*printf("write %d bytes\r\n",wl);*/
        }

        if ((responseLen - (slice * 800)) > 0)
        {
            wl = write(rpcSock, &response[slice * 800], responseLen - (slice * 800));
            resend = 0;

            while ((wl != (responseLen - (slice * 800))) && (resend < 5))
            {
                printf("resend : wl=%d,len = %d\r\n", wl, responseLen - (slice * 800));
                taskDelay(2);

                if (wl < 0)
                { wl = 0; }

                wl += write(rpcSock, &response[wl], responseLen - (slice * 800) - wl);
                resend++;
            }

            if (wl != (responseLen - (slice * 800)))
            {
                printf("send error : wl=%d,len = %d\r\n", wl, (responseLen - (slice * 800)));
            }

            /*printf("write %d bytes\r\n",wl);*/
        }
    }

    return wl;
}

/** Send timeout or errno */
#define RPC_RESPONSE_SIZE_CHK(rsp_len) if((rsp_len) <= 0) continue;//return -1


#define SYMBOL_CHAR     sizeof(char)
#define SYMBOL_SHORT    sizeof(short)
#define SYMBOL_LONG     sizeof(long)

#define FCHAR           "%c "
#define FINTEGER        "%d "
#define FHEXA           "0x%02x "
#define FUNSIGNED       "%u "

static inline void rpc_log_details(
    const void *buff,       /* Pointer to the array to be dumped */
    unsigned long addr,     /* Heading address value */
    int len,                /* Number of items to be dumped */
    int width,              /* Size of the items (DW_CHAR, DW_SHORT, DW_LONG) */
    char *formate           /* Formate of string */
)
{
    int i;
    const unsigned char *bp;
    const unsigned short *sp;
    const unsigned long *lp;
    char *form;
    form = formate;

    if (form == NULL)
    { form = " %02X"; }

    printf("%08lX ", addr);         /* address */

    switch (width)
    {
        case SYMBOL_CHAR:
            bp = buff;

            for (i = 0; i < len; i++)       /* Hexdecimal dump */
            { printf(form, bp[i]); }

            putchar(' ');

            for (i = 0; i < len; i++)       /* ASCII dump */
            { putchar((bp[i] >= ' ' && bp[i] <= '~') ? bp[i] : '.'); }

            break;

        case SYMBOL_SHORT:
            sp = buff;

            do                              /* Hexdecimal dump */
            { printf(form, *sp++); }
            while (--len);

            break;

        case SYMBOL_LONG:
            lp = buff;

            do                              /* Hexdecimal dump */
            { printf(form, *lp++); }
            while (--len);

            break;
    }

    putchar('\n');
}

static inline void RPCClntRequestDebug(unsigned char *request, int rpc_id, int request_len)
{
#if defined(RPC_DEBUG_ENA)
    int ofs = 0;
    char *request_hex = NULL;
    printf("RPC client send request[%d], request ID[%d]: \n", request_len, rpc_id);

    for (request_hex = (char *) request, ofs = 0; ofs < request_len; request_hex += 16, ofs += 16)
    { rpc_log_details((char *) request_hex, ofs, 16, 1, FHEXA); }

    printf("\n\n");
#else
    request = request;
    rpc_id = rpc_id;
    request_len = request_len;
#endif
}
static inline void RPCServRequestDebug(unsigned char *request, int rpc_id, int request_len)
{
#if defined(RPC_DEBUG_ENA)
    int ofs = 0;
    char *request_hex = NULL;
    printf("RPC server recv request[%d], request ID[%d]: \n", request_len, rpc_id);
    
    for (request_hex = (char *) request, ofs = 0; ofs < request_len; request_hex += 16, ofs += 16)
    { rpc_log_details((char *) request_hex, ofs, 16, 1, FHEXA); }

    printf("\n\n");
#else
    request = request;
    rpc_id = rpc_id;
    request_len = request_len;
#endif
}
static inline void RPCServResponseDebug(unsigned char *response, int rpc_id, int response_len)
{
#if defined(RPC_DEBUG_ENA)
    int ofs = 0;
    char *response_hex = NULL;
    printf("RPC server send response[%d], response ID[%d]: \n", response_len, rpc_id);

    for (response_hex = (char *) response, ofs = 0; ofs < response_len; response_hex += 16, ofs += 16)
    { rpc_log_details((char *) response_hex, ofs, 16, 1, FHEXA); }

    printf("\n\n");
#else
    response = response;
    rpc_id = rpc_id;
    response_len = response_len;
#endif
}
static inline void RPCClntResponseDebug(unsigned char *response, int rpc_id, int response_len)
{
#if defined(RPC_DEBUG_ENA)
    int ofs = 0;
    char *response_hex = NULL;
    printf("RPC client recv response[%d], response ID[%d]: \n", response_len, rpc_id);

    for (response_hex = (char *) response, ofs = 0; ofs < response_len; response_hex += 16, ofs += 16)
    { rpc_log_details((char *) response_hex, ofs, 16, 1, FHEXA); }

    printf("\n\n");
#else
    response = response;
    rpc_id = rpc_id;
    response_len = response_len;
#endif
}
#define RPC_GENERATE_PARAM(type, len, param)\
    do{\
        rpc_generate_param(request, &offset, (int *)&pkt_len, type, len, (unsigned char*)&param);\
        param_num++;\
    }while(0)

#define RPC_GENERATE_PARAM_POINTER(type, len, param)\
    do{\
        rpc_generate_param(request, &offset, (int *)&pkt_len, type, len, (unsigned char*)param);\
        param_num++;\
    }while(0)

#endif
