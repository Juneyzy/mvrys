#include "sysdefs.h"
#include "ams.h"

void * AmsGardiansClientTask (void *param)
{
	int	s;
	char	*test_data = "test_data";

	param = param;
	
	FOREVER {
		do {
			s = (rt_lsock_client (AMS_GARDIANS_SOCK));
			if (s > 0) {
				break;
			}
			rt_log_notice ("Connecting to (.sock=%s, sock=%d): %s",
						AMS_GARDIANS_SOCK, s, "failure");
			sleep (3);
		} while (s < 0);

		rt_log_notice ("Connecting to (.sock=%s, sock=%d): %s",
						AMS_GARDIANS_SOCK, s, "success");

		do {
			rt_sock_send (s, test_data, strlen (test_data));
			sleep (1);
		} while (s > 0);
	}

	task_deregistry_id(pthread_self ());

	return NULL;
}

void * AmsGardiansServerRoutine (void *param)
{
	int 	tmo;
	int	xerror;
	int 	sock = *(int *)param;
	char buffer[64]; 
	
	FOREVER {

		xerror = is_sock_ready (sock, 30 * 1000000, &tmo);
		if (xerror == 0 && tmo == 1)
			continue;
		if (xerror < 0) {
			rt_sock_close (&sock, NULL);
			break;
		}

		int len = rt_sock_recv (sock, buffer, 64);
		if (len > 0)
			printf ("test_data=%s\n", buffer);
	}

	task_deregistry_id(pthread_self ());

	return NULL;
	
}


void * AmsGardiansServerTask (void *param)
{
	int	s;
	int 	tmo;
	int	xerror;
	int 	sock;
	
	param = param;
	
	FOREVER {

		do {
			s = rt_lsock_server (AMS_GARDIANS_SOCK);
			if (s > 0) {
				break;
			}
			rt_log_notice ("Listen (.sock=%s, sock=%d), %s",
						AMS_GARDIANS_SOCK, s, "failure");
			sleep (3);
		} while (s < 0);
/**
		rt_log_notice ("Ready to Listen (.sock=%s, sock=%d)",
						AMS_GARDIANS_SOCK, s);
*/
		do {
			
			xerror = is_sock_ready (s, 30 * 1000000, &tmo);
			if (xerror == 0 && tmo == 1)
				continue;
			if (xerror < 0) {
				rt_sock_close (&s, NULL);
				break;
			}
			
			sock = rt_serv_accept (0, s);
			if (sock <= 0) {
				rt_log_error(ERRNO_SOCK_ACCEPT, "%s\n", strerror(errno));
				continue;
			}
			
			struct rt_task_t task;
			task.argvs = &sock;
			memcpy (task.name, "AMS Application Server Routine Task", strlen ("AMS Application Server Routine Task"));
			task.routine = AmsGardiansServerRoutine;

			task_spawn_quickly (&task);
			
		} while (s > 0);
	}

	task_deregistry_id(pthread_self ());

	return NULL;
	
}


static struct rt_task_t AMSGardiansCTask =
{
    .module = THIS,
    .name = "AMS Application Client Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = AmsGardiansClientTask,
};

static struct rt_task_t AMSGardiansSTask =
{
    .module = THIS,
    .name = "AMS Application Server Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = AmsGardiansServerTask,
};

void ams_lsock_test ()
{
	task_registry (&AMSGardiansSTask);
	task_registry (&AMSGardiansCTask);
}
