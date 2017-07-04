#include "sysdefs.h"
#include "conf.h"
#include "conf_yaml_loader.h"
#include "vrs.h"
#include "vrs_senior.h"

static int load_vrsweb_conf ()
{
    ConfNode *base = NULL, *subchild = NULL;
    int xret = 0;
    char * ip = NULL;
    uint16_t port = 0;
    struct vpm_t *_this = vrs_default_trapper()->vpm;

    base = ConfGetNode("vrsweb");
    if (!base){
        goto finish;
    }


    TAILQ_FOREACH(subchild, &base->head, next){

        if(!STRCMP(subchild->name, "ip"))
            ip  = subchild->val;

        if(!STRCMP(subchild->name, "port"))
            port = integer_parser(subchild->val, 0, 65535);
    }

    if (port != _this->web_port ||
        strncmp (ip, _this->web, strlen (ip))) {
        memset (_this->web, 0, strlen (_this->web));
        memcpy (_this->web, ip, strlen(ip));
        _this->web_port = port;
        vpm_flags_set_bit (VPM_FLGS_RSYNC, 1);
    }

finish:
    return xret;
}


static __rt_always_inline__ int vpm_mkdir (const char *path)
{
    int xret = -1;

    if (!path)
        goto finish;

    char *temp = strdup(path);
    char *pos = temp;

    if (STRNCMP(temp, "/", 1) == 0) {
        pos += 1;
    } else if (STRNCMP(temp, "./", 2) == 0) {
        pos += 2;
    }

    for ( ; *pos != '\0'; ++ pos) {
        if (*pos == '/') {
            *pos = '\0';
            xret = mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            *pos = '/';
        }
    }
    if (*(pos - 1) != '/') {
        xret = mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    free(temp);

finish:
    return xret;
}


static int load_private_log_conf()
{

    const char *log_dir = "/usr/local/etc/vpm/logs";
    ConfNode *base = NULL, *subchild = NULL;
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper ();

    base = ConfGetNode("log");
    if (!base){
        goto finish;
    }

    memset (rte->log_dir, 0, strlen(rte->log_dir));
    memcpy (rte->log_dir, log_dir, strlen(log_dir));
    vpm_mkdir (log_dir);

    TAILQ_FOREACH(subchild, &base->head, next){
        if(!STRCMP(subchild->name, "level")) {
            memset (rte->log_level, 0, strlen (rte->log_level));
               memcpy (rte->log_level, subchild->val, strlen (subchild->val));
            }
    }

    rt_logging_open (rte->log_level, rte->log_form, rte->log_dir);

finish:
    return xret;
}

static int load_sample_path()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    ConfNode *base = ConfGetNode("wave-path");
    if (NULL == base){
        xret = -1;
        goto finish;
    }

    if (likely (base->val)) {
        memset (rte->sample_dir, 0, strlen (rte->sample_dir));
        memcpy (rte->sample_dir, base->val, strlen (base->val));
    }

finish:
    return xret;
}

static int load_massive_path()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    ConfNode *base = ConfGetNode("mass-path");
    if (NULL == base){
        xret = -1;
        goto finish;
    }

    if (likely (base->val)) {
        memset (rte->mass_dir, 0, strlen (rte->mass_dir));
        memcpy (rte->mass_dir, base->val, strlen (base->val));
    }

finish:
    return xret;

}

static int load_category_path()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    ConfNode *base = ConfGetNode("comp-path");
    if (NULL == base){
        xret = -1;
        goto finish;
    }

    if (likely (base->val)) {
        memset (rte->category_dir, 0, strlen (rte->category_dir));
        memcpy (rte->category_dir, base->val, strlen (base->val));
    }

finish:
    return xret;
}

static int load_model_path()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    ConfNode *base = ConfGetNode("dst-path");

    if (NULL == base){
        xret = -1;
        goto finish;
    }

    if (likely (base->val)) {
        memset (rte->model_dir, 0, strlen (rte->model_dir));
        memcpy (rte->model_dir, base->val, strlen (base->val));
    }

finish:
    return xret;
}

static int load_vdu_path()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    ConfNode *base = ConfGetNode("vdu-path");

    if (NULL == base){
    xret = -1;
    goto finish;
    }

    if (likely (base->val)) {
        memset (rte->vdu_dir, 0, strlen (rte->vdu_dir));
        memcpy (rte->vdu_dir, base->val, strlen (base->val));
    }

finish:
    return xret;
}

static int load_targetmatch_path()
{
    int xret = -1;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    ConfNode *base = ConfGetNode("target-path");

    if (NULL == base) {
        rt_log_error(ERRNO_FATAL, "target-path not find.");
        goto finish;
    }
    if (likely(base->val)) {
        xret = 0;
        memset(rte->targetmatch_dir, 0, sizeof(rte->targetmatch_dir));
        memcpy(rte->targetmatch_dir, base->val, strlen(base->val));
    } else {
        rt_log_error(ERRNO_FATAL, "targetmatch-path not find.");
    }

finish:
    return xret;
}


