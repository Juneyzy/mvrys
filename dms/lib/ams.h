#ifndef __AMS_H__
#define __AMS_H__

#define	AMS_GARDIANS_SOCK	"/usr/local/etc/ams/gardians.sock"
#define	AMS_MGR_AGENT_SOCK	"/usr/local/etc/ams/ma.sock"

#define	AMS_CHECK
#define	AMS_VERSION	(0x01)
#define	AMS_REQUEST	(0x01)
#define	AMS_RESPONSE	(0x02)

struct ams_pkthdr_t {

	unsigned int version	: 8;
	unsigned int pkttype	: 8;
	unsigned int cmd		: 16;

	unsigned int sequence	: 16;
	unsigned int resv		: 16;

	unsigned int pktlen	: 16;
	unsigned int params	: 16;
};

struct ams_tlv_t {
	unsigned int		type: 16;
	unsigned int		length: 16;
	unsigned char		value[];
};

enum ams_type_t {
    TypeServInvalid,
    TypeServI8,
    TypeServI16,
    TypeServI32,
    TypeServI64,
    TypeServU8,
    TypeServU16,
    TypeServU32,
    TypeServU64,
    TypeServDouble,
    TypeServString
} ;

#define AMSHDR_SIZE		((int)sizeof (struct ams_pkthdr_t))


static __rt_always_inline__ int 
ams_pkthdr_chk (unsigned char *packet)
{
#if defined(AMS_CHECK)
	struct ams_pkthdr_t *__amshdr = (struct ams_pkthdr_t *) packet;
	if (__amshdr == NULL || __amshdr->version != AMS_VERSION ||
	        ((__amshdr->pkttype != AMS_REQUEST) && (__amshdr->pkttype != AMS_RESPONSE)))
		return -1;
#else
	packet = packet;
#endif
	return 0;
}

static __rt_always_inline__ int 
ams_param_num(unsigned char *packet)
{
	struct ams_pkthdr_t *__amshdr = (struct ams_pkthdr_t *) packet;
	return NTOHS(__amshdr->params);
}

static __rt_always_inline__ int 
ams_unpack_head (unsigned char *hdrstr, int hdrstr_size, 
						struct ams_pkthdr_t *amshdr)
{
	struct ams_pkthdr_t *__amshdr = (struct ams_pkthdr_t *) hdrstr;
	
	if(hdrstr_size < AMSHDR_SIZE ||
		(ams_pkthdr_chk (hdrstr) < 0))
		return -1;
	
	amshdr->version		=	__amshdr->version;
	amshdr->pkttype		=	__amshdr->pkttype;
	amshdr->cmd		=	NTOHS (__amshdr->cmd);
	amshdr->sequence	=	NTOHS (__amshdr->sequence);
	amshdr->pktlen		=	NTOHS (__amshdr->pktlen);
	amshdr->params		=	NTOHS (__amshdr->params);

	return 0;
}

static __rt_always_inline__ void 
ams_unpack_param (unsigned char *response, int *offset, unsigned char *result)
{
	struct ams_tlv_t *__tlv = (struct ams_tlv_t *)(response + *offset);
	uint64_t val64 = 0;
	uint32_t val32 = 0;
	uint16_t val16 = 0;
	uint8_t *val08 = (uint8_t *)&__tlv->value[0];

	switch (NTOHS(__tlv->type))
	{
		case TypeServI64:
		case TypeServU64:
		    val64 = NTOHLL(*(uint64_t *)val08);
		    memcpy(result, &val64, NTOHS (__tlv->length) - 4);
		    break;
		case TypeServI32:
		case TypeServU32:
		    val32 = NTOHL(*(uint32_t *)val08);
		    memcpy(result, &val32, NTOHS (__tlv->length) - 4);
		    break;
		case TypeServI16:
		case TypeServU16:
		    val16 = NTOHS(*((uint16_t *)val08));
		    memcpy(result, &val16, NTOHS (__tlv->length) - 4);
		    break;    
		default :
		    memcpy(result, __tlv->value, NTOHS (__tlv->length) - 4);
		    break;
	}

	*offset += NTOHS (__tlv->length);
}




#endif

