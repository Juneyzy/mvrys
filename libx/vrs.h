#ifndef __VRS__H__
#define __VRS__H__

#include "rt_list.h"
#include "rt_pool.h"
#include "vrs_model.h"
#include "vrs_rule.h"
#include "tool.h"
#include "rt_ethxx_packet.h"

#define HAVE_PF_RING

#define VRS_VPW_CFG			"/usr/local/etc/vpw/vpw.yaml"
#define VRS_SYSTEM_BASIC_CFG		"/usr/local/etc/vrs.yaml"
#define VRS_RULE_CFG			"/usr/local/etc/vpw/vrsrules.conf"
#define VRS_VPM_CFG			"/usr/local/etc/vpm/vpm.yaml"

union vrs_exts_sample_mcb_t {
	struct {
		uint64_t	ecd		: 16;	/** encoding. Defined by enum vrs_exts_encode_t */
		uint64_t	dir		: 2;	/** 1-upstream, 2-downstream, unused if THIS.channels = 2 */
		uint64_t	channels	: 2;	/** 1(Mono, single channel), 2(Stereo, dual, channels) */
		uint64_t	quant_bits	: 8;	/** Quantization Bits per Sample. 8,16,24,32 etc.  */
		uint64_t	sample_rate	: 32;	/** Samples per Second, eg. 8000, 16000 etc. */
		uint64_t	resv		: 4;
	};
	uint64_t	data;
};

union vrs_exts_pesdoheader_t {
	struct {
		uint16_t	sp;
		uint16_t	dp;
		uint16_t	s;
		uint16_t	chksum;
	};
	uint64_t	data;
};

struct vrs_exts_header_t {

	/** DWORD0 */
	uint64_t			pesdo_header;

	/** DWORD1 */
	uint64_t			source_sys_id	: 32;
	uint64_t			subsys_id	: 16;
	uint64_t			type	: 8;
	uint64_t			mask	: 8;

	/** DWORD2 */
	uint64_t			sequence;

	/** DWORD3 */
	uint64_t			call_id;

	/** DWORD4 */
	uint64_t			dw4r0	: 8;
	uint64_t			dw4r1	: 8;
	uint64_t			dw4r2	: 8;
	uint64_t			dw4r3	: 8;
	uint64_t		context_size	: 32;

	union vrs_exts_sample_mcb_t	smcb;
	uint8_t				context[];
};

enum vrs_exts_source_t {
	SRC_SYSTEM_ID_YJ	=	0x20204A59,	/** YJ source system ID 0x20204A59 = "YJ  " */
	SRC_SYSTEM_ID_1WAP	=	0x36443531,	/** 0x20504157 = "1WAP" */
};

enum vrs_exts_encode_t {

	/* Subtypes from here on. */
	O_XFORM_PCM_S8		= 0x0001,		/* Signed 8 bit data */
	O_XFORM_PCM_16		= 0x0002,		/* Signed 16 bit data */
	O_XFORM_PCM_24		= 0x0003,		/* Signed 24 bit data */
	O_XFORM_PCM_32		= 0x0004,		/* Signed 32 bit data */
	O_XFORM_PCM_U8		= 0x0005,		/* Unsigned 8 bit data (WAV and RAW only) */

	O_XFORM_FLOAT		= 0x0006,		/* 32 bit float data */
	O_XFORM_DOUBLE		= 0x0007,		/* 64 bit float data */

	O_XFORM_ULAW		= 0x0010,		/* U-Law encoded. */
	O_XFORM_ALAW		= 0x0011,		/* A-Law encoded. */
	O_XFORM_IMA_ADPCM	= 0x0012,		/* IMA ADPCM. */
	O_XFORM_MS_ADPCM	= 0x0013,		/* Microsoft ADPCM. */

	O_XFORM_GSM610		= 0x0020,		/* GSM 6.10 encoding. */
	O_XFORM_VOX_ADPCM	= 0x0021,		/* OKI / Dialogix ADPCM */

	O_XFORM_G721_32		= 0x0030,		/* 32kbs G721 ADPCM encoding. */
	O_XFORM_G723_24		= 0x0031,		/* 24kbs G723 ADPCM encoding. */
	O_XFORM_G723_40		= 0x0032,		/* 40kbs G723 ADPCM encoding. */

	O_XFORM_DWVW_12		= 0x0040, 		/* 12 bit Delta Width Variable Word encoding. */
	O_XFORM_DWVW_16		= 0x0041, 		/* 16 bit Delta Width Variable Word encoding. */
	O_XFORM_DWVW_24		= 0x0042, 		/* 24 bit Delta Width Variable Word encoding. */
	O_XFORM_DWVW_N		= 0x0043, 		/* N bit Delta Width Variable Word encoding. */

