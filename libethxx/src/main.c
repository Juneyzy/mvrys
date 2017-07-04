#include "sysdefs.h"

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
#include "rt.h"
#endif

#include "ui.h"
#include "conf.h"
#include "command-line-interface.h"
#include "cluster_decoder.h"
#include "capture.h"

#define default_cfg   "default_template_cfg.yaml"

struct user_args_desc{
    int serial_num;
    char *basic_cfg;
    char *default_dir;
    char *default_format;
    RTLogLevel default_level;
};
struct user_args_desc args_desc;

int libIndex = 0;
typedef void (*argv_parser)(char **argv, struct user_args_desc *aa);

static void log_config_console(ConfNode *base, RTLogInitData *sc_lid)
{
    ConfNode *output_node = NULL;
    char *enable = NULL, *format = NULL;
    RTLogLevel level = RT_LOG_NOTSET;

    TAILQ_FOREACH(output_node, &base->head, next){
        if(!STRCMP(output_node->name, "enable")){
            enable = output_node->val;
        }
        if(!STRCMP(output_node->name, "level")){
            level = rt_logging_parse_level(output_node->val);
        }
        if(!STRCMP(output_node->name, "format")){
            format = output_node->val;
        }
    }

    if(level == RT_LOG_NOTSET){
        level = args_desc.default_level;
    }
    if(!format){
        format = args_desc.default_format;
    }

    if (!STRCMP(enable, "yes")){
        rt_logging_init_console(format, level, sc_lid);
    }
}

static void log_config_file(ConfNode *base, RTLogInitData *sc_lid)
{
    ConfNode *output_node = NULL;
    char *enable = NULL, *filename = NULL, *format = NULL;
    RTLogLevel level = RT_LOG_NOTSET;

    TAILQ_FOREACH(output_node, &base->head, next){
        if(!STRCMP(output_node->name, "enable")){
            enable = output_node->val;
        }
        #if 0
        if(!STRCMP(output_node->name, "filename")){
            filename = output_node->val;
        }
        #else

        char file[256] = {0};
        rt_logging_mkfile(file, 255);
        filename = &file[0];
        #endif
        if(!STRCMP(output_node->name, "level")){
            level = rt_logging_parse_level(output_node->val);
        }
        if(!STRCMP(output_node->name, "format")){
            format = output_node->val;
        }
    }

    if(level == RT_LOG_NOTSET){
        level = args_desc.default_level;
    }
    if(!format){
        format = args_desc.default_format;
    }

    if (!STRCMP(enable, "yes")){
        rt_logging_init_file(filename, args_desc.default_dir, format, level, sc_lid);
    }
}

static void log_config_syslog(ConfNode *base, RTLogInitData *sc_lid)
{
    ConfNode *output_node = NULL;
    char *enable = NULL, *facility = NULL, *format = NULL;

    TAILQ_FOREACH(output_node, &base->head, next){
        if(!STRCMP(output_node->name, "enable")){
            enable = output_node->val;
        }
        if(!STRCMP(output_node->name, "facility")){
            facility = output_node->val;
        }
        if(!STRCMP(output_node->name, "format")){
            format = output_node->val;
        }
    }

    if (!STRCMP(enable, "yes")){
        rt_logging_init_syslog(facility, format, args_desc.default_level, sc_lid);
    }

}

static void log_config_output(ConfNode *base, RTLogInitData *sc_lid)
{
    ConfNode* output_node = NULL;

    TAILQ_FOREACH(output_node, &base->head, next){
        if(!STRCMP(output_node->name, "console")){
            log_config_console(output_node, sc_lid);
        }

        if(!STRCMP(output_node->name, "file")){
            log_config_file(output_node, sc_lid);
        }

        if(!STRCMP(output_node->name, "syslog")){
            log_config_syslog(output_node, sc_lid);
        }
    }
}

static inline void log_init()
{
    ConfNode *master_node = NULL;
    RTLogInitData *sc_lid = NULL;

    sc_lid = rt_logging_sclid_init();
    if (!sc_lid )
        goto finish;

    ConfNode *base = ConfGetNode("logging");
    if (!base){
        rt_log_error(ERRNO_FATAL, "Can not get logging configuration\n");
        goto finish;
    }

    TAILQ_FOREACH(master_node, &base->head, next){
        if(!STRCMP(master_node->name, "default-log-dir")){
            args_desc.default_dir = master_node->val;
        }
        if(!STRCMP(master_node->name, "default-log-format")){
            args_desc.default_format = master_node->val;
        }
        if(!STRCMP(master_node->name, "default-log-level")){
            args_desc.default_level = rt_logging_parse_level(master_node->val);
        }

        if(!STRCMP(master_node->name, "outputs")){
            ConfNode *output_node = NULL;
            TAILQ_FOREACH(output_node, &master_node->head, next){
                log_config_output(output_node, sc_lid);
            }
        }
    }

    if (sc_lid->startup_message == '\0'){
        free(sc_lid);
        sc_lid = NULL;
    }
finish:
    rt_logging_init(sc_lid);
}

