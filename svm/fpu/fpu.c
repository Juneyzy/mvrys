
#include "sysdefs.h"
#include "fax_decode.h"

extern int librecv_init(int __attribute__((__unused__)) argc,
    char __attribute__((__unused__))**argv);

extern void mixer_test ();
extern void G711_test ();

static LIST_HEAD(decrypt_list);
static INIT_MUTEX(decrypt_list_lock);

#define IPSTR_SIZE	16
#define MPSTR_SIZE	64

struct vpu_host {
	int sock;
	char	mnt[MPSTR_SIZE];
	struct list_head	list;
	void *fax_trapper;
};

struct fax_trapper_t {
	/** unused */
	char	mnt[64];	/** root_path/case   root_path/normal */

	uint16_t	fax_port;
	int	fax_sock;					/** unused */
	atomic64_t	count;
	char	log_level[8], log_form[32], log_dir[64];
	
	rt_mutex	vpu_hl_lock;
	struct	list_head	vpu_hl_head;
	int	hosts;
};

struct  fax_cdr_t {
	uint64_t callid;
	time_t   time;
	unsigned short path;
	unsigned char callflag;
	unsigned char ruleflag;
	unsigned int  recv3;
};

struct rt_decrypt_item_t {
#define	PATH_SIZE	64
	char dotwav_file[32];
	char root_path[PATH_SIZE];
	int  is_fax;
	struct list_head list;
};

static struct fax_trapper_t faxTrapper = {
	.mnt = "/data_1",
    	.log_dir = "/root/vrs/logs",
    	.log_level = "info",
    	.log_form = "[%i] %t - (%f:%l) <%d> (%n) -- ",
	.fax_port = 2012,
	.fax_sock  = -1,
	.hosts = 0,
	.vpu_hl_lock = PTHREAD_MUTEX_INITIALIZER,
	.count = ATOMIC_INIT (0),
};

static inline void __alaw_file_sprintf (char *src, char *alaw_file, struct rt_decrypt_item_t *_this)
{
	sprintf(src, "%s/%s", _this->root_path, _this->dotwav_file);
	sprintf(alaw_file, "%s/%s", _this->root_path, _this->dotwav_file);
}

static inline void __decrypt_enqueue (struct rt_decrypt_item_t *item)
{
	rt_mutex_lock (&decrypt_list_lock);
	list_add_tail (&item->list, &decrypt_list);
	rt_mutex_unlock (&decrypt_list_lock);
}

static inline void decrypt_enqueue (const char *root, struct fax_cdr_t *cdr)
{
	time_t time;
	struct tm *tms, tms_buf;
	uint32_t    pos;
	uint8_t e1_no, ts_no;
	uint64_t callid;
	struct rt_decrypt_item_t *up, *down;

	callid = cdr->callid;
	time = (callid >> 32) & 0xffffffff;
	pos = (callid >> 15) & 0x1ffff;
	e1_no = (pos >> 5) & 0x3f;
	ts_no = pos & 0x1f;

	tms = localtime_r (&time, &tms_buf);

	if (unlikely (!tms)) {
		rt_log_notice ("sss");
		return;
	}

	up		=	(struct  rt_decrypt_item_t *)kmalloc(sizeof(struct rt_decrypt_item_t), MPF_CLR, -1);
	down	=	(struct  rt_decrypt_item_t *)kmalloc(sizeof(struct rt_decrypt_item_t), MPF_CLR, -1);
	
	if (likely(up)) {
		INIT_LIST_HEAD (&up->list);
		if (cdr->ruleflag == 0) {
			snprintf (up->root_path, PATH_SIZE - 1, "%s/%s/%04d-%02d-%02d/%02d/%02d", root,
				"normal", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday, e1_no, ts_no);
		} else {
			snprintf (up->root_path, PATH_SIZE - 1, "%s/%s/%04d-%02d-%02d/%02d/%02d", root,
				"case", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday, e1_no, ts_no);
		}
		sprintf(up->dotwav_file, "%016lx_up", cdr->callid);
		__decrypt_enqueue (up);	
		goto finish;
	}

	if (likely(down)) {
		INIT_LIST_HEAD (&down->list);
		if (cdr->ruleflag == 0) {
			snprintf (down->root_path, PATH_SIZE - 1, "%s/%s/%04d-%02d-%02d/%02d/%02d", root,
				"normal", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday, e1_no, ts_no);
		} else {
			snprintf (down->root_path, PATH_SIZE - 1, "%s/%s/%04d-%02d-%02d/%02d/%02d", root,
				"case", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday, e1_no, ts_no);
		}
		sprintf(down->dotwav_file, "%016lx_down", cdr->callid);
		__decrypt_enqueue (down);
		goto finish;
	}

	rt_log_notice ("Can not allocate memory for fax decrypt item (callid=%lu)", cdr->callid);

finish:
	
	return;
}

