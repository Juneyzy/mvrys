#ifndef __XCONFIG_H__
#define __XCONFIG_H__

#define BRANCH_A29          0x01
#define BRANCH_LOCAL      0x02
#define BRANCH_VRS          0x03

#define SPASR_BRANCH_EQUAL(branch)  ((SPASR_BRANCH_TARGET==branch)?1:0)

#endif
