#include "sysdefs.h"
#include "tool.h"
#include "vrs_model.h"


static struct rt_vrstool_t	default_engine_ops = {
	.engine_init = VRSEngineInit,
	.model_cvrto_ops = WavToModel,
	.model_cvrto_cache_ops = WavToModelMemory,
	.voice_recognition_ops = WavToProbability,
	.voice_recognition_advanced_ops = WavToProbabilityAdvanced,
	.sg_batch_counter = ATOMIC_INIT (0),
	.sg_batch = ATOMIC_INIT (0),
	.sg_batch_size = ATOMIC_INIT (0),
	.categories = ATOMIC_INIT (0),
	.category_entries = ATOMIC_INIT (0),
	.modelist_lock =	PTHREAD_MUTEX_INITIALIZER,
	.modelist		=	NULL,
	.category_dir	=	"./data.cat",
	.model_dir	=	"./temp.model",
	.batch_dir	=	"./data",
	.threshold	=	65,
	.circulating_factor = ATOMIC_INIT (0),
	.allowded_max_tasks = MAX_INWORK_CORES,
	.cur_tasks	=	4,
};

struct rt_vrstool_t *engine_default_ops ()
{
	return &default_engine_ops;
}
