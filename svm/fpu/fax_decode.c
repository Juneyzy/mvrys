#include "sysdefs.h"
#include "fax_decode.h"
#include "G711.h"

#define DISBIT1     0x01
#define DISBIT2     0x02
#define DISBIT3     0x04
#define DISBIT4     0x08
#define DISBIT5     0x10
#define DISBIT6     0x20
#define DISBIT7     0x40
#define DISBIT8     0x80

enum
{
    FAX_NONE,
    FAX_V27TER_RX,
    FAX_V29_RX,
    FAX_V17_RX
};

static const struct
{
    int bit_rate;
    int modem_type;
    int which;
    uint8_t dcs_code;
} fallback_sequence[] =
{
    {14400, T30_MODEM_V17,    T30_SUPPORT_V17,    DISBIT6},
    {12000, T30_MODEM_V17,    T30_SUPPORT_V17,    (DISBIT6 | DISBIT4)},
    { 9600, T30_MODEM_V17,    T30_SUPPORT_V17,    (DISBIT6 | DISBIT3)},
    { 9600, T30_MODEM_V29,    T30_SUPPORT_V29,    DISBIT3},
    { 7200, T30_MODEM_V17,    T30_SUPPORT_V17,    (DISBIT6 | DISBIT4 | DISBIT3)},
    { 7200, T30_MODEM_V29,    T30_SUPPORT_V29,    (DISBIT4 | DISBIT3)},
    { 4800, T30_MODEM_V27TER, T30_SUPPORT_V27TER, DISBIT4},
    { 2400, T30_MODEM_V27TER, T30_SUPPORT_V27TER, 0},
    {    0, 0, 0, 0}
};

int decode_test = FALSE;
int rx_bits = 0;

static struct fax_context_t default_fax_context = {
	.t4_up = FALSE,
	.fast_trained = FAX_NONE,
	.line_encoding = T4_COMPRESSION_ITU_T4_1D,
	.x_resolution = T4_X_RESOLUTION_R8,
	.y_resolution = T4_Y_RESOLUTION_STANDARD,
	.image_width = 1728,
	.octets_per_ecm_frame = 256,
	.error_correcting_mode = FALSE,
	.current_fallback = 0,
};

static inline void decode_20digit_msg(const uint8_t *pkt, int len)
{
	int p;
	int k;
	char msg[T30_MAX_IDENT_LEN + 1];

	if (len > T30_MAX_IDENT_LEN + 3) {
	    rt_log_warning (ERRNO_FAX_DECODE, 
			"XXX %d %d\n", len, T30_MAX_IDENT_LEN + 1);
	    msg[0] = '\0';
	    return;
	}
	pkt += 2;
	p = len - 2;
	/* Strip trailing spaces */
	while (p > 1  &&  pkt[p - 1] == ' ')
	    p--;
	/* The string is actually backwards in the message */
	k = 0;
	while (p > 1)
	    msg[k++] = pkt[--p];
	msg[k] = '\0';
	
	rt_log_warning (ERRNO_FAX_DECODE, "%s is: \"%s\"\n", t30_frametype(pkt[0]), msg);
}

static inline  int find_fallback_entry(int dcs_code)
{
	int i;

	/* The table is short, and not searched often, so a brain-dead linear scan seems OK */
	for (i = 0;  fallback_sequence[i].bit_rate;  i++) {
	    if (fallback_sequence[i].dcs_code == dcs_code)
	        break;
	}

	if (fallback_sequence[i].bit_rate == 0)
	    return -1;

	return i;
}

static inline void print_frame (struct fax_context_t *fctx, const char *io, const uint8_t *fr, int frlen)
{
	int i, type;
	const char *country;
	const char *vendor;
	const char *model;
    
	rt_log_warning (ERRNO_FAX_DECODE,
		"(%s:%d)%s %s:", __FILE__, __LINE__, io, t30_frametype(fr[2]));
	for (i = 2;  i < frlen;  i++)
		fprintf(stderr, " %02x", fr[i]);
	fprintf(stderr, "\n");

	type = fr[2] & 0xFE;
	
	if (type == T30_DIS  ||  type == T30_DTC  ||  type == T30_DCS)
		t30_decode_dis_dtc_dcs (&fctx->t30_dummy, fr, frlen);
	if (type == T30_CSI  ||  type == T30_TSI  ||  type == T30_PWD  ||  type == T30_SEP  ||  type == T30_SUB  ||  type == T30_SID)
		decode_20digit_msg (fr, frlen);

	if (type == T30_NSF  ||  type == T30_NSS  ||  type == T30_NSC) {
		if (t35_decode(&fr[3], frlen - 3, &country, &vendor, &model)) {
			if (country)
			    rt_log_warning (ERRNO_FAX_DECODE, "The remote was made in '%s'\n", country);
			if (vendor)
			    rt_log_warning (ERRNO_FAX_DECODE, "The remote was made by '%s'\n", vendor);
			if (model)
			    rt_log_warning (ERRNO_FAX_DECODE, "The remote is a '%s'\n", model);
		}
	}
}

