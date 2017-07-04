#include "sysdefs.h"
#include "stdform.h"

struct pcm_head_t	default_pcm_head = {
	.chunk_id = riff_str_hex,	/** R I F F */
	.chunk_size = 0,			/** FiltTotalSize - 8 */
	.form_type = wav_str_hex,	/** W A V E */
	.fmt = fmt_str_hex,		/** F M T  */
	.filtered_bytes = 0x10,
	.fmt_tag = FMTAG_PCM,
	.channels = 0x2,
	.sample_rate = 0xac44,		/** 0xac44 = 44100 */
	.byte_rate = 0x2b110,		/** 0x2b110 = 176400 */
	.block_align = 0x4,			/** bits_per_sample * channels / 8 */
	.bits_per_sample = 16,
	.data_tag = data_str_hex,	/** D A T A */
	.data_len = 0,				/** FileTotalSize - 44 */
	
};

struct xlaw_head_t	default_alaw_head = {
	.chunk_id = riff_str_hex,	/** R I F F */
	.chunk_size = 0,			/** FiltTotalSize - 8 */
	.form_type = wav_str_hex,	/** W A V E */
	.fmt = fmt_str_hex,		/** F M T  */
	.filtered_bytes = 0x12,		/** Fixed */
	.fmt_tag = FMTAG_ALAW,
	.channels = 0x1,
	.sample_rate = 0x3E80,		/** 0xac44=44100, 0x3E80=16000,  */
	.byte_rate = 0x3E80,		/** 0x2b110=176400 */
	.block_align = 0x1,			/** Fixed */
	.bits_per_sample = 8,		/** Fixed, 8 bits */
	.fact = fact_str_hex,		/** F A C T */
	.t1 = 0x00000004,
	.t2 = 0x00075300,
	.data_tag = data_str_hex,	/** D A T A */
	.data_len = 0,				/** FileTotalSize - 58 */

};

struct xlaw_head_t	default_ulaw_head = {
	.chunk_id = riff_str_hex,	/** R I F F */
	.chunk_size = 0,			/** FiltTotalSize - 8 */
	.form_type = wav_str_hex,	/** W A V E */
	.fmt = fmt_str_hex,		/** F M T  */
	.filtered_bytes = 0x12,		/** Fixed */
	.fmt_tag = FMTAG_ULAW,
	.channels = 0x1,
	.sample_rate = 0x3E80,		/** 0xac44=44100, 0x3E80=16000,  */
	.byte_rate = 0x3E80,		/** 0x2b110=176400 */
	.block_align = 0x1,			/** Fixed */
	.bits_per_sample = 8,		/** Fixed, 8 bits */
	.fact = fact_str_hex,		/** F A C T */
	.t1 = 0x00000004,			/** Fixed */
	.t2 = 0x00075300,			/** Fixed */
	.data_tag = data_str_hex,	/** D A T A */
	.data_len = 0,				/** FileTotalSize - 58 */

};

struct xlaw_head_t	default_sbc_head = {
	.chunk_id = riff_str_hex,	/** R I F F */
	.chunk_size = 0,			/** FiltTotalSize - 8 */
	.form_type = wav_str_hex,	/** W A V E */
	.fmt = fmt_str_hex,		/** F M T  */
	.filtered_bytes = 0x12,		/** Fixed */
	.fmt_tag = FMTAG_SBC,
	.channels = 0x1,
	.sample_rate = 0x3E80,		/** 0xac44=44100, 0x3E80=16000,  */
	.byte_rate = 0x3E80,		/** 0x2b110=176400 */
	.block_align = 0x25,		/** Fixed */
	.bits_per_sample = 16,		/** Fixed, 16 bits */
	.fact = fact_str_hex,		/** F A C T */
	.t1 = 0x00000004,
	.t2 = 0x00042876,
	.data_tag = data_str_hex,	/** D A T A */
	.data_len = 0,				/** FileTotalSize - 58 */

};

struct xlaw_head_t	default_celp_head = {
	.chunk_id = riff_str_hex,	/** R I F F */
	.chunk_size = 0,			/** FiltTotalSize - 8 */
	.form_type = wav_str_hex,	/** W A V E */
	.fmt = fmt_str_hex,		/** F M T  */
	.filtered_bytes = 0x12,		/** Fixed */
	.fmt_tag = FMTAG_CELP,
	.channels = 0x1,
	.sample_rate = 0x3E80,		/** 0xac44=44100, 0x3E80=16000,  */
	.byte_rate = 0x3E80,		/** 0x2b110=176400 */
	.block_align = 0x0C,		/** Fixed */
	.bits_per_sample = 16,		/** Fixed, 16 bits */
	.fact = fact_str_hex,		/** F A C T */
	.t1 = 0x00000004,
	.t2 = 0x00075260, 
	.data_tag = data_str_hex,	/** D A T A */
	.data_len = 0,				/** FileTotalSize - 58 */

};

