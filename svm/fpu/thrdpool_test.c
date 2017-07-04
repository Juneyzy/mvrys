#include "sysdefs.h"

struct thpool_  *thrdpool;
int thrds = 4, wqe_size = 4096;
MQ_ID	queue;
const char *section_data = "hello, the crule world";
atomic64_t	enq = ATOMIC_INIT (0), deq = ATOMIC_INIT (0);

static void *MQSender (void *param)
{
	param = param;
	
	char *input;
	
	FOREVER {
		input = (char *) kmalloc (1024, MPF_CLR, -1);
		if (likely (input)) {
			memcpy64 (input, section_data, strlen (section_data));
			if (rt_mq_send (queue, input, strlen (input)) < 0) {
				kfree (input);
				continue;
			}
		}
		atomic64_inc (&enq);
		usleep (10);
	}

	task_deregistry_id (pthread_self());
	
	return NULL;
}

static inline void
___thrdpool_feed(void *pv_val)
{
	atomic64_inc (&deq);
	kfree (pv_val);
}

static void *MQReceiver (void *param)
{

	param = param;
	
	char *input = NULL;
	int	s;
	
	FOREVER {
		input = NULL;
		if (rt_mq_recv (queue, (void **)&input, &s) == MQ_SUCCESS) {
			threadpool_add (thrdpool,  (void *)___thrdpool_feed, (void *)input, 0);
		}
	}

	task_deregistry_id (pthread_self());
	
	return NULL;
}

static struct rt_task_t MQSenderTask =
{
    .module = THIS,
    .name = "NQ Sender Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = MQSender,
};

static struct rt_task_t MQReceiverTask =
{
    .module = THIS,
    .name = "NQ Receiver Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = MQReceiver,
};
void thrdpool_test()
{
	queue = rt_mq_create ("my test queue");

	assert((thrdpool = thpool_create(thrds, wqe_size, 0)) != NULL);

	fprintf(stderr, "Pool started with %d threads and "
		"queue size of %d\n", thrds, wqe_size);

	task_registry (&MQReceiverTask);
	task_registry (&MQSenderTask);
	
	task_run ();
	FOREVER {
		int64_t eq, dq;
		eq = atomic64_add (&enq, 0);
		dq = atomic64_add (&deq, 0);
		
		printf ("enq=%ld, deq=%ld (remain = %ld)\n", eq, dq, (eq - dq));
		sleep (3);
	}

}
