#include "sysdefs.h"
#include "rt_throughput.h"
#include "rt_throughput_proto.h"
#include "rt_throughput_message.h"
#include "rt_throughput_exce.h"
#include "rt_throughput_sum.h"

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
static void
xtemplate_start()
{
    enum template_type tt;
    struct eval_template *templ;
    char xdesc[TASK_NAME_SIZE] = {0};

    for (tt = TEMP_RTSS; tt < TEMP_MAX; tt ++){
        templ = xtemplate[tt];
        if (!templ) continue;

        /** init fn start at first in every template */
        templ->priv.init();

        if (templ->cache_ctx){
            snprintf (xdesc, TASK_NAME_SIZE -1, "%s Cache Task", (templ->priv.desc));
            task_spawn(xdesc, 0, NULL, templ->cache_ctx, templ->priv.param());
        }

        if (templ->archive_ctx){
            snprintf (xdesc, TASK_NAME_SIZE -1, "%s Archive Task", (templ->priv.desc));
            task_spawn(xdesc, 0, NULL, templ->archive_ctx, templ->priv.param());
        }
   }
}
#endif

void rt_init()
{
#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
#if 0
    rt_throughput_init();
    rt_throughput_proto_init();
    rt_throughput_message_init();
    rt_throughput_sum_init();
    rt_throughput_exce_init();
#endif
    xtemplate_start();
#endif

}