static inline char *fmt_str(int fmt)
{
	switch (fmt) {
		case 1:
			return "PCM";
		case 6:
			return "A-LAW";
		case 7:
			return "U-LAW";
		default:
			return "Unknown";
	}
}

void audio_head (const char *desc, void *xhead)
{
	char	*chunk, *form_type, *fmt, *fact, *tag;
	struct pcm_head_t *ahead = (struct pcm_head_t *)xhead;
	struct xlaw_head_t *xlawhdr;
	
	chunk = (char*)&ahead->chunk_id;
	form_type = (char *)&ahead->form_type;
	fmt = (char *)&ahead->fmt;
	tag = (char *)&ahead->data_tag;

	if (desc)
		printf ("****************** %s ***************\n", desc);

	printf ("		Chunk ID = %08x(\"%c%c%c%c\")\n",  ahead->chunk_id, chunk[0], chunk[1], chunk[2], chunk[3]);
	printf ("		Chunk Size = %d (%f MB)\n", ahead->chunk_size, (double)ahead->chunk_size / (1024*1024));
	printf ("		Form Type = %08x(\"%c%c%c%c\")\n", ahead->form_type, form_type[0], form_type[1], form_type[2], form_type[3]);
	printf ("		FMT = %08x(\"%c%c%c%c\")\n",  ahead->fmt, fmt[0], fmt[1], fmt[2], fmt[3]);
	printf ("		Filter Bytes = %04x\n", ahead->filtered_bytes);
	printf ("		Audio Form = %d(%s)\n", ahead->fmt_tag, fmt_str(ahead->fmt_tag));
	printf ("		Channels = %d\n", ahead->channels);
	printf ("		Sample Rate = %d\n", ahead->sample_rate);
	printf ("		Bits per Sample = %d\n", ahead->bits_per_sample);
	printf ("		Byte Rate = %d\n", ahead->byte_rate);
	printf ("		Block Align = %d\n", ahead->block_align);
	
	if (ahead->fmt_tag == FMTAG_ALAW ||
		ahead->fmt_tag == FMTAG_ULAW) {
		xlawhdr = (struct xlaw_head_t *)xhead;
		fact = (char *)&xlawhdr->fact;
		printf ("		Fact = %08x(\"%c%c%c%c\")\n", xlawhdr->fact, fact[0], fact[1], fact[2], fact[3]);
		printf ("		t1 = %d(%08x)\n", xlawhdr->t1, xlawhdr->t1);
		printf ("		t2 = %d(%08x)\n", xlawhdr->t2, xlawhdr->t2);
	}
	
	printf ("		Data Tag = %08x(\"%c%c%c%c\")\n", ahead->data_tag, tag[0], tag[1], tag[2], tag[3]);
	printf ("		Data Len = %d\n", ahead->data_len);
	
}

void alaw_head_init (struct xlaw_head_t *ahead, size_t s, int sample_rate, int channels, int quant_bits)
{
	memcpy (ahead, &default_alaw_head, sizeof (default_alaw_head));
	ahead->chunk_size	=	s + 50;
	ahead->channels		=	channels;
	ahead->sample_rate	=	sample_rate;
	ahead->bits_per_sample	=	quant_bits;
	ahead->block_align		=	(channels * quant_bits/8);
	ahead->byte_rate			=	(channels * sample_rate * quant_bits/8);
	ahead->data_len			=	s;
}

void ulaw_head_init (struct xlaw_head_t *ahead, size_t s, int sample_rate, int channels, int quant_bits)
{
	memcpy (ahead, &default_ulaw_head, sizeof (default_ulaw_head));
	ahead->chunk_size	=	s + 50;
	ahead->channels		=	channels;
	ahead->sample_rate	=	sample_rate;
	ahead->bits_per_sample	=	quant_bits;
	ahead->block_align		=	(channels * quant_bits/8);
	ahead->byte_rate			=	(channels * sample_rate * quant_bits/8);
	ahead->data_len			=	s;
}


void pcm_head_init (struct pcm_head_t *ahead, size_t s, int sample_rate, int channels, int quant_bits)
{
	memcpy (ahead, &default_pcm_head, sizeof (default_pcm_head));
	ahead->chunk_size	=	s + sizeof (struct pcm_head_t) - 8; //s + 36;
	ahead->channels		=	channels;
	ahead->sample_rate	=	sample_rate;
	ahead->bits_per_sample	=	quant_bits;
	ahead->block_align		=	(channels * quant_bits/8);
	ahead->byte_rate			=	(channels * sample_rate * quant_bits/8);
	ahead->data_len			=	s;
}
