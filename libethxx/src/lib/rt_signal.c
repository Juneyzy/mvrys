#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

int rt_signal_block(int signum)
{
    sigset_t x;
    if (sigemptyset(&x) < 0)
        return -1;
    if (sigaddset(&x, signum) < 0)
        return -1;
    if (sigprocmask(SIG_BLOCK, &x, NULL) < 0)
        return -1;

    return 0;
}

void rt_signal_handler_setup(int sig, void (*handler)())
{
#if defined (OS_WIN32)
	signal(sig, handler);
#else
    struct sigaction action;
    memset(&action, 0x00, sizeof(struct sigaction));

    action.sa_handler = handler;
    sigemptyset(&(action.sa_mask));
    sigaddset(&(action.sa_mask),sig);
    action.sa_flags = 0;
    sigaction(sig, &action, 0);
#endif /* OS_WIN32 */

    return;
}

int rt_signal_handler(int sig, void (*handler)())
{
    struct sigaction action;
    memset(&action, 0x00, sizeof(struct sigaction));

    sigaction(sig, NULL, &action);

    return (action.sa_handler == handler);
}


