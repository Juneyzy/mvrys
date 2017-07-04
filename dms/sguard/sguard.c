#include "sysdefs.h"

extern int librecv_init(int __attribute__((__unused__)) argc,
    char __attribute__((__unused__))**argv);

struct resource_threshold_t {
	int	cpu_threshold;
	int	mem_threshold;
	int	disk_threshold;
};

struct guardian_t {
	uint16_t	local_port;
	struct resource_threshold_t	threshold;
	
};


struct guardian_t	DMSGuardian = {
	.local_port	=	2000,
	.threshold	=	{80, 80, 80},
};

extern void apputils_test ();
extern void daemonize (void);
int main ()
{
	librecv_init (0, NULL);

	rt_logging_open ("debug", "[%i] %t - (%f:%l) <%d> (%n) -- ", "./logs/");

	apputils_test ();

	task_run ();

	daemonize ();
	
	FOREVER {
		sleep (1000);
	}
	
	return 0;
}

