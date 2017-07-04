#ifndef __RT_TM_H__
#define __RT_TM_H__

uint64_t rt_get_epoch_timestamp(void);
struct tm *rt_localtime(time_t timep, struct tm *result);
int rt_str2tms(char *date, const char *tm_form, uint64_t *ts);
int rt_tms2str(uint64_t ts, const char *tm_form, char *date, size_t len);
int rt_curr_tms2str(const char *tm_form, char *date, size_t len);

int
rt_file_mk_by_time(const char *realpath, const char *tm_form, char *name, 
    int len);

void tmscanf_ns(uint64_t timestamp_ns, int *year, int *month, int *day, int *hour, int *min, int *sec);
void tmscanf_s(uint64_t timestamp_s, int *year, int *month, int *day, int *hour, int *min, int *sec);
void rt_timedwait(int usec);
uint64_t rt_time_ms(void);

#endif