static int check_rx_dcs (struct fax_context_t *fctx, const uint8_t *msg, int len)
{
	static const int widths[3][4] =
	{
		{ 864, 1024, 1216, -1}, /* R4 resolution - no longer used in recent versions of T.30 */
		{1728, 2048, 2432, -1}, /* R8 resolution */
		{3456, 4096, 4864, -1}  /* R16 resolution */
	};
	uint8_t dcs_frame[T30_MAX_DIS_DTC_DCS_LEN];

	/* Check DCS frame from remote */
	if (len < 6) {
		rt_log_warning (ERRNO_FAX_DECODE,
			"Short DCS frame");
		return -1;
	}

	/* Make a local copy of the message, padded to the maximum possible length with zeros. This allows
	   us to simply pick out the bits, without worrying about whether they were set from the remote side. */
	if (len > T30_MAX_DIS_DTC_DCS_LEN) {
	    memcpy(dcs_frame, msg, T30_MAX_DIS_DTC_DCS_LEN);
	}
	else {
	    memcpy(dcs_frame, msg, len);
	    if (len < T30_MAX_DIS_DTC_DCS_LEN)
			memset(dcs_frame + len, 0, T30_MAX_DIS_DTC_DCS_LEN - len);
	}

	fctx->octets_per_ecm_frame = (dcs_frame[6] & DISBIT4)  ?  256  :  64;
	if ((dcs_frame[8] & DISBIT1))
		fctx->y_resolution = T4_Y_RESOLUTION_SUPERFINE;
	else if (dcs_frame[4] & DISBIT7)
		fctx->y_resolution = T4_Y_RESOLUTION_FINE;
	else
		fctx->y_resolution = T4_Y_RESOLUTION_STANDARD;

	fctx->image_width = widths[(dcs_frame[8] & DISBIT3)  ?  2  :  1][dcs_frame[5] & (DISBIT2 | DISBIT1)];

	/* Check which compression we will use. */
	if ((dcs_frame[6] & DISBIT7))
		fctx->line_encoding = T4_COMPRESSION_ITU_T6;
	else if ((dcs_frame[4] & DISBIT8))
		fctx->line_encoding = T4_COMPRESSION_ITU_T4_2D;
	else
		fctx->line_encoding = T4_COMPRESSION_ITU_T4_1D;

	fprintf(stderr, "Selected compression %d\n", fctx->line_encoding);

	if ((fctx->current_fallback = find_fallback_entry(dcs_frame[4] & (DISBIT6 | DISBIT5 | DISBIT4 | DISBIT3))) < 0)
		printf("Remote asked for a modem standard we do not support\n");

	fctx->error_correcting_mode = ((dcs_frame[6] & DISBIT3) != 0);

	//v17_rx_restart(&v17, fallback_sequence[fallback_entry].bit_rate, FALSE);
	return 0;
}

static inline void t4_begin (struct fax_context_t *fctx)
{
	int i;

	rt_log_debug ("Begin T.4 - %d %d %d %d", 
		fctx->line_encoding, fctx->x_resolution, fctx->y_resolution, fctx->image_width);
	
	t4_rx_set_rx_encoding (&fctx->t4_rx_state, fctx->line_encoding);
	t4_rx_set_x_resolution (&fctx->t4_rx_state, fctx->x_resolution);
	t4_rx_set_y_resolution (&fctx->t4_rx_state, fctx->y_resolution);
	t4_rx_set_image_width (&fctx->t4_rx_state, fctx->image_width);

	t4_rx_start_page (&fctx->t4_rx_state);
	fctx->t4_up = TRUE;

	for (i = 0;  i < 256;  i++)
		fctx->ecm_len[i] = -1;
}

