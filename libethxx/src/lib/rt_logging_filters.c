#include "rt_common.h"
#include "rt_stdlib.h"
#include "rt_string.h"
#include "rt_sync.h"
#include "rt_logging.h"
#include "rt_logging_filters.h"
#include "rt_enum.h"
#include "rt_stdlib.h"

/* both of these are defined in util-debug.c */
extern int rt_logging_module_initialized;
extern int rt_logging_module_cleaned;

/* used to indicate if any FG filters are registered */
int rt_logging_fg_filters_present = 0;

/* used to indicate if any FD filters are registered */
int rt_logging_fd_filters_present = 0;

/**
 * \brief Holds the fine-grained filters
 */
RTLogFGFilterFile *rt_logging_fg_filters[RT_LOG_FILTER_MAX] = { NULL, NULL };

/**
 * \brief Mutex for accessing the fine-grained fiters rt_logging_fg_filters
 */
static rt_mutex rt_logging_fg_filters_m[RT_LOG_FILTER_MAX] = { PTHREAD_MUTEX_INITIALIZER,
                                                                  PTHREAD_MUTEX_INITIALIZER };

/**
 * \brief Holds the function-dependent filters
 */
static RTLogFDFilter *rt_logging_fd_filters = NULL;

/**
 * \brief Mutex for accessing the function-dependent filters rt_logging_fd_filters
 */
static rt_mutex rt_logging_fd_filters_m = PTHREAD_MUTEX_INITIALIZER;

/**
 * \brief Holds the thread_list required by function-dependent filters
 */
static RTLogFDFilterThreadList *rt_logging_fd_filters_tl = NULL;

/**
 * \brief Mutex for accessing the FD thread_list rt_logging_fd_filters_tl
 */
static rt_mutex rt_logging_fd_filters_tl_m = PTHREAD_MUTEX_INITIALIZER;

static __rt_always_inline__ void *lf_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

/**
 * \brief Helper function used internally to add a FG filter
 *
 * \param file     File_name of the filter
 * \param function Function_name of the filter
 * \param line     Line number of the filter
 * \param listtype The filter listtype.  Can be either a blacklist or whitelist
 *                 filter listtype(RT_LOG_FILTER_BL or RT_LOG_FILTER_WL)
 *
 * \retval  0 on successfully adding the filter;
 * \retval -1 on failure
 */
static int RTLogAddFGFilter(const char *file, const char *function,
                                   int line, int listtype)
{
    RTLogFGFilterFile *fgf_file = NULL;
    RTLogFGFilterFile *prev_fgf_file = NULL;

    RTLogFGFilterFunc *fgf_func = NULL;
    RTLogFGFilterFunc *prev_fgf_func = NULL;

    RTLogFGFilterLine *fgf_line = NULL;
    RTLogFGFilterLine *prev_fgf_line = NULL;

    int found = 0;

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return -1 ;
    }

    if (file == NULL && function == NULL && line < 0) {
        printf("Error: Invalid arguments supplied to RTLogAddFGFilter\n");
        return -1;
    }

    rt_mutex *m = &rt_logging_fg_filters_m[listtype];

    rt_mutex_lock(m);

    fgf_file = rt_logging_fg_filters[listtype];

    prev_fgf_file = fgf_file;
    while (fgf_file != NULL) {
        prev_fgf_file = fgf_file;
        if (file == NULL && fgf_file->file == NULL)
            found = 1;
        else if (file != NULL && fgf_file->file != NULL)
            found = (strcmp(file, fgf_file->file) == 0);
        else
            found = 0;

        if (found == 1)
            break;

        fgf_file = fgf_file->next;
    }

    if (found == 0) {
        RTLogAddToFGFFileList(prev_fgf_file, file, function, line, listtype);
        goto done;
    }

    found = 0;
    fgf_func = fgf_file->func;
    prev_fgf_func = fgf_func;
    while (fgf_func != NULL) {
        prev_fgf_func = fgf_func;
        if (function == NULL && fgf_func->func == NULL)
            found = 1;
        else if (function != NULL && fgf_func->func != NULL)
            found = (strcmp(function, fgf_func->func) == 0);
        else
            found = 0;

        if (found == 1)
            break;

        fgf_func = fgf_func->next;
    }

    if (found == 0) {
        RTLogAddToFGFFuncList(fgf_file, prev_fgf_func, function, line);
        goto done;
    }

    found = 0;
    fgf_line = fgf_func->line;
    prev_fgf_line = fgf_line;
    while(fgf_line != NULL) {
        prev_fgf_line = fgf_line;
        if (line == fgf_line->line) {
            found = 1;
            break;
        }

        fgf_line = fgf_line->next;
    }

    if (found == 0) {
        RTLogAddToFGFLineList(fgf_func, prev_fgf_line, line);
        goto done;
    }

 done:
    rt_mutex_unlock(&rt_logging_fg_filters_m[listtype]);
    rt_logging_fg_filters_present = 1;

    return 0;
}