	O_XFORM_DPCM_8		= 0x0050,		/* 8 bit differential PCM (XI only) */
	O_XFORM_DPCM_16		= 0x0051,		/* 16 bit differential PCM (XI only) */

	O_XFORM_VORBIS		= 0x0060,		/* Xiph Vorbis encoding. */
};

struct vdu_t{
	int     sock;
	uint32_t     id;
	char		ip[16];
	uint16_t    port;
	atomic_t  flags;
	atomic_t  stop_times;
};

struct	vpw_clnt_t {
	int	sock;
	struct	list_head	list;
	struct	vpm_t	*vpm;
	MQ_ID	mq;		/** Unused. private message queue */
};

#define	VPM_FLGS_CONN_BIT	(0)
#define	VPM_FLGS_RSYNC_BIT (1)
#define	VPM_FLGS_CONN	(1 << VPM_FLGS_CONN_BIT)	/** 1, connected to vpm, otherwise disconnected */
#define	VPM_FLGS_RSYNC (1 << VPM_FLGS_RSYNC_BIT)	/** notify vpw to reload model */
struct vpm_t {

	uint32_t     	id;

	int 		serial_num;

	atomic_t	flags;

	/** used in VPM, 255.255.255.255 */
	char			web[16];
	uint16_t		web_port;
	int			web_sock;
	atomic64_t	seq;

	/**	VPW <-> VPM */
	int			serv_sock;
	struct	list_head	clnt_socks_list;
	int			clnt_socks;
	rt_mutex		clnt_socks_lock;
	MQ_ID		notify_mq, mass_mq, regular_mq, boost_mq, cdb_mq, target_mq;

	/** used in VPW */
	int		sock;
	char     	ip[16];
	uint16_t    	port;
};

#define	CDR_FLGS_CONN_BIT	(0)
#define	CDR_FLGS_CONN	(1 << CDR_FLGS_CONN_BIT)	/** 1, connected to cdr, otherwise disconnected */
struct cdr_t {
	int	sock;
	char    ip[16];
	uint16_t    port;
	atomic_t    flags;
};

typedef struct __hit_second_conf {
    int hit_second_en;
    int update_score;
    int hit_score;
    int topn_max;
    int topn_min;
    char topn_dir[128];
} hit_second_conf_t;

struct vpw_t{
	uint32_t    	id;
	int 		serial_num;
	char		netdev[16];
	atomic_t   score_threshold;
	atomic_t   clue_layer_filter;
	atomic_t   stage_time[3];
	atomic_t   short_voice_enable;
	struct cdr_t cdr;
};

struct __cdr_t {

	union call_link_t	call;
	uint64_t			up_userid;
	uint64_t  	   		down_userid;

	uint64_t  			up_voiceid;
	uint64_t  			down_voiceid;

	uint64_t  			up_ruleid[16];		/** MAX_CLUES_PER_TARGET */
	uint64_t  			down_ruleid[16];		/** MAX_CLUES_PER_TARGET */

	uint32_t  			up_rules_num;
	uint32_t  			down_rules_num;

	int32_t   			up_percent;
	int32_t   			down_percent;

	uint32_t			stage;
	uint32_t			flags;				/** 0-local system CDR, 1-external system CDR. */

};

/**
struct vrs_cdr_t {

	uint32_t			source_sys_id;

	uint64_t			callid;
	uint64_t			up_userid		: 32;
	uint64_t  	   		dn_userid		: 32;

	uint64_t  			up_voiceid		: 32;
	uint64_t  			dn_voiceid		: 32;

	uint64_t  			up_ruleid[16];
	uint64_t  			dn_ruleid[16];

	uint32_t  		ups_rules_num		: 16;
	uint32_t  		dns_rules_num		: 16;

	int32_t   			up_percent;
	int32_t   			dn_percent;

	uint32_t			stage;
	uint32_t			flags;

};
*/

struct vrs_matcher_t {
	atomic_t			sg_enable;

	struct list_head	sg_list_head;
	rt_mutex			sg_list_lock;
	struct modelist_t	*sg_modelist;

	int			matcher_id;
};

struct cdr_hlist{
	struct hlist_head	head;
    rt_mutex mtx;
};

#define CALL_ID_HASHED
struct vrs_trapper_t{
    int vrs_boost_st; // vrs boost开关，等于1的时候开启

