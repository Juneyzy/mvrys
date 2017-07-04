#include "sysdefs.h"
#include "applist.h"

#define	APPUTILS_FLAG_APPID	0
#define	APPUTILS_FLAG_SNID	1

int apputils_decode_idx (char *appdesc, int *idx, int flags)
{
	char ac_cmd[64] = {0};
	char ac_buf[500] = {0};
	int pid = 0;
	FILE *fp = NULL;
	int sn = 0;
	char *pc_tmp = NULL;

	
	sprintf (ac_cmd, "ps axj | grep %s", appdesc);
	
	fp = popen (ac_cmd, "r");
	if (fp == NULL ) {
		rt_log_notice ("%s", strerror(errno));
		return -1;
	}

	while ((fgets(ac_buf, sizeof(ac_buf), fp))!=NULL) {

		if (sscanf(ac_buf, "%*d %d %*d %*d %*s %*d %*s %*d %*s %[^\n]", &pid, ac_cmd) == 2) {
			if (flags == APPUTILS_FLAG_APPID) {

				if (strcmp(ac_cmd, appdesc) == 0) {
					*idx = pid;
					pclose (fp);
					return 0;
				}
			} else  {
				//if (strcmp(ac_cmd, appdesc) == 0) 
				{
					if ((pc_tmp = strstr (ac_cmd," --sn")) != NULL) {
						sn = 0;
						if (sscanf (pc_tmp, " --sn %d", &sn) == 1) {
							*idx = sn;
							pclose (fp);
							return 0;
						}
					}
				}
			}
		}        
	}
  
	pclose (fp);
	return -1;
}

int apputils_decode_owner (char *cp_owner, int pid)
{
	char ac_cmd[100];	
	FILE *fp;
	int xerror = -1;
	
	sprintf(ac_cmd,"ps -f -p %d | awk '{print $1}' |tail -n 1", pid);
	
	fp = popen (ac_cmd, "r");
	if (fp == NULL ) {
		rt_log_notice ("%s", strerror(errno));
		return -1;
	}

	if (fgets (ac_cmd, sizeof (ac_cmd), fp) != NULL) {
		if (!strcmp (ac_cmd, "UID"))
			goto finish;
		
		strcpy (cp_owner, ac_cmd);
		xerror = 0;
	}

finish:
	pclose (fp);

	return xerror;
}

int apputils_appsn_alloc ()
{
	int appsn = APPLIST_INVALID_APPSN;

	for (appsn = 1; appsn < APPLIST_MAX_APPSN; appsn++) {
		if (applist_dev_exist (appsn, APPLIST_INVALID_APPID))
			continue;
		break;
	}
    
	return appsn;
}

void * ApplistMTask (void *param)
{
	struct	appdev_t *__this, *p;
	struct	applist_t	*applist = &appdev_list;
	char	tmnow[64];
	
	param = param;

	FOREVER {

		rt_mutex_lock (&applist->applist_lock);

		list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {
			if (__this->rtm_state == A_RTM_STATE_RUNNING) {
				int pid = 0;
				if (apputils_decode_idx (__this->startup_dl, &pid, APPUTILS_FLAG_APPID) < 0) {
					__this->rtm_state = A_RTM_STATE_DEATH;
					__this->tv_latest_shutdown = time(NULL);

					rt_tms2str (__this->tv_latest_shutdown, EVAL_TM_STYLE_FULL, tmnow, 63);
					rt_log_error (ERRNO_FATAL, 
						"\"%s(%d)\" shutting down,  LatestTime=\"%s)\" ...", __this->startup_dl, __this->app_pid, tmnow);
				}
			}
		}
		
		rt_mutex_unlock (&applist->applist_lock);

		sleep (1);
	}
}


void * ApplistGTask (void *param)
{
	struct	appdev_t *__this, *p;
	struct	applist_t	*applist = &appdev_list;
	char	tmnow[64], tm1st[64];
	
	param = param;

	FOREVER {

		rt_mutex_lock (&applist->applist_lock);

		list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {

			if (__this->rtm_state == A_RTM_STATE_DEATH ||
				__this->rtm_state == A_RTM_STATE_RESTARTING) {

				appdev_run (__this);
				
				if (__this->rtm_mode & A_RTM_MODE_DAEMONIZED) {
					; /** reserved */
				}
				
				rt_tms2str (__this->tv_latest_shutdown, EVAL_TM_STYLE_FULL, tmnow, 63);
				rt_tms2str (__this->tv_1st_startup, EVAL_TM_STYLE_FULL, tm1st, 63);

				rt_log_info ( 
						"\"%s(%d)\" starting up, FirstTime=\"%s\", LatestTime=\"%s)\" ...", 
						__this->startup_dl, __this->app_pid, tm1st, tmnow);
			}
		}
		
		rt_mutex_unlock (&applist->applist_lock);

		sleep (1);
	}
}


static struct rt_task_t AMSApplistGTask =
{
    .module = THIS,
    .name = "AMS Application Startup Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = ApplistGTask,
};

static struct rt_task_t AMSApplistMTask =
{
    .module = THIS,
    .name = "AMS Application Self-Check Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = ApplistMTask,
};

void apputils_test ()
{
	int id;
	char owner[36], *app = "app";
		
	applist_config (NULL);

	applist_dev_read ();

	
	task_registry (&AMSApplistGTask);
	task_registry (&AMSApplistMTask);
	
	apputils_decode_idx (app, &id, APPUTILS_FLAG_APPID);
	apputils_decode_owner (owner, id);
	printf ("%s: %s\n", app, owner);


	//ams_lsock_test ();
	
}


