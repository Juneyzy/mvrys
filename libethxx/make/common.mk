include $(SPASR_ROOT)/make/config.mk
# Add flags set by config.mk
CFLAGS_GLOBAL += $(CFLAGS_COMMON_CONFIG)
ASFLAGS_GLOBAL += $(CFLAGS_COMMON_CONFIG)

CPPFLAGS_GLOBAL = -I$(SPASR_ROOT)
CPPFLAGS_GLOBAL += $(SPASR_CPPFLAGS_GLOBAL_ADD)

CFLAGS_GLOBAL = $(CPPFLAGS_GLOBAL)
CFLAGS_GLOBAL += $(SPASR_CFLAGS_GLOBAL_ADD)

CFLAGS_GLOBAL += -DSPASR_BRANCH_TARGET=${BRANCH_TARGET} -DSEMP_PLATFORM=${SEMP_PLATFORM} -DRELEASED=${RELEASED}
CFLAGS_GLOBAL += -Wall -Werror 

ASFLAGS_GLOBAL = $(CPPFLAGS_GLOBAL) -g
ASFLAGS_GLOBAL += $(SPASR_ASFLAGS_GLOBAL_ADD)

LDFLAGS_GLOBAL =
LDFLAGS_GLOBAL += $(SPASR_LDFLAGS_GLOBAL_ADD)

LDFLAGS_PATH = -L$(SPASR_ROOT)/target/lib

SUPPORTED_TARGETS=linux_64 linux_n32 linux_uclibc linux_o32
ifeq ($(findstring $(SPASR_ARCH_TARGET), $(SUPPORTED_TARGETS)),)
    ${error Invalid value for SPASR_ARCH_TARGET. Supported values: ${SUPPORTED_TARGETS}}
endif

ifeq (${SPASR_ARCH_TARGET},linux_64)
    PREFIX=-linux_64
    CFLAGS_GLOBAL += -DSPASR_ARCH_TARGET=${SPASR_ARCH_TARGET}
    ASFLAGS_GLOBAL += -DSPASR_ARCH_TARGET=${SPASR_ARCH_TARGET}
    LDFLAGS_GLOBAL += -lrt
else # linux_64
ifeq (${SPASR_ARCH_TARGET},linux_n32)
    # Add here
else # linux_n32
ifeq (${SPASR_ARCH_TARGET},linux_uclibc)
    PREFIX=-linux_uclibc
else # linux_uclibc
ifeq (${SPASR_ARCH_TARGET},linux_o32)
    PREFIX=-linux_o32
else # linux_o32
    ${error Invalid value for SPASR_ARCH_TARGET. Supported values: ${SUPPORTED_TARGETS}}
endif # linux_o32
endif # linux_uclibc
endif # linux_n32
endif # linux_64

ifeq (linux,$(findstring linux,$(SPASR_ARCH_TARGET)))
    CC = gcc
    AR = ar
    LD = ld
    STRIP = strip
    OBJDUMP = objdump
    NM = nm
else
    CC =
    AR = 
    LD = 
    STRIP = 
    OBJDUMP =
    NM =
endif

#  build object directory

OBJ_DIR = obj$(PREFIX)

#  standard compile line

COMPILE = $(CC) $(CFLAGS_GLOBAL) $(CFLAGS_LOCAL) -MD -c -o $@ $<

ASSEMBLE = $(CC) $(ASFLAGS_GLOBAL) $(ASFLAGS_LOCAL) -MD -c -o $@ $<

MYARCHIVE = $(AR) -cr $@ $<

