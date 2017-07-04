#ifndef __RT_CAPTURE_H__
#define __RT_CAPTURE_H__

/** Added by tsihang, 2016-1-19 */
#include "rt_ethxx_trapper.h"
#include "rt_ethxx_capture.h"
#include "rt_ethxx_pcap.h"
#include "rt_ethxx_reporter.h"

#ifndef RT_ETHXX_CAPTURE_ADVANCED
#include "rt_mq.h"
int spasr_pkt_mq_recv(MQ_ID qid, message *msg, int *msg_size);

int spasr_pkt_mq_send(MQ_ID qid, message msg, int msg_size);

extern MQ_ID captor_qid;

void pcap_starup();

void rt_captor_status_set (int status);
#endif  /** end of RT_ETHXX_CAPTURE_ADVANCED */

#endif

