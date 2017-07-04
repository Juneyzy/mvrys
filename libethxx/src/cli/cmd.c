#include "sysdefs.h"
#include "zserv.h"
#include "vty.h"
#include "command.h"
#include "rt_task.h"
#include "cmd_probe.h"
#include "cmd_cluster.h"


DEFUN(loglevel_set, loglevel_set_cmd,
      "loglevel (all|receiver|probe|sock|proto|pcap|msgqueue|ma|ui|yaml|requestor|responsor|test) (enable|disable) [LEVEL]",
      "loglevel\n"
      "Log等级设置\n"
      "All modules\n"
      "所有模块\n"
      "Receiver module\n"
      "Receiver模块\n"
      "Probe module\n"
      "Probe模块\n"
      "Sock module\n"
      "Sock模块\n"
      "Proto module\n"
      "Proto模块\n"
      "Pcap module\n"
      "Pcap模块\n"
      "Msgqueue module\n"
      "Msgqueue模块\n"
      "Ma module\n"
      "Ma模块\n"
      "Ui module\n"
      "Ui模块\n"
      "Yaml module\n"
      "Yaml模块\n"
      "Requestor module\n"
      "Requestor模块\n"
      "Responsor module\n"
      "Responsor模块\n"
      "Test module\n"
      "Test模块\n"
      "Log enable\n"
      "Log使能\n"
      "Log disable\n"
      "Log禁用\n"
      "Log level {EMERG(0),ALERT(1),CRIT(2),ERROR(3),WARNING(4),NOTICE(5),INFO(6),DEBUG(7)}\n"
      "Log等级 {EMERG(0),ALERT(1),CRIT(2),ERROR(3),WARNING(4),NOTICE(5),INFO(6),DEBUG(7)}\n")
{
    self = self;
#if 0
    int mod = 0;
    int en = 0;
    int level = 0;

    if (!STRNCMP (argv[1], "enable", 1))
    {
        en = 1;
        if (argc == 3)
        {
            level = atoi (argv[2]);
            if ((level < 0) || (level >= __MAX_LEVEL))
            {
                vty_out (vty, "Warning: Invalid level (range 0-7)%s", VTY_NEWLINE);
                return CMD_WARNING;
            }
        }
        else
        {
            level = __WARNING;
        }
    }
    else if (!STRNCMP (argv[1], "disable", 1))
    {
        en = DISABLE;
    }
    
    if (!STRNCMP (argv[0], "all", 3))
    {
        for (mod = 0; mod < __MAX_MOD; mod ++)
        {
            logLevelSet (mod, en, level);
        }
    }
    else
    {
        mod = modStr2Int (argv[0]);
        if (mod >= __MAX_MOD)
        {
            vty_out (vty, "Err: Unmatched log module %s %s", argv[0], VTY_NEWLINE);
            return CMD_WARNING;
        }
        logLevelSet (mod, en, level);
    }
#endif
    return CMD_SUCCESS;
}

#define LOGLEVEL_SHOW_PROMPT_FORMAT     "  %-10s%-10s%-10s\r\n"
#define LOGLEVEL_SHOW_DATA_FORMAT       "  %-10s%-10s%-10s\r\n"
#define LOGLEVEL_SHOW_PROMPT()\
do{\
    vty_out (vty, "\r\n");\
    vty_out (vty, LOGLEVEL_SHOW_PROMPT_FORMAT, "MODULES", "STATUS", "LOGLEVEL"); \
    vty_out (vty, LOGLEVEL_SHOW_PROMPT_FORMAT, "-------", "------", "--------"); \
}while(0)

DEFUN(loglevel_show, loglevel_show_cmd,
      "show loglevel",
      SHOW_STR
      SHOW_CSTR
      "loglevel\n"
      "Log等级\n")
{
    self = self;
    argc = argc;
    argv = argv;
#if 0
    int mod = 0;

    LOGLEVEL_SHOW_PROMPT ();

    for (mod = 0; mod < __MAX_MOD; mod ++)
    {
        if(1 == logStatus(mod))
        {
            vty_out (vty, LOGLEVEL_SHOW_DATA_FORMAT, 
                modInt2Str (mod), logstatusInt2Str (logStatus(mod)), loglevelInt2Str (logLevel(mod)));
        }
        else
        {
            vty_out (vty, LOGLEVEL_SHOW_DATA_FORMAT, modInt2Str (mod), logstatusInt2Str (logStatus(mod)), "");
        }
    }

    vty_out (vty, "\r\n");
#endif    
    return CMD_SUCCESS;
}


void receiver_cmdline_initialize(void)
{
    install_element (CONFIG_NODE, &loglevel_set_cmd);
    install_element (CONFIG_NODE, &loglevel_show_cmd);
}

extern char kill_all_vty;
struct thread *daemon_thread, *global_thread;
static unsigned short int alternative_port = 0;

static int cmdline_run (void)
{
    while (1)
    {
        char *vty_addr = NULL;
        short vty_port = ZEBRA_VTY_PORT;
        struct thread thread;

        /* Make vty server socket. */
        if (alternative_port != 0)
        {
            vty_port = alternative_port;
        }

        vty_serv_sock (vty_addr, vty_port, ZEBRA_VTYSH_PATH);
        global_thread = &thread;

        while (thread_fetch (cmdline_interface.master, &thread))
        {
            thread_call (&thread);

            if (kill_all_vty)
                vty_kill_all();
        }
    }

    /* Not reached... */
    exit (0);
}
void *cmdline_task (void *argv)
{
    argv = argv;

    /*Shield sinal of sigalrm*/
    rt_signal_block(SIGALRM);
    //rt_log_debug ("\tCommand Line Interface Initialization ...\n");
    /** Make master thread emulator. */
    cmdline_interface.master = thread_master_create();
    //rt_log_debug ("\tMaster thread okay !\n");
    /** Initialize command line interface running enviroment . */
    cmdline_runtime_env_initialize();
    /** Config access ticket */
    vty_config_password();
    //rt_log_debug ("\tInternal configuration initialize okay !\n");
    /** Initialize common command . */
    common_cmd_initialize();
    //rt_log_debug ("\tCommon command initialize okay !\n");
    /** Vty related initialize. */
    vty_init (cmdline_interface.master);
    //rt_log_debug ("\tVty and vty-related command initialize okay !\n");

    receiver_cmdline_initialize ();
    probe_cmdline_initialize ();
    cluster_cmdline_initialize();


    cmdline_run ();

    return NULL;
}

static struct rt_task_t spasr_cmdline_task =
{
    .module = THIS,
    .name = "Command Line Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = cmdline_task,  /** Front end system request task */
};

void cmdline_init()
{
    task_registry(&spasr_cmdline_task);
}

