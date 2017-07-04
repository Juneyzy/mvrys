#ifndef __DEBUG_FILTERS_H__
#define __DEBUG_FILTERS_H__

#include <pthread.h>

/**
 * \brief Enum that holds the different kinds of filters available
 */
enum {
    RT_LOG_FILTER_BL = 0,
    RT_LOG_FILTER_WL = 1,
    RT_LOG_FILTER_MAX = 2,
};

/**
 * \brief Structure used to hold the line_no details of a FG filter
 */
typedef struct RTLogFGFilterLine_ {
    int line;

    struct RTLogFGFilterLine_ *next;
} RTLogFGFilterLine;

/**
 * \brief structure used to hold the function details of a FG filter
 */
typedef struct RTLogFGFilterFunc_ {
    char *func;
    RTLogFGFilterLine *line;

    struct RTLogFGFilterFunc_ *next;
} RTLogFGFilterFunc;

/**
 * \brief Structure used to hold FG filters.  Encapsulates filename details,
 *        func details, which inturn encapsulates the line_no details
 */
typedef struct RTLogFGFilterFile_ {
    char *file;
    RTLogFGFilterFunc *func;

    struct RTLogFGFilterFile_ *next;
} RTLogFGFilterFile;

/**
 * \brief Structure used to hold the thread_list used by FD filters
 */
typedef struct RTLogFDFilterThreadList_ {
    int entered;
    pthread_t t;
//    pid_t t;

    struct RTLogFDFilterThreadList_ *next;
} RTLogFDFilterThreadList;

/**
 * \brief Structure that holds the FD filters
 */
typedef struct RTLogFDFilter_ {
    char *func;

    struct RTLogFDFilter_ *next;
} RTLogFDFilter;


extern int rt_logging_fg_filters_present;

extern int rt_logging_fd_filters_present;


int RTLogAddFGFilterWL(const char *, const char *, int);

int RTLogAddFGFilterBL(const char *, const char *, int);

int RTLogMatchFGFilterBL(const char *, const char *, int);

int RTLogMatchFGFilterWL(const char *, const char *, int);

void RTLogReleaseFGFilters(void);

int RTLogAddFDFilter(const char *);

int RTLogPrintFDFilters(void);

void RTLogReleaseFDFilters(void);

int RTLogRemoveFDFilter(const char *);

int RTLogCheckFDFilterEntry(const char *);

void RTLogCheckFDFilterExit(const char *);

int RTLogMatchFDFilter(const char *);

int RTLogPrintFGFilters(void);

void RTLogAddToFGFFileList(RTLogFGFilterFile *,
                                         const char *,
                                         const char *, int,
                                         int);

void RTLogAddToFGFFuncList(RTLogFGFilterFile *,
                                         RTLogFGFilterFunc *,
                                         const char *, int);

void RTLogAddToFGFLineList(RTLogFGFilterFunc *,
                                         RTLogFGFilterLine *,
                                         int);

void RTLogReleaseFDFilter(RTLogFDFilter *);
#endif /* __DEBUG_H__ */