static inline struct vpu_host *
vpu_host_alloc (int sock)
{
	struct vpu_host *clnt;
	
	clnt = (struct vpu_host *)kmalloc(sizeof (struct vpu_host), MPF_CLR, -1);
	if (likely (clnt)) {
		INIT_LIST_HEAD (&clnt->list);
		clnt->sock = sock;
	}

	return clnt;
}

static inline int
vpu_host_del (struct vpu_host *clnt, 
				void __attribute__((__unused__))*data, size_t __attribute__((__unused__))s)
{
	struct fax_trapper_t	*rte;

	rte = clnt->fax_trapper;
	if (likely (clnt)) {
		rt_sock_close (&clnt->sock, NULL);
		list_del (&clnt->list);
		kfree (clnt);
		rte->hosts --;
	}

	return 0;
}

static inline int
vpu_host_dummy (struct vpu_host *clnt, 
				void __attribute__((__unused__))*data, size_t __attribute__((__unused__))s)
{
	if (likely (clnt)) {
		printf (" %d", clnt->sock);
	}

	return 0;
}

static inline void vpu_hostlist_foreach (struct fax_trapper_t *rte, 
					int (*routine)(struct vpu_host *, void *, size_t), void *param, size_t s)
{
	struct	vpu_host *__this, *p;
	
	rt_mutex_lock (&rte->vpu_hl_lock);

	list_for_each_entry_safe (__this, p, &rte->vpu_hl_head, list) {
		if (routine)
			routine (__this, param, s);
	}
	
	rt_mutex_unlock (&rte->vpu_hl_lock);
}


static inline void vpu_hostlist_release (struct fax_trapper_t *rte)
{
	struct	vpu_host *__this, *p;
	
	rt_mutex_lock (&rte->vpu_hl_lock);

	list_for_each_entry_safe (__this, p, &rte->vpu_hl_head, list) {
		vpu_host_del (__this, NULL, 0);
	}
	
	rt_mutex_unlock (&rte->vpu_hl_lock);
}

static inline struct vpu_host *vpu_hostlist_add (struct fax_trapper_t *rte, int sock)
{
	struct	sockaddr_in sock_addr;
	struct 	vpu_host	*clnt = NULL;

	if (rt_sock_getpeername (sock, &sock_addr) < 0) {
		rt_sock_close (&sock, NULL);
		goto finish;
	}

	clnt = vpu_host_alloc (sock);

	if (unlikely (!clnt)) {
		rt_sock_close (&sock, NULL);
		goto finish;
		
	} else {
	
		rt_mutex_lock (&rte->vpu_hl_lock);
		rte->hosts ++;
		clnt->fax_trapper = rte;
		list_add_tail (&clnt->list, &rte->vpu_hl_head);
		rt_mutex_unlock (&rte->vpu_hl_lock);
		memcpy (clnt->mnt, rte->mnt, MPSTR_SIZE);

		rt_log_notice ("Peer(VPW) (%s:%d, sock=%d) connected, add to vpw list (total=%d)",
					inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock, rte->hosts);
	}

finish:
	return clnt;
}


static inline void vpu_hostlist_query (struct fax_trapper_t *rte, int sock, 
					int (*routine)(struct vpu_host *, void *, size_t), void *param, size_t s)
{
	struct	vpu_host *__this, *p;
	
	rt_mutex_lock (&rte->vpu_hl_lock);

	list_for_each_entry_safe (__this, p, &rte->vpu_hl_head, list) {
		if (sock == __this->sock) {
			if (routine)
				routine (__this, param, s);
		}
	}
	
	rt_mutex_unlock (&rte->vpu_hl_lock);
}

static void * SGFaxReceiver (void *args)
{

	struct vpu_host *host = (struct vpu_host *)args;
	struct fax_cdr_t	cdr;
	ssize_t s;
	
	FOREVER {
		if (host->sock > 0) {
			memset (&cdr, 0, sizeof (struct fax_cdr_t));
			s = rt_sock_recv (host->sock, &cdr, sizeof(struct  fax_cdr_t));
			if (s > 0) {
				decrypt_enqueue (host->mnt, &cdr);
			}
		}	
	}

	task_deregistry_id(pthread_self());

	return NULL;
}

