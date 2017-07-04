#include "sysdefs.h"
#include "conf.h"
#include "conf_yaml_loader.h"
#include "vrs.h"
#include "vrs_session.h"
#include "vrs_senior.h"

static __rt_always_inline__ int ip_parser(char *y_ip, char *ip)
{
    memcpy (ip, y_ip, strlen (y_ip));
    return 0;
}

static __rt_always_inline__ int port_parser(char *strport, uint16_t *pt)
{
    *pt = tcp_udp_port_parse(strport);
    return (int)(*pt);
}

static __rt_always_inline__ int vpw_mkdir(const char *path)
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

static int load_cdr_conf(int reload)
{
    ConfNode *base = NULL, *child = NULL;
    int xret = 0;
    struct vpw_t *_this = vrs_default_trapper()->vpw;

    base = ConfGetNode("cdr");
    if (!base){
        goto finish;
    }
    TAILQ_FOREACH(child, &base->head, next){
        if(!STRCMP(child->name, "ip")){
            xret = ip_parser (child->val, _this->cdr.ip);
            if (xret < 0){
                rt_log_error(ERRNO_YAML, "Ip invalid\n");
                goto finish;
            }
        }
        if(!STRCMP(child->name, "port")){
            xret |= port_parser (child->val, &(_this->cdr.port));
            if (xret < 0){
                rt_log_error(ERRNO_DECODER_YAML_ARGU, "Port invalid\n");
                goto finish;
            }
        }
    }

    if (xret && reload){
        atomic_set(&(_this->cdr.flags), 1);
    }


finish:
    return xret;
}


static int load_private_log_conf ()
{
    ConfNode *base = NULL, *child = NULL;
    int xret = 0;
    struct vrs_trapper_t *trapper = vrs_default_trapper ();
    const char *log_dir = "/usr/local/etc/vpw/logs";

    base = ConfGetNode("log");
    if (!base){
        goto finish;
    }

    memset (trapper->log_dir, 0, strlen(trapper->log_dir));
    memcpy (trapper->log_dir, log_dir, strlen(log_dir));
    vpw_mkdir (log_dir);

    TAILQ_FOREACH(child, &base->head, next){
        if(!STRCMP(child->name, "level")) {
            memset (trapper->log_level, 0, strlen (trapper->log_level));
            memcpy (trapper->log_level, child->val, strlen (child->val));
        }
    }

    rt_logging_open (trapper->log_level, trapper->log_form, trapper->log_dir);

finish:
    return xret;
}


static int load_stage_time()
{
    int xret = -1, stage_time[3] = {0};
    char *value = NULL;
    struct vpw_t *_this = vrs_default_trapper()->vpw;

    if (ConfGet("stage-time", &value)){
        sscanf(value, "%d %d %d", &stage_time[0], &stage_time[1], &stage_time[2]);
        int i = 0;
        do{
            atomic_set(&_this ->stage_time[i], stage_time[i]);
            i++;
        }while(i < 3);
    }

    return xret;
}

static int load_vpw_default_configure()
{
    int xret = -1, value = 0;
    struct vpw_t *_this = vrs_default_trapper()->vpw;

    /** up_score */
    xret = ConfYamlReadInt("up-score", &value);
    if (!xret) {
        atomic_set(&_this->score_threshold, value);
        default_global_score = value;
     }

    value = 0;
    /** mul filt enable */
    xret = ConfYamlReadInt("mul-filt-enable", &value);
    if (!xret)
        atomic_set(&_this->clue_layer_filter, 1);

    value = 0;
    /** short voice enable */
    xret = ConfYamlReadInt("short-voice-enable", &value);
    if (!xret)
        atomic_set(&_this->short_voice_enable, value);

    /** stage time*/
    xret = load_stage_time();

    return xret;
}


int load_private_vpw_config (int reload)
{
    int xret = -1;

    struct vpw_t *_this = vrs_default_trapper()->vpw;

    yaml_init();
    /* vpw allocate file */
    if (ConfYamlLoadFile(VRS_VPW_CFG)){
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_VPW_CFG);
        goto finish;
    }

    /** vpw id*/
    xret = ConfYamlReadInt("id", (int *)&(_this->id));
    if (xret != 0){
        rt_log_error(ERRNO_INVALID_VAL, "Failed to read vpw id from the yaml file ");
    }

    load_private_log_conf ();

    /** stage_time*/
    load_vpw_default_configure();

    /** cdr ip*/
    load_cdr_conf (reload);

