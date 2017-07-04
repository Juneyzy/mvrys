#include "head.h"
#include "link.h"
#include "conn_serv.h"
#include "util-daemon.h"
#include "pkt_fifo.h"
#include "atomic.h"
PQUEUE queue;
CONF_T conf_para;
session_t voice_info[SR155_NUM][E1_NUM][TS_NUM];
pkt_fifo_t pkt_fifos[WORKER_NUM];
//TpThreadPool *tp_pool; 
vrs_t *vrs_msg;
pthread_mutex_t mutex;

enum vdu_debug_cmd
{
    VDU_DEBUG_INIT,
    VDU_DEBUG_GET_STAT,
    VDU_DEBUG_CLR_STAT
};

static int g_vdu_pid = 0;

void vdu_debug_usage()
{
    printf("\n");
    printf("===================vdu debug usage=====================\n");    
    printf("[vdu debug info             :] print vdu debug cmd info\n");
    printf("[vdu debug get statistics   :] print vdu packet statistics info\n");    
    printf("[vdu debug clr statistics   :] clear vdu packet statistics info\n");
    printf("\n");
}

void vdu_debug_parse(int argc, char *argv[])
{
    if ((argc == 4) && !(strcmp(argv[2], "get")) && (argv[3][0] == 's'))
    {
        kill(g_vdu_pid, SIGUSR2);
    }
    else if ((argc == 4) && !(strcmp(argv[2], "clr")) && (argv[3][0] == 's'))
    {
        kill(g_vdu_pid, SIGINT);
    }
    else
    {
        vdu_debug_usage();
    }
}

static void vdu_debug_print_handle(__attribute__((unused))int signo)
{
    vdu_print_statistics_info();
}

static void vdu_debug_clear_handle(__attribute__((unused))int signo)
{
    vdu_clear_statistics_info();
}

void vdu_get_pid_by_name(char *task_name)
{
    FILE *fp;
    DIR *dir;
    struct dirent *ptr;
    char filepath[64] = {0};
    char cur_task_name[64] = {0};
    char buf[1024] = {0};
    int pid = 0;

    pid = getpid();
    dir = opendir("/proc"); //打开路径
    if (NULL == dir)
    {
        printf("opendir error , %s\n", strerror(errno));
        return ;
    }
    while ((ptr = readdir(dir)) != NULL) //循环读取路径下的每一个文件/文件夹
    {
        if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))
        {
            continue;
        }
        if (DT_DIR != ptr->d_type)
        {
            continue;
        }
        sprintf(filepath, "/proc/%s/status", ptr->d_name);//生成要读取的文件的路径
        fp = fopen(filepath, "r");//打开文件
        if (NULL != fp)
        {
            if( fgets(buf, 1023, fp) == NULL)
            {
                fclose(fp);
                continue;
            }
            sscanf(buf, "%*s %s", cur_task_name);
            //如果文件内容满足要求则打印路径的名字（即进程的PID）
            if (!strcmp(task_name, cur_task_name))
            {
                sscanf(ptr->d_name, "%d", &g_vdu_pid);
                fclose(fp);
                if (pid == g_vdu_pid)
                {
                    continue;
                }
                break;
            }
            fclose(fp);
        }
    }
    closedir(dir);//关闭路径
}

int main(int argc, const char *argv[])
{
    if ((argc >= 3) && (!strcmp(argv[1], "debug")))
    {
        vdu_get_pid_by_name("vdu");
        vdu_debug_parse(argc, (char **)argv);
        return 0;
    }

	Daemonize();
	open_applog("vdu", LOG_NDELAY, LOG_LOCAL0);
	applog_set_debug_mask(0);
	memset(&conf_para, 0, sizeof(conf_para));
	//tp_pool = tp_create(TP_NUM, TP_NUM);
	//tp_init(tp_pool);
	
	signal(SIGUSR2, vdu_debug_print_handle);
	signal(SIGINT, vdu_debug_clear_handle);

	param_parser(argc, (char **)argv);
	get_conf_para();
	get_vrs_conf();
	get_dms_conf();		
	init_atomic_counter();
	init_vrs_msg();
	init_pkt_fifo();
	init_voice_info();
	pthread_t pid, 
	//		  voice_handle_id, 
			  voice_handle_id[WORKER_NUM], 
			  vrs_id, 
			  recv_vrs_id,
			  register_sguard_id,
			  connect_ma_id,
			  stop_cmd_id;
	int ret = 0;
	uint64_t i;

	pthread_mutex_init(&mutex, NULL);
	queue = create_queue();
	ret = pthread_create(&pid, NULL, thread_delivery, NULL);
	if (ret == -1) {
		applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create pthread is fail");
		return -1;
	}
#if 0
	ret = pthread_create(&voice_handle_id, NULL, thread_voice_handle, NULL);
	if (ret == -1) {
		syslog(LOG_INFO, "create thread_voice_handle is fail");
		return -1;
	}
#endif
	for (i = 0; i < WORKER_NUM; i ++) {
		ret = pthread_create(&voice_handle_id[i], NULL, thread_voice_handle, (void *)(uint64_t)i);
		if (ret == -1) {
			applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create thread_voice_handle is fail");
			return -1;
		}
	}
	ret = pthread_create(&vrs_id, NULL, get_vrs_stop_cmd, NULL);
	if (ret == -1) {
		applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create get_vrs_stop_cmd is fail");
		return -1;
	}

	ret = pthread_create(&recv_vrs_id, NULL, thread_stop_session, NULL);
	if (ret == -1) {
		applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create thread_stop_session is fail");
		return -1;
	}
	ret = pthread_create(&stop_cmd_id, NULL, thread_get_stop_cmd, NULL);
	if (ret == -1) {
		applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create thread_get_stop_cmd is fail");
		return -1;
	}
	ret = pthread_create(&register_sguard_id, NULL, thread_register_sguard, NULL);
	if (ret == -1) {
		applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create thread_register_sguard is fail");
		return -1;
	}
	
	ret = pthread_create(&connect_ma_id, NULL, thread_connect_ma, NULL);
	if (ret == -1) {
		applog(APP_LOG_LEVEL_ERR, VDU_LOG_MASK_BASE, "create thread_connect_ma is fail");
		return -1;
	}

	pthread_join(connect_ma_id, NULL);
	pthread_join(stop_cmd_id, NULL);
	pthread_join(register_sguard_id, NULL);
	pthread_join(recv_vrs_id, NULL);
	//pthread_join(voice_handle_id, NULL);
#if 1
	for (i = 0; i < WORKER_NUM; i ++) {
		pthread_join(voice_handle_id[i], NULL);
	}
#endif
	pthread_join(vrs_id, NULL);
	pthread_join(pid, NULL);	
//	tp_close(tp_pool, 1 );
	pthread_mutex_destroy(&mutex);	
	release_memory();

	return 0;
}
