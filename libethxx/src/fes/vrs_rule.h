#ifndef __VRS_TARGET_H__
#define __VRS_TARGET_H__

#define	INVALID_TARGET	-1
#define	INVALID_CLUE	-1
#define	MAX_TARGETS	5
#define	MAX_CLUES_PER_TARGET	16

typedef uint64_t	target_id;
typedef	uint32_t	sg_id;


struct clue_t{
	int	id;
	int	flags;
};

/** VRS RULE MAP TABLE */
struct vrmt_t {
	target_id	id;	/** target id */
	struct clue_t	clue[MAX_CLUES_PER_TARGET];
	int	flags;
	int	clues;
};

extern int  vrmt_lookup(target_id tid, int *slot, struct vrmt_t **_table);

#endif

