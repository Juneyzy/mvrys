#ifndef __VRS__H__
#define __VRS__H__

struct vdu_t{
	int     sock;
	uint32_t     ip;
	uint16_t    port;
	atomic_t  flags;
	atomic_t  stop_times;
};

struct vpm_t{
	uint32_t     ip;
	uint16_t    port;
	atomic_t  flags;
};

struct vpw_t{
	uint32_t     ip;
	uint16_t    port;
	atomic_t  flags;
};

struct cdr_t
{
	uint64_t  callid;
	uint64_t  up_userid;
	uint64_t  down_userid;

	uint64_t  up_voiceid;
	uint64_t  down_voiceid;

	uint64_t  up_ruleid[16];
	uint64_t  down_ruleid[16];

	uint32_t  up_rules_num;   //匹配规则数量
	uint32_t  down_rules_num;    //匹配规则数量

	int32_t   up_percent;
	int32_t   down_percent;

	uint32_t  stage;
	uint32_t  flag;
};

extern struct vdu_t	vdu;
extern struct vpm_t	vpm;
extern struct	vpw_t	vpw;

#endif
