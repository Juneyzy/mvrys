#ifndef __VRS_OCI_H__
#define __VRS_OCI_H__

#define DO_CALL_FUNC(f) \
    do {\
        int xret = f; \
        if (xret != 0){ \
            rt_log_info("Function Return val error, ret = %d\n", xret); \
            return xret; \
        } \
    } while(0)

struct vrs_db_info {
    char *dbname;
    char *usrname;
    char *passwd;
};

struct hit_records_t
{
    struct hit_records_t *next;
    int      dir;
    uint64_t callid;
};

struct counter_upload_t
{
    int vpw_id;
    int matched_sum;
    int hitted_sum;
    char tm[64];
};

extern void *SGCDBWriter(void *param);

extern int vrs_insert_stat_info(struct counter_upload_t *_this);
extern int vrs_get_topx_records(int topx, int clue_id, int object_id, int *score_records);
extern int vrs_get_hit_records(int clue_id, int object_id, struct hit_records_t *records);
extern void vrs_oci_conn_tmr (uint32_t __attribute__((__unused__))uid, int __attribute__((__unused__))argc,
                char __attribute__((__unused__))**argv);

#endif //__VRS_OCI_H__