static inline void t4_end (struct fax_context_t *fctx)
{
	t4_stats_t stats;
	int i, shift = 0;
	char bit[256] = {0};
	
	if (!fctx->t4_up)
		return;
	
	if (fctx->error_correcting_mode)
	{
		for (i = 0;  i < 256;  i++)
		{
			if (fctx->ecm_len[i] > 0)
				t4_rx_put_chunk (&fctx->t4_rx_state, fctx->ecm_data[i], fctx->ecm_len[i]);

			shift += snprintf (bit + shift, 256 - shift, "%d ", (fctx->ecm_len[i] <= 0)  ?  0  :  1);
		}
	}
	
	t4_rx_end_page(&fctx->t4_rx_state);
	t4_rx_get_transfer_statistics(&fctx->t4_rx_state, &stats);

	fctx->t4_up = FALSE;

	rt_log_debug ("Bits = %s", bit);
	rt_log_debug ("Pages = %d", stats.pages_transferred);
	rt_log_debug ("Image size = %dx%d", stats.width, stats.length);
	rt_log_debug ("Image resolution = %dx%d", stats.x_resolution, stats.y_resolution);
	rt_log_debug ("Bad rows = %d", stats.bad_rows);
	rt_log_debug ("Longest bad row run = %d", stats.longest_bad_row_run);
	
}


static void hdlc_accept(void *user_data, const uint8_t *pkt, int len, int ok)
{
	int type;
	int frame_no;

	struct fax_context_t *fctx;
	fctx = (struct fax_context_t *)user_data;

	if (len < 0) {
		/* Special conditions */
		rt_log_notice ("xxxxxxxxxxxxxxxx HDLC status is %s (%d)", signal_status_to_str(len), len);
		return;
	}

	if (ok) {
		if (pkt[0] != 0xFF  ||  !(pkt[1] == 0x03  ||  pkt[1] == 0x13)) {
			rt_log_notice ("Bad frame header - %02x %02x", pkt[0], pkt[1]);
			return;
		}
		int i;
	        printf("Good frame, len = %d\n", len);
	        printf("HDLC:  ");
	        for (i = 0;  i < len;  i++)
	            printf("%02X ", pkt[i]);
	        printf("\n");
		print_frame (fctx, "HDLC: ", pkt, len);
		type = pkt[2] & 0xFE;
		switch (type) {
			case T4_FCD:
				if (len <= 4 + 256) {
					frame_no = pkt[3];
					/* Just store the actual image data, and record its length */
					memcpy(&fctx->ecm_data[frame_no][0], &pkt[4], len - 4);
					fctx->ecm_len[frame_no] = (int16_t) (len - 4);
				}
				break;
			case T30_DCS:
				check_rx_dcs (fctx, pkt, len);
				break;
		}
    }
}

static void v17_put_bit(void *user_data, int bit)
{

	struct fax_context_t *fctx;
	fctx = (struct fax_context_t *)user_data;

	if (bit < 0) {
		/* Special conditions */
		rt_log_debug ("V.17 rx status is %s (%d)", 
			signal_status_to_str(bit), bit);
		switch (bit) {
			case SIG_STATUS_TRAINING_SUCCEEDED:
				fctx->fast_trained = FAX_V17_RX;
				t4_begin(fctx);
				break;
			case SIG_STATUS_CARRIER_DOWN:
				t4_end(fctx);
				if (fctx->fast_trained == FAX_V17_RX)
				    fctx->fast_trained = FAX_NONE;
				break;
		}
		return;
	}
	
	if (fctx->error_correcting_mode)
	    hdlc_rx_put_bit (&fctx->hdlcrx, bit);
	else {
		if (t4_rx_put_bit (&fctx->t4_rx_state, bit)){
			t4_end(fctx);
			rt_log_info ("End of page detected");
		}
    	}
	
}

static void v21_put_bit(void *user_data, int bit)
{

	struct fax_context_t *fctx;
	fctx = (struct fax_context_t *)user_data;

	if (bit < 0) {
		/* Special conditions */
		rt_log_debug ("V.21 rx status is %s (%d)", 
			signal_status_to_str(bit), bit);

		switch (bit) {
			case SIG_STATUS_CARRIER_DOWN:
				//t4_end();
				break;
		}
		return;
	}

	if (fctx->fast_trained == FAX_NONE)
		hdlc_rx_put_bit (&fctx->hdlcrx, bit);
    
}

