#include "sysdefs.h"

extern void ui_init_a29();
extern void ui_init_local();

void ui_init()
{
#if SPASR_BRANCH_EQUAL(BRANCH_A29)
    ui_init_a29();
#endif

#if SPASR_BRANCH_EQUAL (BRANCH_LOCAL)
    ui_init_local();
#endif
}