/**
 * \brief Internal function used to check for matches against registered FG
 *        filters.  Checks if there is a match for the incoming log_message with
 *        any of the FG filters.  Based on whether the filter type is whitelist
 *        or blacklist, the function allows the message to be logged or not.
 *
 * \param file     File_name from where the log_message originated
 * \param function Function_name from where the log_message originated
 * \param line     Line number from where the log_message originated
 * \param listtype The filter listtype.  Can be either a blacklist or whitelist
 *                 filter listtype(RT_LOG_FILTER_BL or RT_LOG_FILTER_WL)
 *
 * \retval  1 if there is a match
 * \retval  0 on no match
 * \retval -1 on failure
 */
static int RTLogMatchFGFilter(const char *file, const char *function, int line,
                              int listtype)
{
    RTLogFGFilterFile *fgf_file = NULL;
    RTLogFGFilterFunc *fgf_func = NULL;
    RTLogFGFilterLine *fgf_line = NULL;
    int match = 1;

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return -1;
    }

    rt_mutex_lock(&rt_logging_fg_filters_m[listtype]);

    fgf_file = rt_logging_fg_filters[listtype];

    if (fgf_file == NULL) {
        rt_mutex_unlock(&rt_logging_fg_filters_m[listtype]);
        return 1;
    }

    while(fgf_file != NULL) {
        match = 1;

        match &= (fgf_file->file != NULL)? !strcmp(file, fgf_file->file): 1;

        if (match == 0) {
            fgf_file = fgf_file->next;
            continue;
        }

        fgf_func = fgf_file->func;
        while (fgf_func != NULL) {
            match = 1;

            match &= (fgf_func->func != NULL)? !strcmp(function, fgf_func->func): 1;

            if (match == 0) {
                fgf_func = fgf_func->next;
                continue;
            }

            fgf_line = fgf_func->line;
            while (fgf_line != NULL) {
                match = 1;

                match &= (fgf_line->line != -1)? (line == fgf_line->line): 1;

                if (match == 1)
                    break;

                fgf_line = fgf_line->next;
            }

            if (match == 1)
                break;

            fgf_func = fgf_func->next;
        }

        if (match == 1) {
            rt_mutex_unlock(&rt_logging_fg_filters_m[listtype]);
            if (listtype == RT_LOG_FILTER_WL)
                return 1;
            else
                return 0;
        }

        fgf_file = fgf_file->next;
    }

    rt_mutex_unlock(&rt_logging_fg_filters_m[listtype]);

    if (listtype == RT_LOG_FILTER_WL)
        return 0;
    else
        return 1;
}

/**
 * \brief Checks if there is a match for the incoming log_message with any
 *        of the FG filters.  If there is a match, it allows the message
 *        to be logged, else it rejects that message.
 *
 * \param file     File_name from where the log_message originated
 * \param function Function_name from where the log_message originated
 * \param line     Line number from where the log_message originated
 *
 * \retval  1 if there is a match
 * \retval  0 on no match
 * \retval -1 on failure
 */
