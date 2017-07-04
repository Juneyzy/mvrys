
/** 
*
*   Not Recommended after the revision of 126
*/
#include "sysdefs.h"
#include "capture.h"

#ifndef RT_ETHXX_CAPTURE_ADVANCED
static void *
recipient (void * __attribute__((__unused__))param)
{
#define __pcap_filepath_size__ 2047
    message msg = NULL;
    int msg_len = -1;
    char *file_path = NULL;
    char command[__pcap_filepath_size__ + 1] = {0};
    int ret = -1;
    
    FOREVER{
        msg = NULL;
        msg_len = -1;

        if (0 != (spasr_pkt_mq_recv (captor_qid, (void **)&msg, &msg_len)))
            continue;

        if (!msg)
            continue;

        file_path = (char *)msg;
        memset (command, 0 , __pcap_filepath_size__);
        snprintf(command, __pcap_filepath_size__,  "rm %s", file_path);
        ret = system(command);
        if ((-1 == ret) || 
            (!WIFEXITED(ret)) || 
            (0 != WEXITSTATUS(ret))){
            rt_log_debug("rm %s failure!\n", file_path);
        }

        free (msg);
    }
    
    return NULL;
}
#endif
int main (int argc, char **argv)
{
    librecv_init (argc, argv);
    #ifndef RT_ETHXX_CAPTURE_ADVANCED
    task_spawn ("Sample for testing librecv", 0, NULL, recipient, NULL);
    #endif
    FOREVER{
        sleep(1000);
    }

    return 0;
}

