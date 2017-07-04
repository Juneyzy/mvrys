#include <sys/utsname.h>
#include "sysdefs.h"
#include "capture.h"
#include "vrs_session.h"
#include "vpw_dms_agent.h"
#include "vpw_init.h"
#include "vrs_senior.h"

static  int wqe_set[MAX_INWORK_CORES] = {0};

extern int librecv_init(int __attribute__((__unused__)) argc,
    char __attribute__((__unused__))**argv);

static void
__rt_ethxx_proc(void *_p,
                void __attribute__((__unused__)) *resv)
{

    //int    wqe = *(int *)resv;

    if (likely (_p)){

        SGProc (_p);
    }
}

static void *
rt_ethxx_proc_routine(void __attribute__((__unused__))*argvs)
{
    int    wqe = *(int *)argvs;

    FOREVER{
        rt_ethxx_proc(NULL, (void *)&wqe, __rt_ethxx_proc, (void *)&wqe);
        usleep(10);
    }

    task_deregistry_id(pthread_self());
    return NULL;
}

static __rt_always_inline__ void vpw_init_task ()
{
    int    i;
    struct rt_task_t    *task;
    struct rt_vrstool_t *tool = vrs_default_trapper()->tool;

    for (i =0; i < tool->cur_tasks; i ++) {
        if (tool->cur_tasks <= tool->allowded_max_tasks ) {
            task    =    (struct rt_task_t    *) kmalloc (sizeof (struct rt_task_t), MPF_CLR, -1);
            if (likely (task)) {
                sprintf (task->name, "SG Proc%d Task", i);
                wqe_set[i] = i;
                task->module = THIS;
                task->core = INVALID_CORE;
                task->prio = KERNEL_SCHED;
                task->argvs = (void *)&wqe_set[i];
                task->recycle = ALLOWED;
                task->routine = rt_ethxx_proc_routine;

                task_registry (task);
            }
        }
    }
}