static void v27ter_put_bit(void *user_data, int bit)
{

	struct fax_context_t *fctx;
	fctx = (struct fax_context_t *)user_data;

	if (bit < 0) {
		/* Special conditions */
		rt_log_debug ("V.27ter rx status is %s (%d)", 
			signal_status_to_str(bit), bit);
		switch (bit) {
			case SIG_STATUS_TRAINING_SUCCEEDED:
				fctx->fast_trained = FAX_V27TER_RX;
				t4_begin(fctx);
				break;
			case SIG_STATUS_CARRIER_DOWN:
				t4_end(fctx);
				if (fctx->fast_trained == FAX_V27TER_RX)
				    fctx->fast_trained = FAX_NONE;
				break;
		}
		return;
	}
	
	if (fctx->error_correcting_mode)
	    hdlc_rx_put_bit (&fctx->hdlcrx, bit);
	else {
		if (t4_rx_put_bit (&fctx->t4_rx_state, bit)){
			t4_end(fctx);
			rt_log_info ("End of page detected");
		}
    	}
}

static void v29_put_bit (void *user_data, int bit)
{

	struct fax_context_t *fctx;
	fctx = (struct fax_context_t *)user_data;

	if (bit < 0) {
		/* Special conditions */
		rt_log_debug ("V.29 rx status is %s (%d)", 
			signal_status_to_str(bit), bit);
		switch (bit) {
			case SIG_STATUS_TRAINING_SUCCEEDED:
				fctx->fast_trained = FAX_V29_RX;
				t4_begin(fctx);
				break;
			case SIG_STATUS_CARRIER_DOWN:
				t4_end(fctx);
				if (fctx->fast_trained == FAX_V29_RX)
				    fctx->fast_trained = FAX_NONE;
				break;
		}
		return;
	}
	
	if (fctx->error_correcting_mode)
	    hdlc_rx_put_bit (&fctx->hdlcrx, bit);
	else {
		if (t4_rx_put_bit (&fctx->t4_rx_state, bit)){
			t4_end(fctx);
			rt_log_info ("End of page detected");
		}
    	}
}

int fax_converto_alaw (const char *src, const char *alaw_file)
{

	struct xlaw_head_t ahead;
	FILE	*fpin, *fpout;
	char	c;
	struct stat st;  
	
	fpin = fopen(src, "r");  
	if (NULL == fpin)  {  
		rt_log_notice ("%s: %s", src, strerror(errno));
		return -1;  
	}  

	if (stat(src, &st) != 0) {  
		rt_log_notice ("%s: %s", src, strerror(errno));
		fclose (fpin);
		return -1;  
	} 
	
	alaw_head_init (&ahead, st.st_size, 8000, 1, 8);
	
	fpout = fopen (alaw_file, "w+");
	if (NULL == fpout)  {  
		fclose (fpin);  
		rt_log_notice ("%s: %s", alaw_file, strerror(errno));
		return -1;
	}
	
	fwrite (&ahead, sizeof (struct xlaw_head_t), 1, fpout);
	while (1 == fread (&c, sizeof(char), 1, fpin))  {
		c = c ^ 0x53;
		fputc(c, fpout);
	}

	fclose (fpin);
	fclose (fpout);
	
	return 0;
}

