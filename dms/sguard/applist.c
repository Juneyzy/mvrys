#include "sysdefs.h"
#include "applist.h"
#include "conf.h"
#include "conf_yaml_loader.h"

struct applist_t	appdev_list = {
	.applist_lock = PTHREAD_MUTEX_INITIALIZER,
	.apps = 0,
};

static __rt_always_inline__ 
struct appdev_t *appdev_alloc (char *appdesc, int app_sn)
{
	struct appdev_t *dev;
	
	dev = (struct appdev_t *)kmalloc(sizeof (struct appdev_t), MPF_CLR, -1);
	if (likely (dev)) {
		INIT_LIST_HEAD (&dev->appdev);
		dev->app_sn			=	app_sn;
		dev->rtm_state			=	APPS_RTM_STATE_DFT;
		dev->tv_latest_shutdown	=	time(NULL);
		memcpy (dev->adesc, appdesc, strlen(appdesc));
	}

	return dev;
}

static __rt_always_inline__ 
int appdev_del (struct appdev_t *dev, 
				void __attribute__((__unused__))*data, size_t __attribute__((__unused__))s)
{
	struct applist_t	*applist;

	applist = dev->applist;
	if (likely (dev)) {
		rt_sock_close (&dev->sock4kalive_from_app, NULL);
		rt_sock_close (&dev->rr_mgt_sock, NULL);
		list_del (&dev->appdev);
		kfree (dev);
		applist->apps --;
	}

	return 0;
}

static __rt_always_inline__ 
int appdev_dummy (struct appdev_t *dev, 
				void __attribute__((__unused__))*data, size_t __attribute__((__unused__))s)
{
	if (likely (dev)) {
		printf ("appdesc=\"%s\", appsn=%d, state=%08x, mode=%08x\n", 
			dev->adesc, dev->app_sn, dev->rtm_state, dev->rtm_mode);
	}

	return 0;
}

static __rt_always_inline__ 
int applist_init ()
{
	struct applist_t *applist = &appdev_list;
	
	INIT_LIST_HEAD (&applist->applist_head);
	
	return 0;
}
	
static __rt_always_inline__ 
void applist_foreach (struct applist_t *applist, 
					int (*routine)(struct appdev_t *, void *, size_t), void *param, size_t s)
{
	struct	appdev_t *__this, *p;
	
	rt_mutex_lock (&applist->applist_lock);

	list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {
		if (routine)
			routine (__this, param, s);
	}
	
	rt_mutex_unlock (&applist->applist_lock);
}

static __rt_always_inline__ 
void applist_release (struct applist_t *applist)
{
	struct	appdev_t *__this, *p;
	
	rt_mutex_lock (&applist->applist_lock);

	list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {
		appdev_del (__this, NULL, 0);
	}
	
	rt_mutex_unlock (&applist->applist_lock);
}

static __rt_always_inline__ 
struct appdev_t *applist_add (struct applist_t *applist, struct appdev_t *appdev)
{
	if (appdev) {
		rt_mutex_lock (&applist->applist_lock);
		applist->apps ++;
		appdev->applist = applist;
		list_add_tail (&appdev->appdev, &applist->applist_head);
		rt_mutex_unlock (&applist->applist_lock);
	}
	
	return appdev;
}


static __rt_always_inline__ 
int applist_query (struct applist_t *applist, int app_sn, int app_pid,
					int (*routine)(struct appdev_t *, void *, size_t), void *param, size_t s)
{
	int	xerror = -1;
	struct	appdev_t *__this, *p;
	
	rt_mutex_lock (&applist->applist_lock);

	if (app_sn != APPLIST_INVALID_APPSN) {
		list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {
			if (app_sn == __this->app_sn) {
				xerror = 0;
				if (routine)
					xerror = routine (__this, param, s);
				goto finish;
			}
		}
	}

	if (app_pid != APPLIST_INVALID_APPID) {
		list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {
			if (app_pid == __this->app_pid) {
				xerror = 0;
				if (routine)
					xerror = routine (__this, param, s);
				goto finish;
			}
		}

	}
		
finish:
	rt_mutex_unlock (&applist->applist_lock);

	return xerror;
}

int applist_config (char *guide_cfg)
{
	ConfNode *base;
	ConfNode *child;
	ConfNode *cch;

	struct	applist_t	*applist = &appdev_list;
	char *node = "applications";
	char	*cfg;

	applist_init ();
	
	ConfInit();

	cfg = guide_cfg;
	if (!guide_cfg)
		cfg = APPS_CFG_GUIDE_RUNNING;
	
	if (ConfYamlLoadFile (cfg) !=0) {
		rt_log_notice ("%s", cfg);
		return -1;
	}

	base = ConfGetNode (node);
	if (base == NULL) {
		ConfDeInit ();
		rt_log_notice ("No such node(%s) [%s]", node, cfg);
		return -1;
	}

	TAILQ_FOREACH (child, &base->head, next) {
		TAILQ_FOREACH (cch, &child->head, next) {
			int sn = integer_parser (cch->val, 1, APPLIST_MAX_APPSN);
			if (sn > 0) {
				applist_add (applist, appdev_alloc (cch->name, sn));
			}
		}
	}

	applist_foreach (applist, appdev_dummy, NULL, -1);

	ConfDeInit();
	
	return 0;
}

