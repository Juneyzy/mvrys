#ifndef __RT_UTIL_H__
#define __RT_UTIL_H__

int isalldigit(const char *str);

int integer_parser(const char *instr, int min, int max);
int integer64_parser(const char *instr, int64_t min, int64_t max);
void do_system(const char *cmd);
void rt_system_preview();
void rt_get_pname_by_pid(unsigned long pid, char *task_name) ;
int nic_address(char *dev, char *ip4,  size_t __attribute__((__unused__))ip4s,
                        char *ip4m, size_t __attribute__((__unused__))ip4ms,
                        uint8_t *mac, size_t macs);
extern int nic_linkdetected(char *interface);

#define tcp_udp_port_parse(pstr) integer_parser(pstr, 0, 65536)

#endif
