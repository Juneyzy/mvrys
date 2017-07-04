#include "sysdefs.h"
#include "vrs_pkt.h"

static uint16_t sguard_num = 0;
uint16_t ma_num = 0;

uint16_t fill_info(uint8_t *pkt, uint8_t *info, uint16_t len, uint16_t type)
{
    uint16_t n_len;
    uint16_t n_type;

    if (pkt == NULL || info == NULL)
    {
        return 0;
    }

    n_type = htons(type);
    memcpy(pkt, &n_type, 2);
    n_len = htons(len);
    memcpy(pkt + 2, &n_len, 2);
    memcpy(pkt + 4, info, len);
    return len + 4;
}

uint32_t fill_regsguard_packet(uint8_t *pkt, uint32_t pid, uint32_t sn,
							   uint32_t id,
                               char __attribute__((__unused__)) *pc_ip,
                               int __attribute__((__unused__)) i_sid)
{
    struct pkt_header *header;
    uint8_t *payload;
    uint16_t len;
    uint16_t payload_len = 0;
    uint32_t n_pid;
    uint32_t n_sn;
    uint32_t n_vrs_id;

    if (pkt == NULL)
    {
        return 0;
    }

    //memset(pkt, 0, SVM_BUFSIZE);
    header = (struct pkt_header *) pkt;
    header->reply = 0;
    header->ver = 1;
    header->cmd = htons(16);
    header->num = htons(sguard_num);
    sguard_num++;
    payload = pkt + sizeof(struct pkt_header) + 4;
    n_pid = htonl(pid);
    len = 4;
    payload_len += fill_info(payload + payload_len, (uint8_t *) &n_pid, len, 2);
    len = 4;
    n_sn = htonl(sn);
    payload_len += fill_info(payload + payload_len, (uint8_t *) &n_sn, len, 3);


    n_vrs_id = htonl(id);

    len = 4;
    payload_len += fill_info(payload + payload_len, (uint8_t *)&n_vrs_id, len, 4);

    payload = pkt + sizeof(struct pkt_header);
    * ((uint16_t *) payload) = htons(1);
    * ((uint16_t *)(payload + 2)) = htons(payload_len);
    header->len = htons(payload_len + 4 + sizeof(struct pkt_header));
    return payload_len + 4 + sizeof(struct pkt_header);
}

uint32_t fill_alive_packet(uint8_t *pkt, uint16_t status)
{
    struct pkt_header *header;
    uint8_t *payload;
    uint16_t len;
    uint16_t payload_len;

    if (pkt == NULL)
    {
        return 0;
    }

    //memset(pkt, 0, SVM_BUFSIZE);
    header = (struct pkt_header *) pkt;
    header->reply = 0;
    header->ver = 1;
    header->cmd = htons(5);
    header->num = htons(sguard_num);
    sguard_num++;
    payload = pkt + sizeof(struct pkt_header) + 4;
    len = sizeof(status);
    payload_len = fill_info(payload, (uint8_t *) &status, len, 1);
    payload = pkt + sizeof(struct pkt_header);
    * ((uint16_t *) payload) = htons(4);
    * ((uint16_t *)(payload + 2)) = htons(payload_len);
    header->len = htons(payload_len + 4 + sizeof(struct pkt_header));
    return payload_len + 4 + sizeof(struct pkt_header);
}

uint32_t fill_ma_log_packet(uint8_t *pkt, char *msg)
{
    struct pkt_header *header;
    uint8_t *payload;


    header = (struct pkt_header *)pkt;
    header->ver = 1;
    header->reply = 1;
    header->cmd = htons(30);
    header->num = htons(ma_num++);
    ma_num++;
    header->len = htons(sizeof(struct pkt_header));
    payload = pkt + sizeof(struct pkt_header);

    memcpy(payload, msg, strlen(msg));
    header->len =  htons(strlen(msg) + sizeof(struct pkt_header));

    return (strlen(msg) + sizeof(struct pkt_header));
}

