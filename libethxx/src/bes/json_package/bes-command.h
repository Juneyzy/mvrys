#ifndef __BES_COMMAND_H__
#define __BES_COMMAND_H__

#define UI_REQUEST  0
#define UI_RESPONSE 1

#define EXEC_SUCCESS    1
#define EXEC_FAILURE    0

#define RECEIVER_PROCESS_NAME   "Receiver"

#define UI_CMD_START            		  0x50
#define UI_CMD_STOP             		  0x51
#define UI_CMD_RESTART          		  0x52
#define UI_CMD_STATISTICS_RT_GET   		  0X70
#define UI_CMD_STATISTICS_PROTOCOL_GET    0X71
#define UI_CMD_STATISTICS_MESSAGE_GET     0X72
#define UI_CMD_STATISTICS_SUMMARY_GET     0X73
#define UI_CMD_STATISTICS_EXCEPTION_GET   0X74
#define UI_CMD_IP5TUPLE_GET     		  0x75
/** set*/
#define UI_CMD_RULE               0x76

#define UI_CMDTYPE_PROCESS_NAME         0x0001
#define UI_CDMTYPE_EXEC_RESULT          0x1001
#define UI_CMDTYPE_FAIL_REASON          0x1003
#define UI_CMDTYPE_RECV_PKTS            0x2001
#define UI_CMDTYPE_SEND_PKTS            0x2002
#define UI_CMDTYPE_LOST_PKTS            0x2003
#define UI_CMDTYPE_DROP_PKTS_TOTAL      0x2004
#define UI_CMDTYPE_DROP_PKTS_ICMP       0x2005
#define UI_CMDTYPE_DROP_PKTS_TTLZERO    0x2006
#define UI_CMDTYPE_DROP_PKTS_LAYER2     0x2007
#define UI_CMDTYPE_DROP_PKTS_ERROR      0x2008

#define PROTOCOL_VERSION 1


#endif