int RTLogMatchFGFilterWL(const char *file, const char *function, int line)
{
    return RTLogMatchFGFilter(file, function, line, RT_LOG_FILTER_WL);
}

/**
 * \brief Checks if there is a match for the incoming log_message with any
 *        of the FG filters.  If there is a match it rejects the logging
 *        for that messages, else it allows that message to be logged
 *
 * \praram file    File_name from where the log_message originated
 * \param function Function_name from where the log_message originated
 * \param line     Line number from where the log_message originated
 *
 * \retval  1 if there is a match
 * \retval  0 on no match
 * \retval -1 on failure
 */
int RTLogMatchFGFilterBL(const char *file, const char *function, int line)
{
    return RTLogMatchFGFilter(file, function, line, RT_LOG_FILTER_BL);
}

/**
 * \brief Adds a Whitelist(WL) fine-grained(FG) filter.  A FG filter WL filter
 *        allows messages that match this filter, to be logged, while the filter
 *        is defined using a file_name, function_name and line_number.
 *
 *        If a particular paramter in the fg-filter(file, function and line),
 *        shouldn't be considered while logging the message, one can supply
 *        NULL for the file_name or function_name and a negative line_no.
 *
 * \param file     File_name of the filter
 * \param function Function_name of the filter
 * \param line     Line number of the filter
 *
 * \retval  0 on successfully adding the filter;
 * \retval -1 on failure
 */
int RTLogAddFGFilterWL(const char *file, const char *function, int line)
{
    return RTLogAddFGFilter(file, function, line, RT_LOG_FILTER_WL);
}

/**
 * \brief Adds a Blacklist(BL) fine-grained(FG) filter.  A FG filter BL filter
 *        allows messages that don't match this filter, to be logged, while the
 *        filter is defined using a file_name, function_name and line_number
 *
 *        If a particular paramter in the fg-filter(file, function and line),
 *        shouldn't be considered while logging the message, one can supply
 *        NULL for the file_name or function_name and a negative line_no.
 *
 * \param file     File_name of the filter
 * \param function Function_name of the filter
 * \param line     Line number of the filter
 *
 * \retval  0 on successfully adding the filter
 * \retval -1 on failure
 */
int RTLogAddFGFilterBL(const char *file, const char *function, int line)
{
    return RTLogAddFGFilter(file, function, line, RT_LOG_FILTER_BL);
}

void RTLogReleaseFGFilters(void)
{
    RTLogFGFilterFile *fgf_file = NULL;
    RTLogFGFilterFunc *fgf_func = NULL;
    RTLogFGFilterLine *fgf_line = NULL;

    void *temp = NULL;

    int i = 0;

    for (i = 0; i < RT_LOG_FILTER_MAX; i++) {
        rt_mutex_lock(&rt_logging_fg_filters_m[i]);

        fgf_file = rt_logging_fg_filters[i];
        while (fgf_file != NULL) {

            fgf_func = fgf_file->func;
            while (fgf_func != NULL) {

                fgf_line = fgf_func->line;
                while(fgf_line != NULL) {
                    temp = fgf_line;
                    fgf_line = fgf_line->next;
                    free(temp);
                }

                if (fgf_func->func != NULL)
                    free(fgf_func->func);
                temp = fgf_func;
                fgf_func = fgf_func->next;
                free(temp);
            }

            if (fgf_file->file != NULL)
                free(fgf_file->file);
            temp = fgf_file;
            fgf_file = fgf_file->next;
            free(temp);
        }

        rt_mutex_unlock(&rt_logging_fg_filters_m[i]);
        rt_logging_fg_filters[i] = NULL;
    }

    return;
}

/**
 * \brief Prints the FG filters(both WL and BL).  Used for debugging purposes.
 *
 * \retval count The no of FG filters
 */
int RTLogPrintFGFilters(void)
{
    RTLogFGFilterFile *fgf_file = NULL;
    RTLogFGFilterFunc *fgf_func = NULL;
    RTLogFGFilterLine *fgf_line = NULL;

    int count = 0;
    int i = 0;

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return 0;
    }