int fax_context_init (struct fax_context_t *fctx)
{
	logging_state_t *logging;

	if (unlikely (!fctx)) {
		rt_log_warning (ERRNO_FAX_DECODE,
			"Invalid fax context instance.");
		return -1;
	}
	
	memcpy (fctx, &default_fax_context, sizeof (struct fax_context_t));
	
	memset (&fctx->t30_dummy, 0, sizeof(t30_state_t));
	span_log_init (&fctx->t30_dummy.logging, SPAN_LOG_FLOW, NULL);
	span_log_set_protocol (&fctx->t30_dummy.logging, "T.30");

	hdlc_rx_init (&fctx->hdlcrx, FALSE, TRUE, 5, hdlc_accept, (void *)fctx);
	fctx->fsk = fsk_rx_init		(NULL, &preset_fsk_specs[FSK_V21CH2], FSK_FRAME_MODE_SYNC, v21_put_bit, (void *)fctx);
	fctx->v17 = v17_rx_init	(NULL, 14400, v17_put_bit, (void *)fctx);
	fctx->v29 = v29_rx_init	(NULL, 9600, v29_put_bit, (void *)fctx);
	//fctx->v29 = v29_rx_init	(NULL, 7200, v29_put_bit, NULL);
	fctx->v27ter = v27ter_rx_init	(NULL, 4800, v27ter_put_bit, (void *)fctx);

	fsk_rx_signal_cutoff	(fctx->fsk, -45.5);
	v17_rx_signal_cutoff	(fctx->v17, -45.5);
	v29_rx_signal_cutoff	(fctx->v29, -45.5);
	v27ter_rx_signal_cutoff	(fctx->v27ter, -40.0);

#if 1
	logging = v17_rx_get_logging_state(fctx->v17);
	span_log_init(logging, SPAN_LOG_FLOW, NULL);
	span_log_set_protocol(logging, "V.17");
	span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);

	logging = v29_rx_get_logging_state(fctx->v29);
	span_log_init(logging, SPAN_LOG_FLOW, NULL);
	span_log_set_protocol(logging, "V.29");
	span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);

	logging = v27ter_rx_get_logging_state(fctx->v27ter);
	span_log_init(logging, SPAN_LOG_FLOW, NULL);
	span_log_set_protocol(logging, "V.27ter");
	span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
#endif


	return 0;
}

SNDFILE *rt_sf_open (const char *alaw_file, int sfm)
{
	SNDFILE *sfp = NULL;
	SF_INFO info;

	if ((sfp = sf_open (alaw_file, sfm, &info)) == NULL) {
		rt_log_notice ("Can not open audio file(\"%s\")", alaw_file);
        	return sfp;
	}

	if (info.samplerate != SAMPLE_RATE) {
		rt_log_notice ("Unexpected sample rate in audio file(\"%s\")", alaw_file);
		return sfp;
	}
	
	if (info.channels != 1) {
		rt_log_notice ("Unexpected number of channels in audio file(\"%s\")", alaw_file);
		return sfp;
	}
	
	rt_log_debug ("Using %s.\n", sf_version_string ()) ;  
	rt_log_debug ("File Name : %s\n", alaw_file);  
	rt_log_debug ("Sample Rate : %d\n", info.samplerate);  
	rt_log_debug ("Channels : %d\n", info.channels);  
	rt_log_debug ("Sections  : %d\n", info.sections );  
	rt_log_debug ("Frames   : %d\n", (int)info.frames );


	return sfp;
}

int fax_decode_alaw (struct fax_context_t *fctx, const char *file_real_path)
{
	char file_realpath [256] = {0};
	sf_count_t s;
	int16_t amp[SAMPLES_PER_CHUNK];
	SNDFILE *sfp;
	SF_INFO info;

	memset(&info, 0, sizeof(info));
		
	sfp = rt_sf_open (file_real_path, SFM_READ);

	sprintf(file_realpath, "%s.tif", file_real_path);
	if (t4_rx_init (&fctx->t4_rx_state, file_realpath, T4_COMPRESSION_ITU_T4_2D) == NULL){
		return -1;
	}

	FOREVER	{
		s = sf_readf_short (sfp, amp, SAMPLES_PER_CHUNK);
		if (s < SAMPLES_PER_CHUNK)
			break;

		fsk_rx (fctx->fsk, amp, s);
		v17_rx (fctx->v17, amp, s);
		v29_rx (fctx->v29, amp, s);
		v27ter_rx (fctx->v27ter, amp, s);
	}
	
	t4_rx_release (&fctx->t4_rx_state);
	
	if (sf_close(sfp)){
		rt_log_notice ("Cannot close audio file(\"%s\")", file_realpath);
		return -1;
	}

	rt_log_info ("Fax decode %s (\"%s\")", rt_file_exsit (file_realpath) ? "success" : "failure", file_realpath);
	
	return 0;
}

