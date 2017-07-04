#ifndef INOTIFYTOOLS_P_H
#define INOTIFYTOOLS_P_H

#include "redblack.h"

struct rbtree *inotifytools_wd_sorted_by_event(int sort_event);

typedef struct watch {
	char *filename;
	int wd;
	int hit_access;
	int hit_modify;
	int hit_attrib;
	int hit_close_write;
	int hit_close_nowrite;
	int hit_open;
	int hit_moved_from;
	int hit_moved_to;
	int hit_create;
	int hit_delete;
	int hit_delete_self;
	int hit_unmount;
	int hit_move_self;
	int hit_total;
} watch;

#endif