#ifdef DEBUG
    printf("Fine grained filters:\n");
#endif

    for (i = 0; i < RT_LOG_FILTER_MAX; i++) {
        rt_mutex_lock(&rt_logging_fg_filters_m[i]);

        fgf_file = rt_logging_fg_filters[i];
        while (fgf_file != NULL) {

            fgf_func = fgf_file->func;
            while (fgf_func != NULL) {

                fgf_line = fgf_func->line;
                while(fgf_line != NULL) {
#ifdef DEBUG
                    printf("%s - ", fgf_file->file);
                    printf("%s - ", fgf_func->func);
                    printf("%d\n", fgf_line->line);
#endif

                    count++;

                    fgf_line = fgf_line->next;
                }

                fgf_func = fgf_func->next;
            }

            fgf_file = fgf_file->next;
        }
        rt_mutex_unlock(&rt_logging_fg_filters_m[i]);
    }

    return count;
}



/* --------------------------------------------------|--------------------------
 * -------------------------- Code for the FD Filter |--------------------------
 * --------------------------------------------------V--------------------------
 */

/**
 * \brief Checks if there is a match for the incoming log_message with any
 *        of the FD filters
 *
 * \param function Function_name from where the log_message originated
 *
 * \retval 1 if there is a match
 * \retval 0 on no match;
 */
int RTLogMatchFDFilter(__attribute__((unused)) const char *function)
{
#ifndef DEBUG
    return 1;
#else
    RTLogFDFilterThreadList *thread_list = NULL;

    pthread_t self = pthread_self();

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return 0;
    }

    rt_mutex_lock(&rt_logging_fd_filters_tl_m);

    if (rt_logging_fd_filters_tl == NULL) {
        rt_mutex_unlock(&rt_logging_fd_filters_tl_m);
        if (rt_logging_fd_filters != NULL)
            return 0;
        return 1;
    }

    thread_list = rt_logging_fd_filters_tl;
    while (thread_list != NULL) {
        if (pthread_equal(self, thread_list->t)) {
            if (thread_list->entered > 0) {
                rt_mutex_unlock(&rt_logging_fd_filters_tl_m);
                return 1;
            }
            rt_mutex_unlock(&rt_logging_fd_filters_tl_m);
            return 0;
        }

        thread_list = thread_list->next;
    }

    rt_mutex_unlock(&rt_logging_fd_filters_tl_m);

    return 0;
#endif
}

/**
 * \brief Updates a FD filter, based on whether the function that calls this
 *        function, is registered as a FD filter or not.  This is called by
 *        a function only on its entry
 *
 * \param function Function_name from where the log_message originated
 *
 * \retval 1 Since it is a hack to get things working inside the macros
 */
int RTLogCheckFDFilterEntry(const char *function)
{
    RTLogFDFilter *curr = NULL;

    RTLogFDFilterThreadList *thread_list = NULL;
    RTLogFDFilterThreadList *thread_list_temp = NULL;

    //pid_t self = syscall(SYS_gettid);
    pthread_t self = pthread_self();

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return 0;
    }

    rt_mutex_lock(&rt_logging_fd_filters_m);

    curr = rt_logging_fd_filters;

    while (curr != NULL) {
        if (strcmp(function, curr->func) == 0)
            break;

        curr = curr->next;
    }

    if (curr == NULL) {
        rt_mutex_unlock(&rt_logging_fd_filters_m);
        return 1;
    }

    rt_mutex_unlock(&rt_logging_fd_filters_m);

    rt_mutex_lock(&rt_logging_fd_filters_tl_m);

    thread_list = rt_logging_fd_filters_tl;
    while (thread_list != NULL) {
        if (pthread_equal(self, thread_list->t))
            break;

        thread_list = thread_list->next;
    }

    if (thread_list != NULL) {
        thread_list->entered++;
        rt_mutex_unlock(&rt_logging_fd_filters_tl_m);
        return 1;
    }

    if ( (thread_list_temp = lf_kmalloc(sizeof(RTLogFDFilterThreadList))) == NULL) {
        rt_mutex_unlock(&rt_logging_fd_filters_tl_m);
        return 0;
    }

    thread_list_temp->t = self;
    thread_list_temp->entered++;

    rt_logging_fd_filters_tl = thread_list_temp;

    rt_mutex_unlock(&rt_logging_fd_filters_tl_m);

    return 1;
}

