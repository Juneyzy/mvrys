#include "sysdefs.h"
#include "vrs_rule.h"

atomic_t		vrmt_count = ATOMIC_INIT(0);
struct vrmt_t	vrmts[MAX_TARGETS];


int vrmt_lookup(target_id tid, int *slot, struct vrmt_t **_vrmt)
{

	int	vrmt_slot = 0;
	int	xerror = (-ERRNO_NO_ELEMENT);
	struct vrmt_t	*vrmt;
	

	if(likely((int64_t)tid == (int64_t)INVALID_TARGET)){
		rt_log_error(ERRNO_INVALID_ARGU, 
				"Target lookup: invalid id %lu", tid);
		return xerror;
	}
	
	for (vrmt_slot = 0; vrmt_slot < MAX_TARGETS; vrmt_slot++) {
		vrmt = &vrmts[vrmt_slot];
		if(vrmt->id == tid) {
			*slot = vrmt_slot;
			*_vrmt = vrmt;
			xerror = XSUCCESS;
			break;
		}
	}
	
	return xerror;
}

static inline void vrmt_init()
{

	int	vrmt_slot, clue_slot;
	struct vrmt_t	*vrmt;
	struct clue_t	*clue;


	memset64(&vrmts[0], 0, sizeof(struct vrmt_t) * MAX_TARGETS);
	atomic_set(&vrmt_count, 0);
	
	for (vrmt_slot = 0; vrmt_slot < MAX_TARGETS; vrmt_slot ++){
		vrmt = &vrmts[vrmt_slot];
		vrmt->id = INVALID_TARGET;
		for(clue_slot = 0;  clue_slot < MAX_CLUES_PER_TARGET; clue_slot ++){
			clue = &vrmt->clue[clue_slot];
			clue->id = INVALID_CLUE;
		}
	}
}

int vrmt_load(const char *vrmt_file,
					int __attribute__((__unused__)) option)
{
	
	char 	readline[1024] = {0};
	int	clue_id, flags;
	int	vrmt_slot = 0, clue_slot = 0;
	int	first_invalid_vrmt_slot;
	
	FILE* 			fp;
	target_id		tid;
	struct clue_t	*clue;
	struct vrmt_t	*vrmt;

	fp = fopen(vrmt_file, "r");
	if(unlikely(!fp))  {
	    	rt_log_error(ERRNO_INVALID_ARGU, 
				"%s", strerror(errno));
		return -1;
	}

	vrmt_init();
	first_invalid_vrmt_slot	=	INVALID_TARGET;
	while(fgets(readline, 1024, fp)) {

		tid = clue_id = flags = 0;
		
		if(sscanf(readline, "%u %lu %d", &clue_id, &tid, &flags) == -1)
			continue;
				
		for (vrmt_slot = 0; vrmt_slot < MAX_TARGETS; vrmt_slot ++) {

			vrmt = &vrmts[vrmt_slot];
			/** Add a clue to current vrmt */
			if (vrmt->id == tid) {
				//printf("Find a vrmt_%d, slot=%d\n", vrmt->id, vrmt_slot);
				for(clue_slot = 0; clue_slot < MAX_CLUES_PER_TARGET; clue_slot++) {
					clue = &vrmt->clue[clue_slot];
					if(clue->id == INVALID_CLUE) {
						clue->id = clue_id;
						clue->flags = flags;
						vrmt->clues ++;
						break;
					}
				}
				break;
			}

			else{
				/** Find a first empty slot for current vrmt */
				if(((int64_t)vrmt->id == (int64_t)INVALID_TARGET)  && 
					(first_invalid_vrmt_slot == INVALID_TARGET)){
					first_invalid_vrmt_slot = vrmt_slot;
					//printf("add to an empty slot_%d\n", first_invalid_vrmt_slot);
				}
			}
			
		}

		if(first_invalid_vrmt_slot != INVALID_TARGET){
	              vrmt = &vrmts[first_invalid_vrmt_slot];
			if ((int64_t)vrmt->id == (int64_t)INVALID_TARGET) {
				vrmt->id = tid;
				for(clue_slot = 0; clue_slot < MAX_CLUES_PER_TARGET; clue_slot++) {
					clue = &vrmt->clue[clue_slot];
					if(clue->id == INVALID_CLUE) {
						clue->id = clue_id;
						clue->flags = flags;
						vrmt->clues ++;
						break;
					}
				}
				atomic_inc(&vrmt_count);
			}
			first_invalid_vrmt_slot	=	INVALID_TARGET;
		}
	}

	fclose(fp);
	
	rt_log_debug("total vrmts = %d", atomic_add(&vrmt_count, 0));
	return 0;
}

int vrmt_generate(const char *vrmt_file)
{

	target_id	tid;
	FILE 		*fp;
	int		clue_id, i = 0;
	
	fp = fopen(vrmt_file, "w+");
	if(unlikely(!fp)) {
	    	rt_log_error(ERRNO_INVALID_ARGU, 
				"%s", strerror(errno));
		return -1;
	}

	while(i ++ < MAX_CLUES_PER_TARGET){
		tid = random();
		clue_id = random();
		fprintf(fp, "%u %lu %d\n", clue_id % MAX_CLUES_PER_TARGET, tid % MAX_TARGETS, i);
	}

	fclose(fp);
	return 0;
}

void vrmt_dump(const char *vrmt_file,
					int __attribute__((__unused__)) option)
{

	int	vrmt_slot = 0, clue_slot = 0;
	FILE 	*fp = NULL;
	struct vrmt_t	*vrmt;
	struct clue_t	*clue;
	
	if(likely(vrmt_file)){
		fp = fopen(vrmt_file, "w+");
		if(unlikely(!fp)) {
		    	rt_log_error(ERRNO_INVALID_ARGU, 
					"%s", strerror(errno));
			return;
		}	
	}
	
	for (vrmt_slot = 0; vrmt_slot < MAX_TARGETS; vrmt_slot ++) {
		vrmt = &vrmts[vrmt_slot];
		
		if((int64_t)vrmt->id != (int64_t)INVALID_TARGET){
			for(clue_slot = 0; clue_slot < MAX_CLUES_PER_TARGET; clue_slot++) {
				clue = &vrmt->clue[clue_slot];
				if(clue->id != INVALID_CLUE) {
					if(likely(fp))
						fprintf(fp, "%d %lu %d\n", clue->id, vrmt->id, clue->flags);
				}
			}
		}
	}

	if(likely(fp))
		fclose(fp);
}

int vrmt_del(const target_id tid)
{

	int	xerror = (-ERRNO_NO_ELEMENT);
	int	slot = 0, clue_slot = 0;
	struct vrmt_t	*vrmt = NULL;
	struct clue_t	*clue;
	
	xerror = vrmt_lookup(tid, &slot, &vrmt);
	if(xerror)
		return xerror;
	
	rt_log_info("Delete vrmt_%lu\n", tid);
	/** INVALID all clue */
	for(clue_slot = 0;  clue_slot < MAX_CLUES_PER_TARGET; clue_slot ++){
		clue = &vrmt->clue[clue_slot];
		clue->id = INVALID_CLUE;
	}
	
	vrmt->id = INVALID_TARGET;
	atomic_dec(&vrmt_count);

	return 0;
}


void vrmt_test()
{
#if 0
	vrmt_generate("./vrmt.txt");
	vrmt_load("./vrmt.txt", 0);
#else
	vrmt_load("./vrmt.txt", 0);
	vrmt_del(2);
	vrmt_dump("./vrmt.tmp.txt", 0);
	vrmt_load("./vrmt.tmp.txt", 0);
#endif	
}