static __rt_always_inline__ 
int applist_dev_read_from_file (struct appdev_t *dev, 
						const char *rootpath, char *appdesc, char *private_yaml)
{

	char	realpath[256], node[32], *__private_yaml = private_yaml;
	ConfNode *base;
	ConfNode *child;
	ConfNode *cch;
		
	if (rootpath) {
		sprintf (realpath, "%s/%s", rootpath, private_yaml);
		__private_yaml = realpath;
	}

	
	
	if (ConfYamlLoadFile(__private_yaml) != 0) {
		rt_log_notice ("%s ", __private_yaml);
		ConfDeInit();
		return -1;
	}

	sprintf (node, "%s", appdesc);
	base = ConfGetNode (node);
	if (base == NULL) {
		rt_log_notice ("No such node(%s) [%s]", node, __private_yaml);
		return -1;
	}

	TAILQ_FOREACH (child, &base->head, next) {
		
		if (strcase_equal (child->name, "version")) {
			strcpy (dev ->aversion, child->val);
		}

		if (strcase_equal (child->name, "private-config")) {
			strcpy (dev ->acfg, child->val);
		}
	}

	sprintf (node, "%s.executives", appdesc);
	base = ConfGetNode (node);
	if (base == NULL) {
		rt_log_notice ("No such node(%s) [%s]", node, __private_yaml);
		return -1;
	}

	TAILQ_FOREACH (child, &base->head, next) {

		TAILQ_FOREACH (cch, &child->head, next) {
			if (strcase_equal (cch->name, "cmd")) {
				strcpy (dev ->startup_dl, cch->val);
			}
			if (strcase_equal (cch->name, "status")) {

				if (strstr (cch->val, "daemon")) {
					
					dev->rtm_mode |= A_RTM_MODE_DAEMONIZED;

					if (strstr (cch->val, "keepalive"))
						dev->rtm_mode |= A_RTM_MODE_KALIVED;
				}

				else {
					
					dev->rtm_mode |= A_RTM_MODE_RIOC;

					if (strstr (cch->val, "keepalive"))
						dev->rtm_mode |= A_RTM_MODE_KALIVED;
				}
			}
		}
	}

	return 0;
}

int applist_dev_read ()
{
	DIR *dir;
	struct dirent *d;
	struct	applist_t	*applist = &appdev_list;
	struct	appdev_t *__this, *p;
	char	private_yaml [32];
	
	dir = opendir(APPS_CFG_PRIVATE_PATH);
	if (dir == NULL) {
		rt_log_notice ("%s", strerror(errno));
		return -1;
	}

	ConfInit();

	while ((d = readdir(dir))!=NULL) {

		rt_mutex_lock (&applist->applist_lock);
		list_for_each_entry_safe (__this, p, &applist->applist_head, appdev) {
			sprintf (private_yaml, "%s.yaml", __this->adesc);
			if ((strstr(d->d_name,".yaml")) != 0
				&& !strcmp (private_yaml, d->d_name)) {
					applist_dev_read_from_file (__this, APPS_CFG_PRIVATE_PATH, __this->adesc, private_yaml);
			}
		}
		rt_mutex_unlock (&applist->applist_lock);
	}
		
	applist_foreach (applist, appdev_dummy, NULL, -1);

	ConfDeInit();

	return 0;
}

static __rt_always_inline__ 
pid_t appdev_fork (char *rundesc)
{
	char *argv[20]; 
	char *p; 
	int i = 0;
	pid_t	app_id;
	
	app_id = fork();

	if (app_id < 0)
		return -1;

	if (app_id > 0)
		return app_id;
	
	p = strtok (rundesc, " ");
	
	while (p != NULL) {
		argv[i] = p; 
		++ i;	
		p = strtok (NULL, " ");
	}

	argv[i] = NULL;	
	execvp (argv[0], argv);

	exit (0);
}

void appdev_run (struct appdev_t *dev)
{

	if (dev->tv_1st_startup == 0)
		dev->tv_1st_startup	=	time(NULL);

	dev->tv_latest_startup = time(NULL);

	waitpid (-1, NULL, WNOHANG);

	dev->app_pid = appdev_fork (dev->startup_dl);
	if (dev->app_pid > 0) {
		dev->rtm_state = A_RTM_STATE_RUNNING;
	}
}

int applist_dev_exist (int app_sn, int app_pid)
{
	return !applist_query (&appdev_list, app_sn, app_pid, NULL, NULL, -1);
}
