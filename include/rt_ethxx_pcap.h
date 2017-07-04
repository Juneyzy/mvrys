#ifndef __RT_ETHXX_PCAP_H__
#define __RT_ETHXX_PCAP_H__

#include <stdio.h>
#include <stdint.h>
#include <pcap.h>

#ifndef __HAVA_PCAP_INIT_H__
struct pcap_timeval {
    int32_t tv_sec;		/* seconds */
    int32_t tv_usec;		/* microseconds */
};

struct pcap_sf_pkthdr {
    struct pcap_timeval ts;	/* time stamp */
    uint32_t caplen;		/* length of portion present */
    uint32_t len;		/* length this packet (off wire) */
};
#endif

#define __HAVE_TIME_H__
#ifndef __HAVE_TIME_H__
struct timeval{
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif

#define __HAVE_PCAP_H__
#ifndef __HAVE_PCAP_H__
/** pcap.h */
struct pcap_pkthdr{
    struct timeval ts;
    bpf_u_int32 caplen;                        /* length of portion present */
    bpf_u_int32 len;
};

struct pcap_file_header {
    bpf_u_int32 magic;        
    u_short version_major;        
    u_short version_minor;       
    bpf_int32 thiszone;     /* gmt to local correction */        
    bpf_u_int32 sigfigs;    /* accuracy of timestamps */        
    bpf_u_int32 snaplen;    /* max length saved portion of each pkt */        
    bpf_u_int32 linktype;   /* data link type (LINKTYPE_*) */
};
#endif

struct rt_ethxx_pcap_writer{
    atomic64_t dispatcher_eq, dispatcher_dq;
    atomic64_t dispatcher_wr;
    atomic64_t reporter_eq, reporter_dq;
    
};

extern struct rt_ethxx_pcap_writer rte_writer;
extern struct pcap_file_header pcap_filehdr;

#define DISPATCH_WR_ADD(n) atomic64_add(&rte_writer.dispatcher_wr, n);
#define DISPATCH_EQ_ADD(n)  atomic64_add(&rte_writer.dispatcher_eq, n);
#define DISPATCH_DQ_ADD(n)  atomic64_add(&rte_writer.dispatcher_dq, n);
#define REPORTER_EQ_ADD(n)  atomic64_add(&rte_writer.reporter_eq, n);
#define REPORTER_DQ_ADD(n)  atomic64_add(&rte_writer.reporter_dq, n);

extern int rt_ethxx_pcap_flush(const char *fname, void *_pkthdr,
                        void *val, size_t __attribute__((__unused__))s);

extern void rt_ethxx_pcap_del_disk(const char *file,
                        int __attribute__((__unused__))flags);

#endif