static void
decoder_argv_parser(char **argv, struct user_args_desc *desc)
{
    int i;

    assert (desc);
    for (i = 0; argv[i] != NULL; i++){
		/** Serial number parser */
        if (!STRCMP (argv[i], "--sn")){
            if (argv[++i] != NULL) {
                sscanf(argv[i], "%d", &desc->serial_num);
                continue;
            }
            printf("ERROR, Serial number is not set\n");
            goto finish;
        }

		/** decoder configuration parser */
        if (!STRCMP(argv[i], "--config")){
            if (argv[++i] != NULL){
                desc->basic_cfg = strdup(argv[i]);
                continue;
            }
            printf("ERROR, Decoder configuration is not set\n");
            goto finish;
        }
    }

finish:
	return;
}


void
decoder_yaml_load(char **argv,
    argv_parser parser)
{
#if SPASR_BRANCH_EQUAL(BRANCH_VRS)
	argv = argv;
	parser = parser;
	ConfYamlLoadFromFile(NULL);
#else
	char conf[128] = {0};

    if (parser)
        parser(argv, &args_desc);

    if (NULL == args_desc.basic_cfg)
        SNPRINTF(conf, sizeof(conf), "conf/default_template_cfg");
    else
        SNPRINTF(conf, sizeof(conf), "%s", args_desc.basic_cfg);

	ConfYamlLoadFromFile(conf);
#endif
    return ;
}
static int signal4pipe;

/** Do not write a socket which has been disconnected */
static void signal_pipe_handler()
{
    signal4pipe++;
}

static void signal_int_handler()
{
    //rt_ethernet_uninit(); /** @BUG */
      exit(0);
}

int librecv_init(int __attribute__((__unused__)) argc,
    char __attribute__((__unused__))**argv)
{
    decoder_yaml_load(argv, decoder_argv_parser);

    rt_system_preview();


   #if !SPASR_BRANCH_EQUAL (BRANCH_VRS)
   log_init();
   #endif

    /** To avoid */
    //rt_signal_handler_setup(SIGPIPE, SIG_IGN);
    rt_signal_handler_setup(SIGPIPE, signal_pipe_handler);
    rt_signal_handler_setup(SIGSYS, SIG_IGN);
    rt_signal_handler_setup(SIGINT, signal_int_handler);

    /* Ma should initialize as early as possible, because of sguard's state monitoring */
    //ui_init();

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
  //  rt_init();
   // decoder_cluster_init();
#endif

   //cmdline_init ();

   //rt_ethernet_init();

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
    task_run();
#endif

    return 0;
}

struct rt_object_t {
    int i;
    struct list_head list;
};

void list_test(void)
{
    static LIST_HEAD(olist);
    int i = 0;

    for (i = 0; i < 5; i ++){
        struct rt_object_t *o = malloc(sizeof(struct rt_object_t));
        o->i = i;
        list_add_tail(&o->list, &olist);
    }

    struct rt_object_t *this, *p;
    list_for_each_entry_safe(this, p, &olist, list){
        list_del(&this->list);
        break;
    }

    struct rt_object_t *l;
    list_for_each_entry(l, &olist, list){
        printf("i=%d\n", l->i);
    }
}

int rt_stack_test(void)
{
    char *ptr[10] = { "hello","how", "are", "you", "?", "Fine", "Thanks", "you"};
    int i = 0;
    char * pop;
    struct stack_t *new_stack = NULL;

    new_stack = rt_stack_new(0, kfree);

    for(i = 0; i < 6; i++)
        rt_stack_push(new_stack, ptr[i]);

    printf ("\n");

    for(i = 0; i < 6; i++){
        pop = rt_stack_pop(new_stack);
        printf("i = %d  sp = %d  size = %d, %s\n", i, (int)new_stack->sp, (int)new_stack ->size, pop);
    }

    rt_stack_delete(new_stack);

    return 0;
}

extern void tmr_test0();
extern void ceph_test();

#if (SPASR_BRANCH_EQUAL(BRANCH_LOCAL))
int main(int __attribute__((__unused__)) argc,
    char __attribute__((__unused__))**argv)
{

#define one_day 86400
    //rt_stack_test();
    //rt_thrdpool_test();
    //list_test();
    //rt_stack_test();
    //ceph_test();
    //tmr_test0();

    librecv_init(argc, argv);

    sleep(1);
    FOREVER{
        sleep(one_day);
        //printf("Good Morning!\n");
    }
    /** Should never reach here */
    ConfYamlDestroy();

    return 0;
}
#endif

