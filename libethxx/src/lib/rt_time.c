/*
*   rt_time.c
*   Created by Tsihang <qihang@semptian.com>
*   25 Mar, 2016
*   Func: Time Component
*   Personal.Q
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include "rt_common.h"
#include "rt_sync.h"

static int live = 1;
static struct timeval current_time = { 0, 0 };
static rt_spinlock current_time_spinlock;
static INIT_MUTEX(ct_mtx);
static rt_cond  ct_cond = PTHREAD_COND_INITIALIZER;

void rt_time(struct timeval *tv)
{
    if (tv == NULL)
        return;

    if (live == 1) {
        gettimeofday(tv, NULL);
    } else {
        rt_spinlock_lock(&current_time_spinlock);
        tv->tv_sec = current_time.tv_sec;
        tv->tv_usec = current_time.tv_usec;
        rt_spinlock_unlock(&current_time_spinlock);
    }
}

struct tm *rt_localtime(time_t timep, struct tm *result)
{
    return localtime_r(&timep, result);
}

#define isLeapYear(year)    (( (year % 100 == 0) && (year % 400 == 0)) || ((year % 100 != 0) && (year % 4 == 0)))
#define daysOfYear(year)    (isLeapYear(year) ? 366 : 365)

static __rt_always_inline__ int daysOfMonth(int year, int month)
{
    if (month == 2)
    {
        if (isLeapYear(year))
        {
            return 29;
        }
        else
        {
            return 28;
        }
    }
    else if ((month == 4) || (month == 6) || (month == 9) || (month == 11))
    {
        return 30;
    }
    else
    {
        return 31;
    }
}

/* calculate year\month\day by epoch days (epoch: 1970-01-01 00:00:00 UTC) */
static __rt_always_inline__ void getDatebyEpochDays(int days, int *year, int *month, int *day)
{
    *day = days;

    for (*year = 1970; ; (*year) ++)
    {
        if (*day > daysOfYear(*year))
        {
            *day -= daysOfYear(*year);
            continue;
        }
        else
        {
            for (*month = 1; *month <= 12; (*month) ++)
            {
                if (*day > daysOfMonth(*year, *month))
                {
                    *day -=  daysOfMonth(*year, *month);
                    continue;
                }

                break;
            }

            break;
        }
    }
}

static __rt_always_inline__ int get_localtimezone()
{
    time_t time_utc;
    struct tm tm_local;
    struct tm tm_gmt;
    int time_zone = 0;

    time(&time_utc);
    localtime_r(&time_utc, &tm_local);
    gmtime_r(&time_utc, &tm_gmt);

    time_zone = tm_local.tm_hour - tm_gmt.tm_hour;
    if (time_zone < -12)
    {
            time_zone += 24;
    }
    else if(time_zone > 12)
    {
            time_zone -=24;
    }

    return time_zone;
}

/* calculate year\month\day by epoch days (epoch: 1970-01-01 00:00:00 UTC) */
void tmscanf_ns(uint64_t timestamp_ns, int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    int days = 0;
    *sec = timestamp_ns / 1000000000 % 60;
    *min = timestamp_ns / 1000000000 / 60 % 60;
    *hour = timestamp_ns /1000000000 / 3600 % 24;
    days = timestamp_ns / 1000000000 / 3600 / 24 + 1;
    getDatebyEpochDays (days, year, month, day);
}

void tmscanf_s(uint64_t timestamp_s, int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    int days = 0;
    *sec = timestamp_s % 60;
    *min = timestamp_s / 60 % 60;
    *hour = timestamp_s / 3600 % 24 + get_localtimezone();
    days = timestamp_s / 3600 / 24 + 1;
    getDatebyEpochDays(days, year, month, day);

    return;
}

uint64_t rt_time_ms(void)
{
#ifdef __linux__
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);    /** CST timestamp */
#else
    return 0;
#endif
};

uint64_t rt_get_epoch_timestamp(void)
{
#ifdef __linux__
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000000 + tv.tv_usec * 1000);    /** CST timestamp */
#else
    return 0;
#endif
}

int
rt_file_mk_by_time(const char *realpath, const char *tm_form, char *name, 
    int len)
{
    char s[128] = {0};
    time_t ts = 0;
    struct tm *p = NULL;

    ts = time(NULL);
    p = localtime(&ts);
    
    strftime(s, 128 - 1, tm_form,  p);
    if (realpath)
        snprintf(name, len - 1, "%s/%s", realpath, s);
    else
        snprintf(name, len - 1, "%s", s);
    
    return 0;
}

/** Convert data to timestamp
    Date formate: %Y-%m-%d, eg:2015-8-20
*/
int rt_str2tms(char *date, const char *tm_form, uint64_t *ts)
{
    struct tm tm;

    assert(date);
    memset(&tm, 0, sizeof(struct tm));
    strptime(date, tm_form, &tm);
    *ts = (uint64_t)mktime(&tm);

    /**
    char buf[255];
    strptime("2001-11-12 18:31:2", "%Y-%m-%d %H:%M:%S", &tm);
    strftime(buf, sizeof(buf), "%d %b %Y %H:%M", &tm);
    puts(buf);
    */
    return 0;
}

int rt_tms2str(uint64_t ts, const char *tm_form, char *date, size_t len)
{
    struct tm *tm = NULL;

    assert(date);
    /** Convert ts to localtime with local time-zone */
    tm = localtime((time_t *)&ts);
    return (int)strftime(date, len - 1, tm_form, tm);
}

int rt_curr_tms2str(const char *tm_form, char *date, size_t len)
{
    return rt_tms2str(time(NULL), tm_form, date, len);
}

void rt_timedwait(int usec)
{
    int xret = -1;
    struct timeval val;
    struct timespec tspec;

    if (usec == 0)
        usec = 60000;

    rt_mutex_lock(&ct_mtx);
    gettimeofday(&val, NULL);
    tspec.tv_sec = val.tv_sec + usec /(1000 * 1000);
    tspec.tv_nsec = val.tv_usec * 1000;
    xret = rt_cond_timedwait(&ct_cond, &ct_mtx, &tspec);
    if (xret == ETIMEDOUT) {
        xret = 0;
    }
    rt_mutex_unlock(&ct_mtx);
}

