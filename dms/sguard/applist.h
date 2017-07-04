#ifndef __APPLIST_H__
#define __APPLIST_H__

#define	APPLIST_INVALID_APPID		(-1)
#define	APPLIST_INVALID_APPSN		(-1)

#define	APPLIST_MAX_APPSN			(1000)
#define	APPLIST_MAX_MGR_APPLICATIONS	255

#define	A_RTM_STATE_RESERVED		(1 << 0)
#define	A_RTM_STATE_STARTUP		(1 << 1)		/** STARTUP IN PROGRESS */
#define	A_RTM_STATE_RUNNING		(1 << 2)		/** Have Startupped, Running */
#define	A_RTM_STATE_CLOSING		(1 << 3)		/** CLOSE IN PROGRESS */
#define	A_RTM_STATE_RESTARTING	(1 << 4)		/** Restart in progress */
#define	A_RTM_STATE_DEATH		(1 << 16)


#define	A_RTM_MODE_DAEMONIZED	(1 << 1)		/** Running in Daemon mode */
#define	A_RTM_MODE_KALIVED		(1 << 2)
#define	A_RTM_MODE_RIOC			(1 << 3)		/** Running in "Run Its Own Course" mode */

#define	APPS_CFG_PRIVATE_PATH		"config/applications"

/** Appications Guide Running */
#define	APPS_CFG_GUIDE_RUNNING		"config/run_template.yaml"

#define	APPS_RTM_STATE_DFT		(A_RTM_STATE_DEATH)


struct applist_t {
	rt_mutex	applist_lock;
	struct list_head	applist_head;
	int	apps;
};

#define	APPSTR_SIZE	64
struct appdev_t {

	int	app_sn;	/** SN. */

	pid_t	app_pid;

	char	adesc[APPSTR_SIZE];
	
	char	aversion[APPSTR_SIZE];

	/** Realpath of Configuration for THIS APP */
	char	acfg[APPSTR_SIZE];

	/** startup description language */
	char	startup_dl[APPSTR_SIZE];

	int	rtm_state;
	
	int	rtm_mode;
	
	void	*applist;
	
	struct list_head	appdev;

	/** Sock for Applications */
	int	sock4kalive_from_app;
	
	/** Register & Report Start/Stop/Restart sock to MA */
	int	rr_mgt_sock;

	uint64_t	tv_1st_startup;
	uint64_t	tv_latest_startup;
	uint64_t	tv_latest_shutdown;
};

extern struct applist_t	appdev_list;

extern void appdev_run (struct appdev_t *dev);
extern int applist_config (char *guide_cfg);
extern int applist_dev_read ();
extern int applist_dev_exist (int app_sn, int app_pid);
extern void * ApplistGTask (void *param);

#endif