/**
static int test1(int argc, char * argv[])  
{ 
    FILE *fpin  = NULL;  
    FILE *fpout = NULL;  
    struct pcm_head_t pcm_head;  
    struct xlaw_head_t alaw_head;  
    char format[5] = ".wav";  
    char filename[50];  
    char c;  
    short pcm_val;  
    struct stat st;  
    int move_len = 0;  
  
    if (argc != 5 || ('0' > argv[2][0] || argv[2][0] > '5'))  
    {  
        print_error();  
        return -1;  
    }  
  
    move_len = atol(argv[4]);  
  
    if ('0' == argv[2][0])  // pcm:wav -> a-lam:wav  
    {  
        fpin = fopen(argv[1], "r");  
        if (NULL == fpin)  
        {  
            printf("OpenFile Error!\n");  
            return -1;  
        }  
        memset(filename, 0, sizeof (filename));  
        if (NULL != strstr(argv[3], format))  
        {  
            sprintf(filename, "%s", argv[3]);  
        }  
        else  
        {  
            sprintf(filename, "%s%s", argv[3], format);  
        }  
        fpout = fopen(filename, "w+");  
        if (NULL == fpout)  
        {  
            fclose(fpin);  
            printf("OpenFile Error!\n");  
            return -1;  
        }  
  
        if (1 != fread(&pcm_head, sizeof(struct pcm_head_t), 1, fpin))  
        {  
            printf("ReadFile Error!\n");  
            goto END;  
        }  
  
        if (pcm_head.fmt_tag != 0x1)  
        {  
            printf("fmt_tag Error!\n");  
            goto END;  
        }  
  
        memset(&alaw_head, 0, sizeof (struct xlaw_head_t));  
        alaw_head.chunk_id = pcm_head.chunk_id;  
        alaw_head.chunk_size = (pcm_head.chunk_size - 36) / 2 + 50;  
        alaw_head.form_type = pcm_head.form_type;  
        alaw_head.fmt = pcm_head.fmt;  
        alaw_head.filtered_bytes = pcm_head.filtered_bytes;//0x00000012;  
        alaw_head.fmt_tag = 0x0006;  
        alaw_head.channels = pcm_head.channels;  
        alaw_head.sample_rate = pcm_head.sample_rate;  
        alaw_head.byte_rate = pcm_head.byte_rate;  
        alaw_head.block_align = pcm_head.block_align;  
        alaw_head.bits_per_sample = pcm_head.bits_per_sample;  
        alaw_head.fact = 0x74636166;    // ????? "fact"  
        alaw_head.t1 = 0x00000004;  
        alaw_head.t2 = (pcm_head.chunk_size - 36) / 2;//0x00075300;  
        alaw_head.data_tag = pcm_head.data_tag;  
        alaw_head.data_len = (pcm_head.chunk_size - 36) / 2;  
  
        fwrite(&alaw_head, sizeof(struct xlaw_head_t), 1, fpout);  
  
        while (1 == fread(&pcm_val, sizeof(short), 1, fpin))  
        {  
            c = linear2alaw(pcm_val);
            fputc(c, fpout);  
        }  
    }  
    else if ('5' == argv[2][0]) // a-lam:wav -> pcm:wav  
    {  
        fpin = fopen(argv[1], "r");
        if (NULL == fpin)  
        {  
            printf("OpenFile Error!\n");  
            return -1;  
        }  
        memset(filename, 0, sizeof (filename));  
        if (NULL != strstr(argv[3], format))  
        {  
            sprintf(filename, "%s", argv[3]);  
        }  
        else  
        {  
            sprintf(filename, "%s%s", argv[3], format);  
        }  
        fpout = fopen(filename, "w+");  
        if (NULL == fpout)  
        {  
            fclose(fpin);  
            printf("OpenFile Error!\n");  
            return -1;  
        }  
  
        if (1 != fread(&alaw_head, sizeof(struct xlaw_head_t), 1, fpin))  
        {  
            printf("ReadFile Error!\n");  
            goto END;  
        }  
  
        if (alaw_head.fmt_tag != 0x6)  
        {  
            printf("fmt_tag Error!\n");  
            goto END;  
        }  
  
        memset(&pcm_head, 0, sizeof (struct pcm_head_t));  
        pcm_head.chunk_id = alaw_head.chunk_id;  
        pcm_head.chunk_size = alaw_head.data_len * 2 + 36;  
        pcm_head.form_type = alaw_head.form_type;  
        pcm_head.fmt = alaw_head.fmt;  
        pcm_head.filtered_bytes = alaw_head.filtered_bytes;//0x00000012;  
        pcm_head.fmt_tag = 0x0001;  
        pcm_head.channels = alaw_head.channels;  
        pcm_head.sample_rate = alaw_head.sample_rate;  
        pcm_head.byte_rate = alaw_head.byte_rate;  
        pcm_head.block_align = alaw_head.block_align;  
        pcm_head.bits_per_sample = alaw_head.bits_per_sample;  
        pcm_head.data_tag = alaw_head.data_tag;  
        pcm_head.data_len = alaw_head.data_len * 2;  
  
        fwrite(&pcm_head, sizeof(struct pcm_head_t), 1, fpout);  
  
        while (1 == fread(&c, sizeof(char), 1, fpin))  
        {  
            pcm_val = alaw2linear(c);  
            fwrite(&pcm_val, sizeof(short), 1, fpout);  
        }  
    }  
    else  
    {  
        if (NULL != strstr(argv[1], format))  
        {  
            printf("FileFormat Error!\n");  
            return -1;  
        }  
  
        if (stat(argv[1], &st) != 0)  
        {  
            printf("Read FileSize Error!\n");  
            return -1;  
        }  
  
        fpin = fopen(argv[1], "r");  
        if (NULL == fpin)  
        {  
            printf("OpenFile Error!\n");  
            return -1;  
        }  
  
        memset(filename, 0, sizeof (filename));  
        if (NULL != strstr(argv[3], format))  
        {  
            sprintf(filename, "%s", argv[3]);  
        }  
        else  
        {  
            sprintf(filename, "%s%s", argv[3], format);  
        }  
        fpout = fopen(filename, "w+");  
        if (NULL == fpout)  
        {  
            fclose(fpin);  
            printf("OpenFile Error!\n");  
            return -1;  
        }  
          
        if (move_len)  
        {  
            fseek(fpin, move_len, SEEK_SET);  
        }  
  
        if ('1' == argv[2][0])  // a-lam -> a-lam:wav  
        {  
            init_alaw_head(&alaw_head);  
            alaw_head.chunk_size = (st.st_size - move_len) + 50;  
            alaw_head.t2 = (st.st_size - move_len);  
            alaw_head.data_len = (st.st_size - move_len);  
  
            fwrite(&alaw_head, sizeof(struct xlaw_head_t), 1, fpout);  
  
            while (1 == fread(&c, sizeof(char), 1, fpin))  
            {  
                fputc(c, fpout);  
            }  
        }  
        else if ('2' == argv[2][0]) // a-lam -> pcm:wav  
        {  
            init_pcm_head(&pcm_head);  
            pcm_head.chunk_size = (st.st_size - move_len) * 2 + 36;  
            pcm_head.data_len = (st.st_size - move_len) * 2;  
  
            fwrite(&pcm_head, sizeof(struct pcm_head_t), 1, fpout);  
  
            while (1 == fread(&c, sizeof(char), 1, fpin))  
            {  
                pcm_val = alaw2linear(c);  
                fwrite(&pcm_val, sizeof(short), 1, fpout);  
            }  
        }  
        else if ('3' == argv[2][0]) // pcm -> pcm:wav  
        {  
            init_pcm_head(&pcm_head);  
            pcm_head.chunk_size = (st.st_size - move_len) + 36;  
            pcm_head.data_len = (st.st_size - move_len);  
  
            fwrite(&pcm_head, sizeof(struct pcm_head_t), 1, fpout);  
  
            while (1 == fread(&pcm_val, sizeof(short), 1, fpin))  
            {  
                fwrite(&pcm_val, sizeof(short), 1, fpout);  
            }  
        }  
        else if ('4' == argv[2][0]) // pcm -> a-lam:wav
        {  
            init_alaw_head(&alaw_head);  
            alaw_head.chunk_size = (st.st_size - move_len) / 2 + 50;  
            alaw_head.t2 = (st.st_size - move_len) / 2;  
            alaw_head.data_len = (st.st_size - move_len) / 2;  
  
            fwrite(&alaw_head, sizeof(struct xlaw_head_t), 1, fpout);  
  
            while (1 == fread(&pcm_val, sizeof(short), 1, fpin))  
            {  
                c = linear2alaw(pcm_val);  
                fputc(c, fpout);  
            }  
        }  
    }  
  
    printf("OK!\n");  
  
END:  
    fclose(fpin);  
    fclose(fpout);  
  
    return 1;  
}  
*/

