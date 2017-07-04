#include "sysdefs.h"
#include "capture.h"

#ifdef RT_ETHXX_CAPTURE_ADVANCED
static void
rt_ethxx_do_job(char *file,
    void __attribute__((__unused__)) *resv)
{
    if (file){
        file = file;
        FILE *fp = fopen("trash.txt", "a+");
        rt_file_write(fp, file, 0, NULL);
        fclose(fp);
    }
}


static void *
rt_report_routine(void __attribute__((__unused__))*argvs)
{
   FOREVER{
       rt_report_proc(NULL, NULL, rt_ethxx_do_job, NULL);
   }
   return NULL;
}
#endif

int main (int argc, char **argv)
{
    librecv_init (argc, argv);

    #ifdef RT_ETHXX_CAPTURE_ADVANCED
    task_spawn("Sample for testing librecv", 0, NULL, rt_report_routine, NULL);
    #endif

    FOREVER{
        sleep(1000);
    }

    return 0;
}