    int topx_switch;  //topx alg switch
    int topx_num;

	struct rt_vrstool_t *tool;

	/** call session bucket pool */
	struct rt_pool_t *cs_bucket_pool;

	/** call match bucket pool */
	struct rt_pool_t *cm_bucket_pool;

	/** cdr bucket pool */
	struct rt_pool_t *cdr_bucket_pool;

	struct rt_pool_t cm_msg_bucket_pool[6];

	MQ_ID cdr_mq, vpm_mq;

	char		model_dir[128], tmp_dir[128], vdu_dir[128], thr_path_name[128];
	char		sample_dir[128], mass_dir[128], category_dir[128], targetmatch_dir[128];

	char *vdu_config;
	struct vdu_t    *vdu;

	char *vpm_config;
	struct vpm_t    *vpm;

	char	*vpw_config;
	struct vpw_t	*vpw;
	hit_second_conf_t hit_scd_conf;

	char log_dir[128];
	char	log_level[8];
	char log_form[32];

	uint32_t	perform_tmr;
    uint32_t    statistics_tmr;
    uint32_t    topn_disc_tmr;
	uint32_t	sock_set_detect_tmr;

#ifdef CALL_ID_HASHED
	rt_mutex	lock;
	struct list_head	list;
	struct hlist_head	*call_hlist_head;
	struct cdr_hlist 	*cdr_hlist_head;
#endif

	int	matchers;
	int	wqe_size;
	struct thpool_  *thrdpool_for_matcher;

	atomic_t	matcher_ctrl;
	struct	vrs_matcher_t		*sg_matchers;

	atomic64_t	list_cursor;	/** list_cursor always be 0 in VPM, otherwise increased by every updating. */
};

extern struct vrs_trapper_t vrsTrapper;

extern void yaml_init();

extern struct vrs_trapper_t *vrs_default_trapper ();
extern struct rt_vrstool_t *vrs_default_tool ();
extern void vrs_trapper_set_tool (struct rt_vrstool_t *tool);

static __rt_always_inline__ int clear_bit (atomic_t *atomic_val, int bit)
{
	int cur_val;

	cur_val = atomic_add (atomic_val, 0);
	cur_val &= ~(1 << bit);

	atomic_set (atomic_val, cur_val);

	return 0;
}

static __rt_always_inline__ int	set_bit (atomic_t *atomic_val, int bit)
{
	int cur_val;

	cur_val = atomic_add (atomic_val, 0);
	cur_val |= (1 << bit);

	atomic_set (atomic_val, cur_val);

	return 0;
}


static __rt_always_inline__ int	write_bit (atomic_t *atomic_val, int bit, int val)
{
	if (val)
		return set_bit (atomic_val, bit);
	else
		return clear_bit (atomic_val, bit);
}

static __rt_always_inline__ int	read_bit (atomic_t *atomic_val, int bit)
{
	int	cur_val = 0;

	cur_val = (1 << bit);
	return (atomic_add(atomic_val, 0) & cur_val);
}

static __rt_always_inline__ int	cdr_flags_chk_bit (int bit)
{
	struct vpw_t	*vpw;
	vpw = vrs_default_trapper()->vpw;

	return read_bit (&vpw->cdr.flags, bit);
}

static __rt_always_inline__ int	cdr_flags_set_bit (int bit, int val)
{
	struct vpw_t	*vpw;
	vpw = vrs_default_trapper()->vpw;

	return write_bit (&vpw->cdr.flags, bit, val);
}

static __rt_always_inline__ int	vpm_flags_chk_bit (int bit)
{
	struct vpm_t	*vpm;
	vpm = vrs_default_trapper()->vpm;

	return read_bit (&vpm->flags, bit);
}

static __rt_always_inline__ int	vpm_flags_set_bit (int bit, int val)
{
	struct vpm_t	*vpm;
	vpm = vrs_default_trapper()->vpm;

	return write_bit (&vpm->flags, bit, val);
}

static __rt_always_inline__ void vpw_matcher_disable ()
{
	struct vrs_trapper_t	*rte = vrs_default_trapper();
	atomic_add (&rte->matcher_ctrl, -1);
}

static __rt_always_inline__ void vpw_matcher_enable ()
{
	struct vrs_trapper_t	*rte = vrs_default_trapper();
	atomic_add (&rte->matcher_ctrl, 1);
}

static __rt_always_inline__ int vpw_matcher_enabled ()
{
	struct vrs_trapper_t	*rte = vrs_default_trapper();
	return (1 == atomic_add (&rte->matcher_ctrl, 0));
}


#endif