finish:
    return xret;
}

int load_public_vpw_conf ()
{
    ConfNode *base = NULL, *child = NULL;
    int xret = -1;
    int id = 0;
    struct vpw_t *_this = vrs_default_trapper()->vpw;

    base = ConfGetNode("vpw");
    if (!base) {
        goto finish;
    }

    TAILQ_FOREACH(child, &base->head, next){
        if(!STRCMP(child->val, "id")){
            ConfNode *subchild = NULL;
            TAILQ_FOREACH(subchild, &child->head, next) {
                if (!STRCMP(subchild->name, "id")){
              id = integer_parser(subchild->val, 0, 65535);
                    if (id < 0 || _this->id != (uint32_t)id) {
                        break;
                    }
                }

                if (!STRCMP(subchild->name, "dev")){
            memset (_this->netdev, 0, strlen (_this->netdev));
            memcpy (_this->netdev, subchild->val, strlen (subchild->val));
            xret = 0;
            goto finish;
                }
            }
        }
    }

finish:
    return xret;
}

int load_public_vpm_conf(int reload)
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
                    xret = ip_parser(subchild->val, _this->ip);
                    if (xret < 0){
                        rt_log_error(ERRNO_YAML, "Ip invalid");
                        goto finish;
                    }
                }
                if (!STRCMP(subchild->name, "port")){
                    xret |= port_parser(subchild->val, &(_this->port));
                    if (xret < 0){
                        rt_log_error(ERRNO_DECODER_YAML_ARGU, "Port invalid");
                        goto finish;
                    }
                }
            }

        }
    }
    if (xret && reload){
        atomic_set(&(_this->flags), 1);
    }

finish:
    return xret;
}

static int check_vpw_id(char *val, int id)
{
    char seps[] = " ";
    char *str = NULL;
    int xret = -1;

    str = strtok(val, seps);
    while(str != NULL){
        if (id == integer_parser(str, 0, 65535)){
            xret = 0;
            break;
        }
        str = strtok(NULL, seps);
    }

    return xret;
}

int load_public_module()
{
    int xret = 0;
    int id = 0;;
    ConfNode *base = NULL, *child = NULL;

    struct vpm_t *_this_vpm    =    vrs_default_trapper()->vpm;
    struct vpw_t *_this_vpw    =    vrs_default_trapper()->vpw;
    struct vdu_t *_this_vdu    =    vrs_default_trapper()->vdu;

    base = ConfGetNode("groups");
    if (!base){
        goto finish;
    }
    TAILQ_FOREACH(child, &base->head, next){
        if (!STRCMP(child->val, "id")){
            ConfNode *subchild = NULL;
            TAILQ_FOREACH(subchild, &child->head, next){
                if (!STRCMP(subchild->name, "id")){
                    id = integer_parser(subchild->val, 0, 65535);
                    if (id < 0){
                        rt_log_error(ERRNO_INVALID_VAL, "id interval invalid");
                        xret = -1;
                        goto finish;
                    }
                }
          if (!STRCMP(subchild->name, "vpw")){
                    xret = check_vpw_id(subchild->val, (int)_this_vpw->id);
                    if (xret < 0){
               _this_vdu->id = 0;
               _this_vpm->id = 0;
                        rt_log_error(ERRNO_NO_ELEMENT, "Vrs is not in the initial group");
               break;
                    }
                }
                if (!STRCMP(subchild->name, "vdu")){
                    id = integer_parser(subchild->val, 0, 65535);
                    if (id  > 0)
                        _this_vdu->id = id;
                }
                if (!STRCMP(subchild->name, "vpm")){
                    id = integer_parser(subchild->val, 0, 65535);
                    if (id > 0)
                        _this_vpm->id = id;
                }
            }
        }

        if (_this_vdu->id != 0 && _this_vpm->id != 0)
            break;
    }

finish:
    return xret;
}

