#include "sysdefs.h"

static volatile sig_atomic_t sigflag = 0;

static void SignalHandlerSigusr1 (__attribute__((unused))int signo) {
    sigflag = 1;
}



/**
 * \brief Tell the parent process the child is ready
 *
 * \param pid pid of the parent process to signal
 */
static void TellWaitingParent (pid_t pid) {
    kill(pid, SIGUSR1);
}

/**
 * \brief Set the parent on stand-by until the child is ready
 *
 * \param pid pid of the child process to wait
 */
static void WaitForChild (pid_t pid) {
    int status;
    rt_log_notice ("Daemon: Parent waiting for child to be ready...\n");
    /* Wait until child signals is ready */
    while (sigflag == 0) {
        if (waitpid(pid, &status, WNOHANG)) {
            /* Check if the child is still there, otherwise the parent should exit */
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                rt_log_notice ("Child died unexpectedly\n");
                exit(EXIT_FAILURE);
            }
        }
        /* sigsuspend(); */
        sleep(1);
    }
}
/**
 * \brief Close stdin, stdout, stderr.Redirect logging info to syslog
 *
 */
static void SetupLogging (void) {
    /* Redirect stdin, stdout, stderr to /dev/null  */
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
        return;
    if (dup2(fd, 0) < 0)
        return;
    if (dup2(fd, 1) < 0)
        return;
    if (dup2(fd, 2) < 0)
        return;
    close(fd);
}

/**
 * \brief Daemonize the process
 *
 */
void daemonize (void) {
    pid_t pid, sid;

    signal(SIGUSR1, SignalHandlerSigusr1);
    /** \todo We should check if wie allow more than 1 instance
              to run simultaneously. Maybe change the behaviour
              through conf file */

    /* Creates a new process */
    pid = fork();

    if (pid < 0) {
        /* Fork error */
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        /* Child continues here */
        umask(027);

        sid = setsid();
        if (sid < 0) {
            exit(EXIT_FAILURE);
        }

        if (chdir("/") < 0) {
	            rt_log_notice ("Error changing to working directory '/'");
        }

        SetupLogging();

        /* Child is ready, tell its parent */
        TellWaitingParent(getppid());

        /* Daemon is up and running */
        rt_log_notice("Daemon is running\n");
        return;
    }
    /* Parent continues here, waiting for child to be ready */
    rt_log_notice("Parent is waiting for child to be ready\n");
    WaitForChild(pid);

    /* Parent exits */
    rt_log_notice("Child is ready, parent exiting\n");
    exit(EXIT_SUCCESS);
}