static void * SGFaxMPointManager (void *args)
{

	struct fax_trapper_t *rte = (struct fax_trapper_t *)args;
	int xerror;
	int tmo, sock;
	char	desc[32] = {0};
	
	FOREVER {
		do {
			rte->fax_sock = rt_serv_sock (0, rte->fax_port, AF_INET);
			if (rte->fax_sock > 0) {
				break;
			}
			
			rt_log_notice ("Listen (port=%d, sock=%d), %s",
						2012, rte->fax_sock, "failure");
			sleep (3);
		} while (rte->fax_sock < 0);

		rt_log_notice ("Ready to Listen (port=%d, sock=%d)",
						rte->fax_port, rte->fax_sock);

		do {
			
			xerror = is_sock_ready (rte->fax_sock, 30 * 1000000, &tmo);
			if (xerror == 0 && tmo == 1)
				continue;
			if (xerror < 0) {
				/** clear clnt list here */
				vpu_hostlist_release (rte);
				rt_sock_close (&rte->fax_sock, NULL);
				break;
			}
			
			sock = rt_serv_accept (0, rte->fax_sock);
			if (sock <= 0) {
				rt_log_error(ERRNO_SOCK_ACCEPT, "%s\n", strerror(errno));
				continue;
			}
			struct vpu_host *host;
			host = vpu_hostlist_add (rte, sock);
			if (likely(host)) {
				sprintf (desc, "vpu%d Task", host->sock);
				task_spawn(desc, 0, NULL, SGFaxReceiver, host);
				task_detail_foreach ();
			}		
		} while (rte->fax_sock > 0);
		
	}

	task_deregistry_id(pthread_self());

	return NULL;
}

static inline void fax_pretend_item ()
{
#define fax_file_x	"5231_fax.down.wav"
#define fax_file_y	"5231_fax.up.wav"

	struct rt_decrypt_item_t *_this = (struct rt_decrypt_item_t *)kmalloc(sizeof (struct rt_decrypt_item_t), MPF_CLR, -1);
	memcpy (&_this->dotwav_file, fax_file_x, strlen (fax_file_x));
	memcpy (&_this->root_path, "/home/svm", strlen ("/home/svm"));
	INIT_LIST_HEAD (&_this->list);
	__decrypt_enqueue (_this);
}

static void * SGFaxDecoder (void __attribute__((__unused__))*args)
{

	struct fax_context_t *fctx;
	struct rt_decrypt_item_t *_this, *p;
	char	file[128], alaw_file[128];


	fctx = (struct fax_context_t *)kmalloc(sizeof (struct fax_context_t), MPF_CLR, -1);
	if (unlikely(!fctx))
		goto finish;
	
	fax_context_init (fctx);

	fax_pretend_item ();
	FOREVER {
		sleep (3);
		rt_mutex_lock (&decrypt_list_lock);
		list_for_each_entry_safe (_this, p, &decrypt_list, list) {
			memset64 (&file, 0, 128);
			__alaw_file_sprintf (file, alaw_file, _this);
			//fax_converto_alaw (file, alaw_file);
			if (!fax_decode_alaw (fctx, file)) {
				
			}
		}
		rt_mutex_unlock (&decrypt_list_lock);
	}

	kfree (fctx);
	
finish:
	task_deregistry_id(pthread_self());

	return NULL;
}


static struct rt_task_t SGFaxDecoderTask = {
    .module = THIS,
    .name = "SG Fax Decode Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &faxTrapper,
    .routine = SGFaxDecoder ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t SGFaxMPointManagerTask = {
    .module = THIS,
    .name = "SG Fax Mount Point Manager Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &faxTrapper,
    .routine = SGFaxMPointManager ,
    .recycle = FORBIDDEN,
};

void fax_logger_open (struct fax_trapper_t *rte)
{
	RTLogInitData *sc_lid = NULL;
	char file[256] = {0};
	char *facility = "local5";
	RTLogLevel level = RT_LOG_NOTSET;
	
    	sc_lid = rt_logging_sclid_init ();
		 
	rt_logging_mkfile(file, 255);

	level = rt_logging_parse_level (rte->log_level);
	rt_logging_init_console (rte->log_form, level, sc_lid);
	rt_logging_init_file (file, rte->log_dir, rte->log_form, level, sc_lid);
	rt_logging_init_syslog (facility, rte->log_form, level, sc_lid);

	rt_logging_deinit ();
	rt_logging_init(sc_lid);
}

static void fax_trapper_init ()
{
	struct fax_trapper_t *rte = &faxTrapper;

	INIT_LIST_HEAD (&rte->vpu_hl_head);

	fax_logger_open (rte);
}

int main (int argc, char **argv)
{
	G711_test ();

extern void thrdpool_test();
	thrdpool_test();


	librecv_init (argc, argv);
	
	fax_trapper_init ();


	mixer_test ();
	
	task_registry (&SGFaxDecoderTask);
	task_registry (&SGFaxMPointManagerTask);
	
	task_run ();

	FOREVER {
		sleep (1000);
	}
	return 0;
}