/**
 * \brief Updates a FD filter, based on whether the function that calls this
 *        function, is registered as a FD filter or not.  This is called by
 *        a function only before its exit.
 *
 * \param function Function_name from where the log_message originated
 *
 */
void RTLogCheckFDFilterExit(const char *function)
{
    RTLogFDFilter *curr = NULL;

    RTLogFDFilterThreadList *thread_list = NULL;

    //pid_t self = syscall(SYS_gettid);
    pthread_t self = pthread_self();

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return;
    }

    rt_mutex_lock(&rt_logging_fd_filters_m);

    curr = rt_logging_fd_filters;

    while (curr != NULL) {
        if (strcmp(function, curr->func) == 0)
            break;

        curr = curr->next;
    }

    if (curr == NULL) {
        rt_mutex_unlock(&rt_logging_fd_filters_m);
        return;
    }

    rt_mutex_unlock(&rt_logging_fd_filters_m);

    rt_mutex_lock(&rt_logging_fd_filters_tl_m);

    thread_list = rt_logging_fd_filters_tl;
    while (thread_list != NULL) {
        if (pthread_equal(self, thread_list->t))
            break;

        thread_list = thread_list->next;
    }

    rt_mutex_unlock(&rt_logging_fd_filters_tl_m);

    if (thread_list != NULL)
        thread_list->entered--;

    return;
}

/**
 * \brief Adds a Function-Dependent(FD) filter
 *
 * \param Name of the function for which a FD filter has to be registered
 *
 * \retval  0 on success
 * \retval -1 on failure
 */
int RTLogAddFDFilter(const char *function)
{
    RTLogFDFilter *curr = NULL;
    RTLogFDFilter *prev = NULL;
    RTLogFDFilter *temp = NULL;

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return -1;
    }

    if (function == NULL) {
        printf("Invalid argument supplied to RTLogAddFDFilter\n");
        return -1;
    }

    rt_mutex_lock(&rt_logging_fd_filters_m);

    curr = rt_logging_fd_filters;
    while (curr != NULL) {
        prev = curr;

        if (strcmp(function, curr->func) == 0) {

            rt_mutex_unlock(&rt_logging_fd_filters_m);
            return 0;
        }

        curr = curr->next;
    }

    if ( (temp = lf_kmalloc(sizeof(RTLogFDFilter))) == NULL) {
        printf("Error Allocating memory (lf_kmalloc)\n");
        exit(EXIT_FAILURE);
    }

    if ( (temp->func = strdup(function)) == NULL) {
        printf("Error Allocating memory (strdup)\n");
        exit(EXIT_FAILURE);
    }

    if (rt_logging_fd_filters == NULL)
        rt_logging_fd_filters = temp;
    /* clang thinks prev can be NULL, but it can't be unless
     * rt_logging_fd_filters is also NULL which is handled here.
     * Doing this "fix" to shut clang up. */
    else if (prev != NULL)
        prev->next = temp;

    rt_mutex_unlock(&rt_logging_fd_filters_m);
    rt_logging_fd_filters_present = 1;

    return 0;
}

/**
 * \brief Releases all the FD filters added to the logging module
 */
void RTLogReleaseFDFilters(void)
{
    RTLogFDFilter *fdf = NULL;
    RTLogFDFilter *temp = NULL;

    rt_mutex_lock(&rt_logging_fd_filters_m);

    fdf = rt_logging_fd_filters;
    while (fdf != NULL) {
        temp = fdf;
        fdf = fdf->next;
        RTLogReleaseFDFilter(temp);
    }

    rt_logging_fd_filters = NULL;

    rt_mutex_unlock( &rt_logging_fd_filters_m );

    return;
}

