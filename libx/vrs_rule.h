#ifndef __VRS_TARGET_H__
#define __VRS_TARGET_H__

#include "rt_list.h"

#define	MAX_TARGETS				(10 *1000)
#define	MAX_CLUES_PER_TARGET	16
#define	INVALID_TARGET (-1)
#define	INVALID_CLUE	 (-1)
#define	INVALID_VID	(-1)

typedef uint64_t	target_id;
typedef	uint32_t	sg_id;

struct owner_t {
	target_id	tid;
	int	vid;
	int	sg_index;
	float	sg_score;
	int	vrmt_index;
	int	cmt[MAX_CLUES_PER_TARGET];	/** CMT clue map table */
	int	cmt_size;	/** The number of CMT */
};


#define V_CLUE_ENA	(1 << 0)
#define V_CLUE_REPORT	(1 << 1)
struct clue_t{
	int	id;
	int	flags;
	int	score;
};

/** VRS RULE MAP TABLE */
struct vrmt_t {
	target_id	id;	/** target id */
	struct clue_t	clue[MAX_CLUES_PER_TARGET];
	int	flags;
	int	target_score;	/** score for this target, unused now */
	int	clues;
	int	slot;			/** current slot in VRMT table */
	struct hlist_node	hlist;
	struct list_head	list;
};

extern int	default_global_score;

#define vrmt_for_each_clue(i, vrmt)\
	for (i = 0; i < MAX_CLUES_PER_TARGET; i++)

#define valid_clue(clue_id)	((int)clue_id != INVALID_CLUE)

extern int vrmt_load(const char *vrmt_file, int __attribute__((__unused__)) option, const char *thr_path);
extern int  vrmt_query (target_id tid, int *slot, struct vrmt_t **_vrmt);
extern int vrmt_query_copyout (target_id tid, int *slot, struct vrmt_t *_vrmt);
extern int vrmt_generate (const char *vrmt_file);
extern void vrmt_random (target_id * tid, int * vid);

#endif