uint32_t fill_regma_packet(uint8_t *pkt, uint32_t pid, uint32_t sn)
{
    struct pkt_header *header;
    uint8_t *payload;
    uint16_t len;
    uint16_t payload_len;
    uint32_t n_pid;
    uint32_t n_sn;

    if (pkt == NULL)
    {
        return 0;
    }

    //memset(pkt, 0, SVM_BUFSIZE);
    header = (struct pkt_header *) pkt;
    header->reply = 0;
    header->ver = 1;
    header->cmd = htons(16);
    header->num = htons(ma_num);
    ma_num++;
    payload = pkt + sizeof(struct pkt_header) + 4;
    payload_len = 0;
    n_pid = htonl(pid);
    len = 4;
    payload_len = fill_info(payload + payload_len, (uint8_t *) &n_pid, len, 1);
    len = 4;
    n_sn = htonl(sn);
    payload_len += fill_info(payload + payload_len, (uint8_t *) &n_sn, len, 2);
    payload = pkt + sizeof(struct pkt_header);
    * ((uint16_t *) payload) = htons(2);
    * ((uint16_t *)(payload + 2)) = htons(payload_len);
    header->len = htons(payload_len + 4 + sizeof(struct pkt_header));
    return payload_len + 4 + sizeof(struct pkt_header);
}

uint32_t fill_ma_counter(uint8_t *pkt, uint16_t sn, uint16_t type,
                         uint8_t *cntr, uint16_t len)
{
    struct pkt_header *header;
    uint8_t *payload;
    uint16_t payload_len;
    uint16_t pkt_len;
    time_t count_time;
    uint16_t version;

    if (pkt == NULL)
    {
        return 0;
    }

    header = (struct pkt_header *) pkt;
    header->reply = 1;
    header->ver = 1;
    header->cmd = htons(19);
    header->num = htons(sn);
    pkt_len = sizeof(struct pkt_header);
    payload = pkt + pkt_len;
    version = htons(1);
    payload_len = fill_info(payload, (uint8_t *) &version, 2, 2);
    pkt_len += payload_len;
    payload = payload + payload_len;
    count_time = time(NULL);
    count_time = htonl(count_time);
    payload_len = fill_info(payload, (uint8_t *) &count_time, sizeof(time_t), 1000);
    pkt_len += payload_len;
    payload = payload + payload_len;
    payload_len = fill_info(payload, cntr, len, type);
    pkt_len += payload_len;
    payload = payload + payload_len;
    header->len = htons(pkt_len);
    return pkt_len;
}

int fill_ma_count_info_struct(unsigned char *pkt, unsigned short pkt_head_num, char *soft_name, char *proc_name, unsigned int field_num, struct count_struct *field_array)
{
	struct pkt_header *pst_ph = NULL;
	unsigned char *pkt_start = pkt;

	if( pkt==NULL || soft_name==NULL || field_num==0 || field_array==NULL || proc_name==NULL )
	{
		return 0;
	}

	//packet head
	pst_ph = (struct pkt_header *)pkt;
	pst_ph->ver = 1;
	pst_ph->reply = 1;
	pst_ph->cmd = htons(39);
	pst_ph->num = htons(pkt_head_num);
	pst_ph->len = 0;
	pkt += sizeof(struct pkt_header);

	//software name
	*(unsigned short *)pkt = htons(1);
	pkt += 2;
	*(unsigned short *)pkt = htons(strlen(soft_name)+1);
	pkt += 2;
	memcpy (pkt, soft_name, strlen(soft_name)+1);
	pkt += strlen(soft_name)+1;

	//proccess name
	*(unsigned short *)pkt = htons(4);
	pkt += 2;
	*(unsigned short *)pkt = htons(strlen(proc_name)+1);
	pkt += 2;
	memcpy (pkt, proc_name, strlen(proc_name)+1);
	pkt += strlen(proc_name)+1;

	// filed total numbler
	*(unsigned short *)pkt = htons(2);
	pkt += 2;
	*(unsigned short *)pkt = htons(sizeof(unsigned int));
	pkt += 2;
	*(unsigned int *)pkt = htonl(field_num);
	pkt += 4;

	// filed name array
	*(unsigned short *)pkt = htons(3);
	pkt += 2;
	*(unsigned short *)pkt = htons(sizeof(struct count_struct)*field_num);
	pkt += 2;
	memcpy (pkt, (void *)field_array, sizeof(struct count_struct)*field_num);
	pkt += (sizeof(struct count_struct)*field_num);

	//fill packet head length
	pst_ph->len = htons((pkt - pkt_start));

	return (pkt - pkt_start);
}