static int load_public_vdu_path ()
{
    int xret = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper ();

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

static int load_public_sample_path ()
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

int load_public_vdu_conf(int reload)
{
    int xret = -1;
    int id = 0;
    ConfNode *base = NULL, *child = NULL;
    struct vdu_t *_this    =    vrs_default_trapper()->vdu;

    /** vdu path*/
    xret = load_public_vdu_path ();
    if (xret < 0){
        rt_log_error(ERRNO_FATAL, "Can not get vdu path configuration");
    }

    base = ConfGetNode("vdu");
    if (!base){
        goto finish;
    }

    TAILQ_FOREACH(child, &base->head, next){
        if (!STRCMP(child->val, "id")){
            ConfNode *subchild = NULL;
            TAILQ_FOREACH(subchild, &child->head, next){
                if (!STRCMP(subchild->name, "id")){
                    /**Support maximum vdu number */
                    id = integer_parser(subchild->val, 0, 65535);
                    if (id < 0 || _this->id != (uint32_t)id){
                        rt_log_error(ERRNO_INVALID_VAL, "id(%d) interval invalid", id);
                        break;
                    }
                    xret = 0;
                }
                if (!STRCMP(subchild->name, "ip")){
                    xret = ip_parser(subchild->val, _this->ip);
                    if (xret < 0){
                        rt_log_error(ERRNO_YAML, "Ip invalid");
                        goto finish;
                    }
                }
                if (!STRCMP(subchild->name, "port")){
                    xret |= port_parser(subchild->val, &(_this->port));
                    if (xret < 0){
                        rt_log_error(ERRNO_DECODER_YAML_ARGU, "Port invalid");
                        goto finish;
                    }
                }
            }
        }
    }

    if (xret && reload){
        atomic_set(&(_this->flags), 1);
    }

finish:
    return xret;
}

static int load_modellist(int reload)
{
    int xret = -1;
    struct vrs_trapper_t *rte = vrs_default_trapper ();

    ConfNode *base = ConfGetNode("dst-path");
    if (NULL == base){
        rt_log_error(ERRNO_FATAL, "Can not get module path configuration");
        goto finish;
    }


    if (likely (base->val)) {
        memset (rte->model_dir, 0, strlen (rte->model_dir));
        memcpy (rte->model_dir, base->val, strlen (base->val));
    }


    xret = rt_dir_exsit(rte->model_dir);
    if (!xret){
        xret = vpw_mkdir(rte->model_dir);
        if (xret < 0){
            /**no release model_path space  */
            rt_log_error(ERRNO_FATAL,
                    "Create %s (%s)", rte->model_dir, strerror(errno));
            goto finish;
        }
    }

    if (reload){
        vpw_modelist_load(rte, ML_FLG_RLD|ML_FLG_MOD_0);
        rt_log_notice ("Reloading Model Database from \"%s\" ... finished(%ld)",
                    rte->model_dir, rte->tool->sg_valid_models);
    }

finish:
    return xret;
}

int load_rule_and_model(int reload)
{
    int xret = -1;

    yaml_init();
    /* vrs allocate file */
    if (ConfYamlLoadFile(VRS_SYSTEM_BASIC_CFG)){
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_SYSTEM_BASIC_CFG);
        goto finish;
    }

    xret = vrmt_load(VRS_RULE_CFG, 0, vrs_default_trapper()->thr_path_name);
    if (xret < 0){
        goto finish;
    }

    xret = load_modellist(reload);
    if (xret < 0){
        goto finish;
    }

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



int load_public_config(int reload)
{
    yaml_init();
    /* vrs allocate file */
    if (ConfYamlLoadFile(VRS_SYSTEM_BASIC_CFG)){
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_SYSTEM_BASIC_CFG);
        goto finish;
    }

    load_public_module();
    load_public_vpm_conf(reload);
    load_public_vpw_conf ();
    load_public_sample_path ();
    load_public_vdu_conf(reload);
    load_thr_path_name();
    load_hit_second_conf();

finish:
    return 0;
}

void vpw_conf_init()
{
    /** Whether to reload  [1 yes,0 no]*/
    int reload = 0;

    /** load_private_vpw_config */
    load_private_vpw_config(reload);

    /** load vrs conf */
    load_public_config(reload);

    load_rule_and_model(reload);
}