int load_vpm_ip_port()
{
    ConfNode *base = NULL, *child = NULL;
    int xret = -1;
    int id = 0;
    struct vpm_t *_this = vrs_default_trapper()->vpm;

    base = ConfGetNode("vpm");
    if (!base) {
        goto finish;
    }
    TAILQ_FOREACH(child, &base->head, next){
        if(!STRCMP(child->val, "id")){
            ConfNode *subchild = NULL;
            TAILQ_FOREACH(subchild, &child->head, next){
                if (!STRCMP(subchild->name, "id")){
                    id = integer_parser(subchild->val, 0, 65535);
                    if (id < 0 || _this->id != (uint32_t)id){
                        rt_log_error(ERRNO_INVALID_VAL, "id interval invalid");
                        break;
                    }
                    xret = 0;
                }
                if (!STRCMP(subchild->name, "ip")){

            memset (_this->ip, 0, strlen (_this->ip));
            memcpy (_this->ip, subchild->val, strlen(subchild->val));
                }
                if (!STRCMP(subchild->name, "port")){
             _this->port = integer_parser(subchild->val, 0, 65535);
                }
            }

        }
    }

finish:
    return xret;
}

int load_boost_switch()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    ConfNode *base = ConfGetNode("boost-sw");
    if (NULL == base){
        xret = -1;
        goto finish;
    }

    if (likely (base->val)) {
        rte->vrs_boost_st = integer_parser(base->val, 0, 1);
    }

finish:
    return xret;
}

int load_private_vpm_config(int reload)
{
    reload = reload;
     struct vpm_t *_this = vrs_default_trapper()->vpm;

    int xret = -1;

    yaml_init();
    /* vpm allocate file */
    if (ConfYamlLoadFile(VRS_VPM_CFG)) {
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_VPW_CFG);
        goto finish;
    }

    /** vpm id*/
    xret = ConfYamlReadInt("id", (int *)&(_this->id));
    if (xret != 0){
        rt_log_error(ERRNO_INVALID_VAL, "Failed to read vpw id from the yaml file ");
    }

    load_private_log_conf ();
    load_vrsweb_conf();
finish:
    return xret;
}

/****************************************************************************
 函数名称  : load_hit_second_conf
 函数功能    : 更新二次命中配置信息
 输入参数    : 无
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      : VPM VPW均会调用
****************************************************************************/
static int load_hit_second_conf(void)
{
    ConfNode *base = NULL, *child = NULL;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    int xret;

    base = ConfGetNode("topn-alg");
    if (!base) {
        xret = -1;
        goto finish;
    }

    TAILQ_FOREACH(child, &base->head, next) {
        if (!STRCMP(child->name, "switch"))
               rte->hit_scd_conf.hit_second_en = integer_parser(child->val, 0, 1);
        if (!STRCMP(child->name, "dir")) {
            memset(rte->hit_scd_conf.topn_dir, 0, sizeof(rte->hit_scd_conf.topn_dir));
            strncpy(rte->hit_scd_conf.topn_dir, child->val, strlen(child->val));
        }
        if (!STRCMP(child->name, "update-threshold")) {
            rte->hit_scd_conf.update_score = integer_parser(child->val, 0, 100);
        }
        if (!STRCMP(child->name, "hit-threshold"))
            rte->hit_scd_conf.hit_score = integer_parser(child->val, 0, 100);
        if (!STRCMP(child->name, "topn-max"))
        {
            rte->hit_scd_conf.topn_max = integer_parser(child->val, 0, 100);
        }
        if (!STRCMP(child->name, "topn-min"))
        {
            rte->hit_scd_conf.topn_min = integer_parser(child->val, 0, 100);
        }
    }

finish:
       return xret;
}

int load_dlna_conf()
{
    ConfNode *base = NULL, *child = NULL;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    int xret = -1;

    base = ConfGetNode("reserved");
    if (!base) {
        goto finish;
    }

    TAILQ_FOREACH(child, &base->head, next) {
        if (!STRCMP(child->name, "reserved1"))
            rte->topx_switch = integer_parser(child->val, 0, 1);

        if (!STRCMP(child->name, "reserved2")) {
            xret = integer_parser(child->val, 2, 10000);
            if (xret < 0)
            {
                rt_log_error(ERRNO_INVALID_VAL, "reserved2 interval invalid");
                goto finish;
            }
            rte->topx_num = xret;
        }
    }

finish:
    return xret;
}

int load_public_config(int reload)
{
    reload = reload;

    yaml_init();
    /* vrs allocate file */
    if (ConfYamlLoadFile(VRS_SYSTEM_BASIC_CFG)){
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_SYSTEM_BASIC_CFG);
        goto finish;
    }

    load_sample_path ();
    load_massive_path ();
    load_category_path ();
    load_model_path ();
    load_vdu_path ();
    load_thr_path_name(); /* 加载阈值配置路径名 */
    load_targetmatch_path ();
    load_vpm_ip_port();
    load_boost_switch();
    load_hit_second_conf();
    load_dlna_conf();

finish:
    return 0;
}

void vpm_conf_init()
{
    /** Whether to reload  [1 yes,0 no]*/
    int reload = 0;

    /** load vpm.yaml */
    load_private_vpm_config(reload);

    /** load vrs.yaml */
    load_public_config(reload);
}