/**
 * \brief Removes a Function-Dependent(FD) filter
 *
 * \param Name of the function for which a FD filter has to be unregistered
 *
 * \retval  0 on success(the filter was removed or the filter was not present)
 * \retval -1 on failure/error
 */
int RTLogRemoveFDFilter(const char *function)
{
    RTLogFDFilter *curr = NULL;
    RTLogFDFilter *prev = NULL;

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return -1 ;
    }

    if (function == NULL) {
        printf("Invalid argument(s) supplied to RTLogRemoveFDFilter\n");
        return -1;
    }

    rt_mutex_lock(&rt_logging_fd_filters_m);

    if (rt_logging_fd_filters == NULL) {
        rt_mutex_unlock(&rt_logging_fd_filters_m);
        return 0;
    }

    curr = rt_logging_fd_filters;
    prev = curr;
    while (curr != NULL) {
        if (strcmp(function, curr->func) == 0)
            break;

        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) {

        rt_mutex_unlock(&rt_logging_fd_filters_m);

        return 0;
    }

    if (rt_logging_fd_filters == curr)
        rt_logging_fd_filters = curr->next;
    else
        prev->next = curr->next;

    RTLogReleaseFDFilter(curr);

    rt_mutex_unlock(&rt_logging_fd_filters_m);

    if (rt_logging_fd_filters == NULL)
        rt_logging_fd_filters_present = 0;

    return 0;
}

/**
 * \brief Prints the FG filters(both WL and BL).  Used for debugging purposes.
 *
 * \retval count The no of FG filters
 */
int RTLogPrintFDFilters(void)
{
    RTLogFDFilter *fdf = NULL;
    int count = 0;

    if (rt_logging_module_initialized != 1) {
        printf("Logging module not initialized.  Call RTLogInitLogModule() "
               "first before using the debug API\n");
        return 0;
    }

#ifdef DEBUG
    printf("FD filters:\n");
#endif

    rt_mutex_lock(&rt_logging_fd_filters_m);

    fdf = rt_logging_fd_filters;
    while (fdf != NULL) {
#ifdef DEBUG
        printf("%s \n", fdf->func);
#endif
        fdf = fdf->next;
        count++;
    }

    rt_mutex_unlock(&rt_logging_fd_filters_m);

    return count;
}

/**
 * \brief Helper function used internally to add a FG filter.  This function is
 *        called when the file component of the incoming filter has no entry
 *        in the filter list.
 *
 * \param fgf_file The file component(basically the position in the list) from
 *                 the filter list, after which the new filter has to be added
 * \param file     File_name of the filter
 * \param function Function_name of the filter
 * \param line     Line number of the filter
 * \param listtype The filter listtype.  Can be either a blacklist or whitelist
 *                 filter listtype(RT_LOG_FILTER_BL or RT_LOG_FILTER_WL)
 */