static __rt_always_inline__ void vpw_check_env (struct vrs_trapper_t *trapper)
{
    if(likely(trapper)) {
        /** vpw */
        struct vpw_t *vpw;
        vpw = trapper->vpw;
        rt_log_notice ("*** Basic Configuration of VPW (%d)", vpw->id);
        rt_log_notice ("        Score Threshold: %d", atomic_add(&vpw->score_threshold, 0));
        rt_log_notice ("        Filter of Clue Layer: %d", atomic_add(&vpw->clue_layer_filter, 0));
        rt_log_notice ("        Stage: (%d:%d:%d)", atomic_add(&vpw->stage_time[0], 0), atomic_add(&vpw->stage_time[1], 0), atomic_add(&vpw->stage_time[2], 0));
        rt_log_notice ("        Short Voice Matcher: %d", atomic_read(&vpw->short_voice_enable));
        rt_log_notice ("        Log Directory: %s", trapper->log_dir);

        rt_log_notice ("*** Basic Configuration of Hit-Second");
        rt_log_notice ("        Hit-Second-en: %d", trapper->hit_scd_conf.hit_second_en);
        rt_log_notice ("        Update-Score: %d", trapper->hit_scd_conf.update_score);
        rt_log_notice ("        Hit-Score: %d", trapper->hit_scd_conf.hit_score);
        rt_log_notice ("        Topn-Max: %d", trapper->hit_scd_conf.topn_max);
        rt_log_notice ("        Topn-Min: %d", trapper->hit_scd_conf.topn_min);
        rt_log_notice ("        TOPN-DIR: %s", trapper->hit_scd_conf.topn_dir);

        rt_log_notice ("*** Basic Configuration of Pools ");
        rt_log_notice ("        Session Pool: %d", rt_pool_bucket_number (trapper->cs_bucket_pool));
        rt_log_notice ("        Match Pool: %d", rt_pool_bucket_number (trapper->cm_bucket_pool));
        rt_log_notice ("        CDR Pool: %d", rt_pool_bucket_number (trapper->cdr_bucket_pool));

        rt_log_notice ("*** Basic Configuration of CDR");
        rt_log_notice ("        CDR: (%s:%d)", vpw->cdr.ip, vpw->cdr.port);


        struct vpm_t    *vpm;
        vpm = trapper->vpm;
        rt_log_notice ("*** Basic Configuration of VPM ");
        rt_log_notice ("        VPM: (%d, %s:%d)", vpm->id, vpm->ip, vpm->port);

        struct vdu_t    *vdu;
        vdu = trapper->vdu;
        rt_log_notice ("*** Basic Configuration of VDU");
        rt_log_notice ("        VDU: (%d, %s:%d)", vdu->id, vdu->ip, vdu->port);

        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Sample", trapper->sample_dir, rt_dir_exsit (trapper->sample_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Model", trapper->model_dir, rt_dir_exsit (trapper->model_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Temp", trapper->tmp_dir, rt_dir_exsit (trapper->tmp_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "VDU", trapper->vdu_dir, rt_dir_exsit (trapper->vdu_dir) ? "ready" : "not ready");

        senior_check_nfs_mount();
    }
}

static void vpw_argv_parser(char **argv, struct vpw_t *vpw)
{
    int i;

    for (i = 0; argv[i] != NULL; i++){
        /** Serial number parser */
        if (!STRCMP (argv[i], "--sn")){
            if (argv[++i] != NULL) {
                sscanf(argv[i], "%d", &vpw->serial_num);
                continue;
            }
            rt_log_error(ERRNO_INVALID_VAL, "Serial number is not set");
            goto finish;
        }
        /** decoder configuration parser */
        if (!STRCMP(argv[i], "--config")){
            if (argv[++i] != NULL){
                continue;
            }
        rt_log_error(ERRNO_INVALID_VAL, "Vpm configuration is not set");
            goto finish;
        }
    }
finish:
    return;
}

static __rt_always_inline__ int __sg_check_and_mkdir(const char *root)
{
    char cmd[256] = {0};
    if(!rt_dir_exsit(root)) {
        sprintf(cmd, "mkdir -p %s", root);
        do_system(cmd);
        return 1;
    }
    return 0;
}

#if defined(HAVE_PF_RING)
static void vpw_pfring_module_init ()
{
#define PF_RING_KMOUDEL       "/usr/local/etc/vpw/pf_ring.ko"
/**
    0  Linux standards for a packet processing flow
    1  Processing both go Linux standard package, also copy to pf_ring
    2  Driver will only copy to pf_ring, the kernel is not to receive these packages
*/
#define PFRING_TRANSPARENT_MODE 1

    struct utsname uts;
    char computer[256] = {0}, pfring_dir[256] = {0};
    char cmd[256] = {0};

    if (gethostname (computer, 255) != 0 || uname(&uts) < 0){
        rt_log_notice ("Unable to get the host information.");
        return ;
    }

    /*Create pf_ring path*/
    SNPRINTF (pfring_dir, 255, "/lib/modules/%s/kernel/net/pf_ring", uts.release);
    if(rt_dir_exsit(pfring_dir)) {
        sprintf(cmd, "modprobe -r %s", "pf_ring");
        do_system(cmd);
    }else{
        sprintf(cmd, "mkdir -p %s", pfring_dir);
        do_system(cmd);
    }

    memset(cmd, 0, sizeof(cmd));
    SNPRINTF (cmd, 255, "cp -rf %s %s", PF_RING_KMOUDEL, pfring_dir);
    do_system (cmd);

    /**
        synchronous /sbin/depmod
    */
    memset(cmd, 0, sizeof(cmd));
    SNPRINTF (cmd, 255, "/sbin/depmod %s/%s", pfring_dir, "pf_ring.ko");
    /*
        insmod pf_ring.ko
        transparent_mode = 1
    */
    memset(cmd, 0, sizeof(cmd));
    SNPRINTF (cmd, 255, "modprobe %s transparent_mode=%d", "pf_ring", PFRING_TRANSPARENT_MODE);
    do_system (cmd);
    rt_log_notice("Loaded PF_RING: \"%s\"", cmd);
}
#endif

void vpw_init(int __attribute__((__unused__)) argc, char **argv)
{
    struct vrs_trapper_t *trapper = vrs_default_trapper();

#if defined(HAVE_PF_RING)
    /**insmod pf_ring */
    vpw_pfring_module_init ();
#endif

    /** read config before do trapper init */
    vpw_conf_init ();

    vpw_argv_parser(argv, trapper->vpw);

    /** trapper init */
    vpw_trapper_init (trapper);
}

void vpw_startup ()
{
    struct vrs_trapper_t *trapper = vrs_default_trapper();

    tmr_start (trapper->perform_tmr);
    tmr_start (trapper->statistics_tmr);

    vpw_check_env (trapper);

    trapper->tool->engine_init ("SpkSRE.cfg", "/usr/local/etc/vpw");
}

/*主函数*/
int main (int argc, char **argv)
{
    librecv_init (argc, argv);

    vpw_init (argc, argv);

    vpw_init_task ();

    vpw_dms_init (argc, argv);

    vpw_startup ();

    /** 本地TOPN加载到内存 */
    if (vrs_default_trapper()->hit_scd_conf.hit_second_en) {
        senior_topn_table_init();
        senior_load_topns(vrs_default_trapper()->hit_scd_conf.topn_dir);
        tmr_start (vrs_default_trapper()->topn_disc_tmr);
    }

    task_run();

    FOREVER{
        sleep(1000);
    }

    ConfYamlDestroy();

    return 0;
}
