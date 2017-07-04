#ifndef __RT_ETHXX_REPORTER_H__
#define __RT_ETHXX_REPORTER_H__


/**
 * @function rt_report_proc
 * @brief proccess a given file with do_job function.
 * @param param0   Unused.
 * @param param1   Unused.
 * @param routine     Pointer to the function that will perform the task.
 * @param argument Argument to be passed to the Routine.
 * @return the number of file have been reported(not proccced).
 */
int
rt_report_proc(void __attribute__((__unused__))*param0,
               void __attribute__((__unused__))*param1,
               void(*routine)(char *file, void *resv), void *argument);


#endif