uint32_t fill_loglevel_packet(void *pv_data, unsigned short us_cmd_num, int i_loglevel)
{
	if( pv_data==NULL )
	{
		syslog (LOG_INFO, "call %s function fail, argument error", __FUNCTION__);
		return -1;
	}

	struct pkt_header *pst_ph = (struct pkt_header *)pv_data;
	unsigned char *puc_pkt = NULL;

	//group packet
	pst_ph->ver = 1;
	pst_ph->reply = 1;
	pst_ph->cmd = htons (33);
	pst_ph->num = htons (us_cmd_num);
	pst_ph->len = htons (sizeof(struct pkt_header));
	puc_pkt = (unsigned char *)pv_data+sizeof(struct pkt_header);

	//group body
	*(unsigned int *)puc_pkt = i_loglevel;
	pst_ph->len = htons (sizeof(struct pkt_header)+sizeof(int));

	return (sizeof(struct pkt_header)+sizeof(int));
}

uint8_t get_ma_reload_conf(uint8_t *data, uint32_t data_len,
                           char **conf, uint32_t num)
{
    uint16_t type;
    uint16_t length;
    unsigned short len;
    char *p;
    uint8_t n;

    if (data == NULL || data_len == 0)
    {
        return 0;
    }

    n = 0;
    len = 0;
    p = (char *) data;

    while ((data_len - len) > 4)
    {
        type = * (uint16_t *) p;
        length = * (uint16_t *)(p + 2);
        type = ntohs(type);
        length = ntohs(length);

        if (type == 1)
        {
            *conf = p + 4;
            conf++;
            n++;
        }

        if (n == num)
        {
            break;
        }

        len += (length + 4);
        p += (length + 4);
    }

    return n;
}


static int ma_cmd_get_a_dev_soft_status(uint8_t *data, uint32_t data_len,
                                        DEV_SOFT_STATUS_ST *dss, uint16_t *slen)
{
    uint16_t type;
    uint16_t length, tlen;
    uint16_t len;
    uint8_t *p;
    type = ntohs((* (unsigned short *)(data)));
    tlen = ntohs((* (unsigned short *)(data + 2)));
    *slen = tlen + 4;

    if ((type == 2) && (tlen <= (data_len - 4)))
    {
        p = data + 4;
        len = 0;

        while (len < tlen)
        {
            type = ntohs((* (unsigned short *)(p)));
            length = ntohs((* (unsigned short *)(p + 2)));

            switch (type)
            {
                case 1:
                    dss->status = ntohl((* (unsigned int *)(p + 4)));
                    break;

                case 2:
                    strncpy(dss->dev, (char *) p + 4, length > 31 ? 31 : length);
                    break;

                case 3:
                    strncpy(dss->soft, (char *) p + 4, length > 31 ? 31 : length);
                    break;

                case 4:
                    dss->id = ntohl(* ((uint32_t *) (p + 4)));
                    break;
            }

            len += (length + 4);
            p += (length + 4);
        }
    }
    else
    {
        return 0;
    }

    return 1;
}


uint8_t ma_cmd_get_dev_soft_status(uint8_t *data, uint32_t data_len,
                                   char *sf_name, uint8_t nlen, DEV_SOFT_STATUS_ST *dss, uint8_t num)
{
    uint16_t type;
    uint16_t length;
    uint32_t len;
    unsigned short slen;
    uint8_t *p;
    uint8_t n;
    int ret;

    if (data == NULL || data_len == 0)
    {
        return 0;
    }

    type = * (uint16_t *) data;
    length = * (uint16_t *)(data + 2);
    type = ntohs(type);
    length = ntohs(length);
    n = 0;

    if (type == 1)
    {
        strncpy(sf_name, (char *) data + 4, nlen - 1);
        len = 4 + length;
        p = data + 4 + length;

        while ((len + 4) < data_len)
        {
            ret = ma_cmd_get_a_dev_soft_status(p, data_len - len, dss, &slen);

            if (ret)
            {
                n++;
                dss++;
            }

            if (n == num)
            {
                break;
            }

            len += slen;
            p += slen;
        }
    }

    return n;
}


