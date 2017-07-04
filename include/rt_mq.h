
#ifndef __RT_MQ_H__
#define __RT_MQ_H__

#define MSG_QUEUE_DEFAULT_WAITTIME	1000
/** If need waiting forever for a message */
#define MSG_QUEUE_WAITFOREVER 1

typedef unsigned long MQ_ID;
typedef void  *message;

typedef enum
{
    MQ_TIMEDOUT =   (-3),
    MQ_ID_INVALID = (-2),
    MQ_FAILURE = (-1),
    MQ_SUCCESS = (0)	
} mq_errno;


mq_errno rt_fifo_push (MQ_ID qid, message msg, int s);
mq_errno rt_fifo_pop (MQ_ID qid, message *msg, int *s);
MQ_ID rt_fifo_create (const char *desc);
int rt_fifo_destroy (MQ_ID qid);


#define rt_mq_create(desc) rt_fifo_create(desc)
#define rt_mq_send(qid,msg,s) rt_fifo_push(qid,msg,s)
#define rt_mq_recv(qid,msg,s) rt_fifo_pop(qid,msg,s)

#endif

