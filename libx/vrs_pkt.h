#ifndef __VRS_DMS_PKT_H__
#define __VRS_DMS_PKT_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

extern uint16_t ma_num;

struct pkt_header
{
    uint8_t ver;
    uint8_t reply;
    uint16_t cmd;
    uint16_t num;
    uint16_t len;
};

typedef struct DEV_SOFT_STATUS_ST_
{
    uint32_t id;
    uint32_t status;
    char dev[32];
    char soft[32];
} DEV_SOFT_STATUS_ST;

#define __ma_pkt_size__ 1024

uint16_t fill_info(uint8_t *pkt, uint8_t *info, uint16_t len, uint16_t type);

uint32_t fill_regsguard_packet(uint8_t *pkt, uint32_t pid, uint32_t sn, uint32_t id, char *pc_ip, int i_sid);

uint32_t fill_alive_packet(uint8_t *pkt, uint16_t status);

uint32_t fill_regma_packet(uint8_t *pkt, uint32_t pid, uint32_t sn);

uint32_t fill_ma_counter(uint8_t *pkt, uint16_t sn, uint16_t type,
                         uint8_t *cntr, uint16_t len);

uint32_t fill_ma_log_packet(uint8_t *pkt, char *msg);

uint32_t fill_loglevel_packet(void *pv_data, unsigned short us_cmd_num, int i_loglevel);

struct count_struct
{
	char *name;
	unsigned int len;
};

int fill_ma_count_info_struct(unsigned char *pkt, unsigned short pkt_head_num, char *soft_name, char *proc_name,
                              unsigned int field_num, struct count_struct *field_array);

uint8_t get_ma_reload_conf(uint8_t *data, uint32_t data_len,
                           char **conf, uint32_t num);
uint8_t ma_cmd_get_dev_soft_status(uint8_t *data, uint32_t data_len,
                                   char *sf_name, uint8_t nlen, DEV_SOFT_STATUS_ST *dss, uint8_t num);
int ma_cmd_get_data(uint8_t *pkt, uint32_t pkt_len,
                    uint16_t *cmd, uint8_t **data, uint32_t *data_len);
void ma_header_get_cmd_len(uint8_t *pkt, uint16_t *cmd, uint16_t *data_len);

int ma_cmd_get_master_slave_status(unsigned char *puc_data, int i_data_len,
                                   char *pc_sname, int i_sname_len, unsigned int *pi_ms_status, unsigned int *pi_sid,
                                   char *pc_devname, int i_devname_len, char *pc_ip, int i_ip_len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif /* __VRS_DMS_PKT_H__ */

