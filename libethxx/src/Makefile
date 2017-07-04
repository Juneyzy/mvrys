
ifndef SPASR_ROOT
SPASR_ROOT = ..
endif

MAJOR="1.0"
BUILD=$(shell git log | grep -cE 'Author:.*<.*@.*>')
BUILDSHA=$(shell git rev-parse --short HEAD)
BUILD_FINGERPRINT=$(MAJOR).$(BUILD).$(BUILDSHA)

# Default simulation parameters
NUM_PROCESSORS=1

#SPASR_ARCH_TARGET:=linux_64

TARGET = PR-AGENT
TARGET_LIB = $(OBJ_DIR)/libethxx.a

all: ${TARGET}

#  standard common Makefile fragment
include $(SPASR_ROOT)/make/config.mk
include $(SPASR_ROOT)/make/common.mk

# 3RD library path
LIB_PATH := $(SPASR_ROOT)/libs

#  application specification

dir := .
OBJS := \
		$(OBJ_DIR)/main.o

dir := ./lib
include $(dir)/lib.mk
OBJS += $(OBJS_$(dir))

dir := ./rt
include $(dir)/rt.mk
OBJS += $(OBJS_$(dir))

dir := ./rpc
include $(dir)/rpc.mk
OBJS += $(OBJS_$(dir))

dir := ./fes
include $(dir)/fes.mk
OBJS += $(OBJS_$(dir))

dir := ./bes/json_package
include $(dir)/json_package.mk
OBJS += $(OBJS_$(dir))

dir := ./bes
include $(dir)/bes.mk
OBJS += $(OBJS_$(dir))

dir := ./cluster
include $(dir)/cluster.mk
OBJS += $(OBJS_$(dir))

dir := ./cli
include $(dir)/cli.mk
OBJS += $(OBJS_$(dir))

dir := ./components/inotify
include $(dir)/inotify.mk
OBJS += $(OBJS_$(dir))

dir := ./components/yaml
include $(dir)/yaml.mk
OBJS += $(OBJS_$(dir))

dir := ./components/command-line-engine
include $(dir)/cli-engine.mk
OBJS += $(OBJS_$(dir))
   
LDFLAGS_GLOBAL += \
                  -lpthread -lcrypt -lm -lyaml -lpcre -lpcap -ljson

CFLAGS_LOCAL = -std=gnu99 -g -O3 -W -Wall -Wunused-parameter \
               -I.\
               -I./lib\
               -I./fes\
               -I./bes\
               -I./bes/json_package\
               -I./cluster\
               -I./cli\
               -I./rt\
               -I./rpc\
               -I./components/yaml\
               -I./components/inotify\
               -I./components/command-line-engine\

DEPS   :=  $(objs:.o=.d)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS) $(DEPS) $(TARGET_LIB) $(OBJ_DIR)

tarball_vrs: $(OBJS)
	ar rcs $(TARGET_LIB) $^
	sh script/tarball_vrs.sh $(TARGET_LIB) $(MAJOR).$(BUILD) $(BUILDSHA)

tarball_a29: $(OBJS)
	ar rcs $(TARGET_LIB) $^
	sh script/tarball_a29.sh $(TARGET_LIB) $(MAJOR).$(BUILD) $(BUILDSHA)

tarball:
	sh script/tarball_all.sh $(TARGET) $(MAJOR).$(BUILD) $(BUILDSHA)

$(OBJ_DIR)/%.o: $(d)/%.c
	$(COMPILE)

include $(SPASR_ROOT)/make/application.mk

clean: 
	rm -rf $(CLEAN_LIST)
	rm -f $(TARGET)

clobber: clean
	rm -rf $(OBJ_DIR)

