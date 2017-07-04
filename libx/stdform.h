#ifndef __STDFORM_H__
#define __STDFORM_H__

#define	SAMPLE_RATE		8000

#define	riff_str_hex		0x46464952
#define	wav_str_hex	0x45564157
#define	fmt_str_hex		0x20746d66
#define	fact_str_hex		0x74636166
#define	data_str_hex	0x61746164

#define	FMTAG_PCM			0x0001			/** Microsoft Corperation */
#define	FMTAG_ADPCM		0x0002			/** Microsoft Corperation */
#define	FMTAG_ALAW		0x0006			/** Microsoft Corperation */
#define	FMTAG_ULAW		0x0007			/** Microsoft Corperation */
#define	FMTAG_OKI_ADPCM	0x0010			/** OKI */
#define	FMTAG_G723_ADPCM	0x0014			/** Antex */
#define	FMTAG_GSM610		0x0031			/** Microsoft Corperation */
#define	FMTAG_ANTEX_ADPCM	0x0033			/** Antex */
#define	FMTAG_G721_ADPCM		0x0040			/** Antex */
#define	FMTAG_MPEG			0x0050			/** Microsoft Corperation */
#define	FMTAG_CELP		0x0070
#define	FMTAG_SBC		0x0071
  
#pragma pack(push, 1)  

struct riff_head_t {
	uint32_t chunk_id;						// 00H 4 char "RIFF" symbol  
	uint32_t chunk_size;					// 04H 4 long int, chunk_size = (FileTotalSize - 8), except chunk_id + chunk_size
	uint32_t form_type;					// 08H 4 char "WAVE" symbol
};

/** http://www.cnblogs.com/shibuliao/p/3809353.html */
/** http://www.cnblogs.com/jqyp/archive/2012/08/16/2641910.html */

/** G.711 PCM: 8K 16bit, total 44 bytes  */
struct  pcm_head_t {  
	uint32_t chunk_id;						// 00H 4 char "RIFF" symbol  
	uint32_t chunk_size;					// 04H 4 long int, chunk_size = (FileTotalSize - 8)
	uint32_t form_type;					// 08H 4 char "WAVE" symbol
	uint32_t fmt;							// 0CH 4 char "fmt " symbol, last word is space=" "
	uint32_t filtered_bytes;					// 10H 4 int 0x10000000H(PCM), 0x12000000H(ALAW), filtering bytes (not fixed)  
	uint16_t fmt_tag;						// 14H 2 int 0x0001H(PCM), 0x0006H(A-LAW), 0x0007(U-LAW)
	uint16_t channels;						// 16H 2 int Channels, 1(Mono, sigal channel), 2(Stereo, dual, channels)
	uint32_t sample_rate;					// 18H 4 int Sample Rate (Samples per Second, Play speed of each Channel),
	uint32_t byte_rate;						// 1CH 4 int Byte Rate (val=Channels×sample_rate×bits_per_sample/8)
	uint16_t block_align;					// 20H 2 int Align factor (val=Channels×bits_per_sample/8)  
	uint16_t bits_per_sample;				// 22H 2 Bits per Sample, Same for each channel. 8, 16 bits
	uint32_t data_tag;						// 24H 4 char Data Symbol (val="fact" and options in head, and val="data" in data block)  
	uint32_t data_len;						// 28H 4 long int Voice Data Lenght (val=4 in head, and val=(FileTotalSize - 44) in data block)  
};  


/** Total 58 bytes, (SBC, CELP and so on) */
struct xlaw_head_t {
	uint32_t chunk_id;						// 00H 4 char "RIFF" symbol  
	uint32_t chunk_size;					// 04H 4 long int, chunk_size = (FileTotalSize - 8)
	uint32_t form_type;					// 08H 4 char "WAVE" symbol
	uint32_t fmt;							// 0CH 4 char "fmt " symbol
	uint32_t filtered_bytes;					// 10H 4 int 0x10000000H(PCM), 0x12000000H(ALAW), filtering bytes (not fixed)  
	uint16_t fmt_tag;						// 14H 2 int 0x0001H(PCM), 0x0006H(A-LAW), 0x0007(U-LAW)
	uint16_t channels;						// 16H 2 int Channels, 1(Mono, sigal channel), 2(Stereo, dual, channels)
	uint32_t sample_rate;					// 18H 4 int Sample Rate (Samples per Second, Play speed of each Channel),  
	uint32_t byte_rate;						// 1CH 4 int Byte Rate (val=Channels×sample_rate×bits_per_sample/8)
	uint16_t block_align;					// 20H 2 int Align factor (val=Channels×bits_per_sample/8)  
	uint32_t bits_per_sample;				// 22H 4 Bits per Sample, Same for each channel. 
	uint32_t fact;							// 26H 4 char "fact" Symbol
	uint32_t t1;							// AH 4 int 0x04000000H  
	uint32_t t2;							// EH 4 int 0x00530700H
	uint32_t data_tag;						// 24H 4 char Data Symbol (val="fact" and options in head, and val="data" in data block)  
	uint32_t data_len;						// 28H 4 long int Voice Data Lenght (val=4 in head, and val=(FileTotalSize - 58) in data block)  
};

typedef struct xlaw_head_t sbc_head_t, celp_head_t;

#pragma pack(pop)  

extern struct pcm_head_t	default_pcm_head;
extern struct xlaw_head_t	default_alaw_head;
extern struct xlaw_head_t	default_ulaw_head;
extern struct xlaw_head_t	default_sbc_head;
extern struct xlaw_head_t	default_celp_head;


extern void audio_head (const char *desc, void *xhead);
extern void alaw_head_init (struct xlaw_head_t *ahead, size_t s, int sample_rate, int channels, int quant_bits);
extern void pcm_head_init (struct pcm_head_t *ahead, size_t s, int sample_rate, int channels, int quant_bits);
	
#endif
