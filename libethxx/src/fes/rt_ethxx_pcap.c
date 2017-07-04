#include "sysdefs.h"
#include "rt_ethxx_pcap.h"

struct pcap_file_header pcap_filehdr = {
    .magic = 0xa1b2c3d4,
    .version_major = PCAP_VERSION_MAJOR,
    .version_minor = PCAP_VERSION_MINOR,
    .linktype = 1,
    .thiszone = 0,
    .snaplen = 65535,
    .sigfigs = 0,
};

struct rt_ethxx_pcap_writer rte_writer = {
    .dispatcher_eq = ATOMIC_INIT(0),
    .dispatcher_dq = ATOMIC_INIT(0),
    .dispatcher_wr = ATOMIC_INIT(0),
    .reporter_eq = ATOMIC_INIT(0),
    .reporter_dq = ATOMIC_INIT(0),
};

#define FNAME_LENGTH  256

static __rt_always_inline__ void __attribute__((__unused__))
rt_ethxx_pcap_filehdr(struct pcap_file_header *filehdr,
    uint32_t __attribute__((__unused__))linktype, 
    int32_t __attribute__((__unused__))thiszone, 
    uint32_t __attribute__((__unused__))snaplen)
{
    filehdr->magic = 0xa1b2c3d4;
    filehdr->version_major = PCAP_VERSION_MAJOR;
    filehdr->version_minor = PCAP_VERSION_MINOR;
    filehdr->linktype = 1;
    filehdr->thiszone = 0;
    filehdr->snaplen = 65535;
    filehdr->sigfigs = 0;

}

static __rt_always_inline__ void __attribute__((__unused__))
rt_ethxx_pcap_pkthdr(struct pcap_pkthdr *pkthdr, 
                        uint64_t timestamp, 
                        uint32_t len)
{
    pkthdr->ts.tv_sec  = timestamp / 1000000000;
    pkthdr->ts.tv_usec = timestamp / 1000 % 1000000;
    pkthdr->caplen     = len;
    pkthdr->len        = len;
}

static __rt_always_inline__ void
rt_ethxx_pkthdr_convert(const struct pcap_pkthdr *hdr,
                        struct pcap_sf_pkthdr *sf_hdr)
{
    sf_hdr->ts.tv_sec    = hdr->ts.tv_sec;
    sf_hdr->ts.tv_usec  = hdr->ts.tv_usec;
    sf_hdr->caplen       = hdr->caplen;
    sf_hdr->len            =   hdr->len;
}

int rt_ethxx_pcap_flush(const char *fname, void *_pkthdr,
                        void *val, size_t __attribute__((__unused__))s)
{
    FILE *fp;
    struct pcap_sf_pkthdr sf_hdr;
    struct pcap_pkthdr *pkthdr = (struct pcap_pkthdr *)_pkthdr;
    
    fp = fopen(fname, "a+");
    if (likely (fp)) {
        rt_ethxx_pkthdr_convert(pkthdr, &sf_hdr);
        /* XXX we should check the return status */
        (void)fwrite((void *)&pcap_filehdr, sizeof(pcap_filehdr), 1, fp);
        (void)fwrite((void *)&sf_hdr, sizeof(struct pcap_sf_pkthdr), 1, fp);
        (void)fwrite((void *)val, pkthdr->caplen, 1, fp);
        fclose(fp);
        DISPATCH_WR_ADD(1);
        
        return XSUCCESS;
    }
    
    rt_log_error(ERRNO_PCAP_ERROR, "%s", strerror(errno));
    
    return 0;
}

void rt_ethxx_pcap_del_disk(const char *file,
                        int __attribute__((__unused__))flags)
{

    char rm[FNAME_LENGTH] = {0};
    
    SNPRINTF(rm, FNAME_LENGTH - 1, "rm %s", file);
    do_system(rm);
}
