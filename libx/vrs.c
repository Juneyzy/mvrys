#include "sysdefs.h"
#include "vrs.h"
#include "conf.h"

static struct vdu_t	vdu = {
	.sock = -1,
	.id = 0,
	.ip = "127.0.0.1",
	.port = 20000,
	.flags = ATOMIC_INIT(0),
	.stop_times = ATOMIC_INIT(0),
};

static struct vpm_t vpm = {
	.sock = -1,
	.id = -1,
	.ip = "192.168.50.4",
	.port = 2050,
	
	.web_sock = -1,
	.web = "192.168.50.12",
	.web_port = 2020,
	.seq = ATOMIC_INIT(0),
	.serv_sock = -1,
	.clnt_socks = 0,
	.clnt_socks_lock = PTHREAD_MUTEX_INITIALIZER,
	.notify_mq	= MQ_ID_INVALID,
	.mass_mq = MQ_ID_INVALID,
	.regular_mq = MQ_ID_INVALID,
	.flags = ATOMIC_INIT(0),
};

static struct vpw_t	vpw = {
	.netdev = "wlp3p2",
	.id = -1,
	.score_threshold = ATOMIC_INIT(60),
	.clue_layer_filter = ATOMIC_INIT(1),
	.stage_time = {ATOMIC_INIT(30), ATOMIC_INIT(90), ATOMIC_INIT(180)},
	.short_voice_enable = ATOMIC_INIT(1),
	.cdr = {-1, "192.168.50.3", 2015, ATOMIC_INIT(0)},
};

struct vrs_trapper_t vrsTrapper = {
	.tool = NULL,
	.cs_bucket_pool = NULL,
    	.vdu = &vdu,
	.vpm = &vpm,
    	.vpw = &vpw,
    	.model_dir = "/root/vrs/modupload",
    	.sample_dir = "/root/vrs/fileupload",
        .mass_dir = "/root/vrs/fullmatch",
        .category_dir = "/root/vrs/crossmatch",
    	.tmp_dir = "/root/vrs/temp",
    	.vdu_dir = "/root/vrs/vdu_model",
    	.log_dir = "/root/vrs/logs",
    	.log_level = "info",
    	.log_form = "[%i] %t - (%f:%l) <%d> (%n) -- ",
    	.list_cursor = ATOMIC_INIT(0),
    	.matchers = 6,
	.wqe_size = 4096,
	.thrdpool_for_matcher = NULL,
	.matcher_ctrl = ATOMIC_INIT (0),	/** disable Matchers before modelist load ready. */
};

void yaml_init()
{
    ConfDeInit();
    ConfInit();
}

struct vrs_trapper_t *vrs_default_trapper ()
{
	return &vrsTrapper;
}

void vrs_trapper_set_tool (struct rt_vrstool_t *tool)
{
	if (likely(tool))
		vrs_default_trapper()->tool = tool;
}
