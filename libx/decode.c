#include "sysdefs.h"
#include "decode.h"
#include "G711.h"
#include "linear.h"

static struct audio_decoder_t x_law_decoder = {
	.dcd_name	=	"X-Law Decoder",
	.s			=	0,
};