void RTLogAddToFGFFileList(RTLogFGFilterFile *fgf_file,
                                         const char *file,
                                         const char *function, int line,
                                         int listtype)
{
    RTLogFGFilterFile *fgf_file_temp = NULL;
    RTLogFGFilterFunc *fgf_func_temp = NULL;
    RTLogFGFilterLine *fgf_line_temp = NULL;

    if ( (fgf_file_temp = lf_kmalloc(sizeof(RTLogFGFilterFile))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogAddToFGFFileList. Exiting...");
        exit(EXIT_FAILURE);
    }

    if ( file != NULL && (fgf_file_temp->file = strdup(file)) == NULL) {
        printf("Error Allocating memory\n");
        exit(EXIT_FAILURE);
    }

    if ( (fgf_func_temp = lf_kmalloc(sizeof(RTLogFGFilterFunc))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogAddToFGFFileList. Exiting...");
        exit(EXIT_FAILURE);
    }

    if ( function != NULL && (fgf_func_temp->func = strdup(function)) == NULL) {
        printf("Error Allocating memory\n");
        exit(EXIT_FAILURE);
    }

    if ( (fgf_line_temp = lf_kmalloc(sizeof(RTLogFGFilterLine))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogAddToFGFFileList. Exiting...");
        exit(EXIT_FAILURE);
    }

    fgf_line_temp->line = line;

    /* add to the lists */
    fgf_func_temp->line = fgf_line_temp;

    fgf_file_temp->func = fgf_func_temp;

    if (fgf_file == NULL)
        rt_logging_fg_filters[listtype] = fgf_file_temp;
    else
        fgf_file->next = fgf_file_temp;

    return;
}

/**
 * \brief Helper function used internally to add a FG filter.  This function is
 *        called when the file component of the incoming filter has an entry
 *        in the filter list, but the function component doesn't have an entry
 *        for the corresponding file component
 *
 * \param fgf_file The file component from the filter list to which the new
 *                 filter has to be added
 * \param fgf_func The function component(basically the position in the list),
 *                 from the filter list, after which the new filter has to be
 *                 added
 * \param function Function_name of the filter
 * \param line     Line number of the filter
 */
void RTLogAddToFGFFuncList(RTLogFGFilterFile *fgf_file,
                                         RTLogFGFilterFunc *fgf_func,
                                         const char *function, int line)
{
    RTLogFGFilterFunc *fgf_func_temp = NULL;
    RTLogFGFilterLine *fgf_line_temp = NULL;

    if ( (fgf_func_temp = lf_kmalloc(sizeof(RTLogFGFilterFunc))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogAddToFGFFuncList. Exiting...");
        exit(EXIT_FAILURE);
    }

    if ( function != NULL && (fgf_func_temp->func = strdup(function)) == NULL) {
        printf("Error Allocating memory\n");
        exit(EXIT_FAILURE);
    }

    if ( (fgf_line_temp = lf_kmalloc(sizeof(RTLogFGFilterLine))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogAddToFGFFuncList. Exiting...");
        exit(EXIT_FAILURE);
    }

    fgf_line_temp->line = line;

    /* add to the lists */
    fgf_func_temp->line = fgf_line_temp;

    if (fgf_func == NULL)
        fgf_file->func = fgf_func_temp;
    else
        fgf_func->next = fgf_func_temp;

    return;
}

/**
 * \brief Helper function used internally to add a FG filter.  This function is
 *        called when the file and function components of the incoming filter
 *        have an entry in the filter list, but the line component doesn't have
 *        an entry for the corresponding function component
 *
 * \param fgf_func The function component from the filter list to which the new
 *                 filter has to be added
 * \param fgf_line The function component(basically the position in the list),
 *                 from the filter list, after which the new filter has to be
 *                 added
 * \param line     Line number of the filter
 */
void RTLogAddToFGFLineList(RTLogFGFilterFunc *fgf_func,
                                         RTLogFGFilterLine *fgf_line,
                                         int line)
{
    RTLogFGFilterLine *fgf_line_temp = NULL;

    if ( (fgf_line_temp = lf_kmalloc(sizeof(RTLogFGFilterLine))) == NULL) {
        rt_log_error(ERRNO_FATAL, "Fatal error encountered in RTLogAddToFGFLineList. Exiting...");
        exit(EXIT_FAILURE);
    }

    fgf_line_temp->line = line;

    /* add to the lists */
    if (fgf_line == NULL)
        fgf_func->line = fgf_line_temp;
    else
        fgf_line->next = fgf_line_temp;

    return;
}

/**
 * \brief Releases the memory alloted to a FD filter
 *
 * \param Pointer to the FD filter that has to be freed
 */
void RTLogReleaseFDFilter(RTLogFDFilter *fdf)
{
    if (fdf != NULL) {
        if (fdf->func != NULL)
            free(fdf->func);
        free(fdf);
    }

    return;
}