int ma_cmd_get_data(uint8_t *pkt, uint32_t pkt_len,
                    uint16_t *cmd, uint8_t **data, uint32_t *data_len)
{
    struct pkt_header *header;
    uint16_t header_len;
    uint16_t len;
    header_len = sizeof(struct pkt_header);

    if ((pkt == NULL) || (header_len > pkt_len))
    {
        return -1;
    }

    header = (struct pkt_header *) pkt;
    *data = pkt + header_len;
    len = ntohs(header->len);

    if (len < header_len)
    {
        return -2;
    }

    *data_len = len - header_len;
    *cmd = ntohs(header->cmd);
    return len;
}

void ma_header_get_cmd_len(uint8_t *pkt, uint16_t *cmd, uint16_t *data_len)
{
    struct pkt_header *header;
    header = (struct pkt_header *) pkt;
    *cmd = ntohs(header->cmd);
    *data_len = ntohs(header->len) - sizeof(struct pkt_header);
    return ;
}


int ma_cmd_get_master_slave_status(unsigned char *puc_data, int i_data_len,
                                   char *pc_sname, int i_sname_len, unsigned int *pi_ms_status, unsigned int *pi_sid,
                                   char *pc_devname, int i_devname_len, char *pc_ip, int i_ip_len)
{
    if (puc_data == NULL || i_data_len <= 0 || pc_sname == NULL || i_sname_len <= 0 || pi_ms_status == NULL ||
            pc_devname == NULL || i_devname_len <= 0 || pc_ip == NULL || i_ip_len <= 0 || pi_sid == NULL)
    {
        return -1;
    }

    unsigned short us_type;
    unsigned short us_len;
    unsigned char *puc_ch = puc_data;
    int i_para_num = 0;

    while (puc_ch - puc_data < i_data_len)
    {
        us_type = ntohs(* (unsigned short *)(puc_ch));
        us_len = ntohs(* (unsigned short *)(puc_ch + 2));

        switch (us_type)
        {
            case 1:
                if (us_len < i_sname_len)
                {
                    memcpy(pc_sname, puc_ch + 4, us_len);
                    i_para_num++;
                }

                break;

            case 2:
                *pi_ms_status = ntohl(* (unsigned int *)(puc_ch + 4));
                i_para_num++;
                break;

            case 4:
                *pi_sid = htonl(* (unsigned int *)(puc_ch + 4));
                i_para_num++;
                break;

            case 3:
            {
                unsigned char *puc_tmp = puc_ch + 4;
                unsigned short us_tlen;
                unsigned short us_ttype;

                while (puc_tmp - (puc_ch + 4) < us_len)
                {
                    us_ttype = ntohs(* (unsigned short *)(puc_tmp));
                    us_tlen = ntohs(* (unsigned short *)(puc_tmp + 2));

                    switch (us_ttype)
                    {
                        case 1:
                            if (us_tlen < i_devname_len)
                            {
                                memcpy(pc_devname, (puc_tmp + 4), us_tlen);
                                i_para_num++;
                            }

                            break;

                        case 2:
                            if (us_tlen < i_ip_len)
                            {
                                memcpy(pc_ip, (puc_tmp + 4), us_tlen);
                                i_para_num++;
                            }

                            break;
                    }

                    puc_tmp = puc_tmp + us_tlen + 4;
                }
            }
            break;
        }

        puc_ch = puc_ch + 4 + us_len;
    }

    if (*pi_ms_status == 1)
    {
        if (i_para_num == 3)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    else if (*pi_ms_status == 0)
    {
        if (i_para_num == 5)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}


