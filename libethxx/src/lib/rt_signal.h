#ifndef __RT_SIGNAL_H__
#define __RT_SIGNAL_H__

int rt_signal_block(int signum);

void rt_signal_handler_setup(int sig, void (*handler)());

int rt_signal_handler(int sig, void (*handler)());

#endif

