#include "sysdefs.h"
#include "rt_ethxx_trapper.h"

void  *rt_ethxx_open(const char *interface)
{
    pcap_t *p = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];
    
    p = pcap_open_live(interface, 65535, 1, 500, errbuf);
    if(unlikely(!p)){
        rt_log_info (
                    "%s", errbuf);
     }
         
    return p;
}

int rt_ethxx_init(struct rt_ethxx_trapper *rte,
                        const char *dev,
                        int up_check)
{

    pcap_t *p = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];
    struct bpf_program bpf;
    bpf_u_int32 ip, mask;
    int xret = 0;
    char filter[300] = "ether[12]=0x80 and ether[13]=0x52";

    if (unlikely(!dev)){
        rt_log_error (ERRNO_PCAP_ERROR,
            "\"%s\" invalid netdev", dev);
        goto finish;
    }

    if (unlikely(!rte)){
        rt_log_error (ERRNO_PCAP_ERROR,
            "Ethxx trapper not alloced");
        goto finish;
    }

    /** search the ethernet device */
    xret = pcap_lookupnet(dev, &ip, &mask, errbuf);
    if (up_check &&
        likely(xret < 0)){
        rt_log_info (
                    "%s", errbuf);
        //goto error;
    }

    p = (pcap_t *)rt_ethxx_open(dev);
    if (unlikely(!p)){
        xret = -1;
        goto error;
    }

    xret = pcap_compile(p, &bpf, &filter[0], 1, mask);
    if (likely(xret < 0)){
        rt_log_error (ERRNO_PCAP,
            "pcap do compiling");
        goto error;
    }

    xret = pcap_setfilter(p, &bpf);
    if (likely(xret < 0)){
        rt_log_error (ERRNO_PCAP,
            "pcap setfilter");
        goto error;
    }

    rte->ipv4 = ip;
    rte->mask4 = mask;
    rte->p = p;

    nic_address(rte->netdev,
        NULL, 63,
        NULL, 63,
        &rte->mac[0], 6);

    goto finish;

error:
finish:
    return xret;
}

void rt_ethxx_deinit(struct rt_ethxx_trapper *rte)
{
    if (likely(rte)){
        if (likely(rte->p))
            pcap_close(rte->p);
        goto finish;
    }

    rt_log_error (ERRNO_PCAP_ERROR,
            "Ethxx trapper not alloced");
    
finish:
    return;
}


