#
# Makefile for compile SPASR
# Created by TSIHANG <qihang@semptian.com>
#
MAJOR="1.0"
BUILD=$(shell git log | grep -cE 'Author:.*<.*@.*>')
BUILDSHA=$(shell git rev-parse --short HEAD)
BUILD_FINGERPRINT=$(MAJOR).$(BUILD).$(BUILDSHA)

P_ORIGIN:=$(origin P)

#override PROJECT:=$(shell echo $(P) | tr '[a-z]' '[A-Z]')
P=$(PROJECT)

#Not used in openwrt compiling
CPUS = $(shell grep -c processor /proc/cpuinfo)
SUBDIRS:= src 

QMAKE := $(MAKE)


.PHONY: all clean distclean

all: prereq prepare
	for d in $(SUBDIRS); do \
        $(QMAKE) -C $$d; \
    done

prereq:
	@echo Platform and revision checking...
	@echo "Revision: ${BUILD_FINGERPRINT}."

prepare:
	@echo nothing to do.

distclean: 
	for d in $(SUBDIRS); do \
        $(QMAKE) clobber -C $$d; \
    done

clean:
	$(QMAKE) clean -C src


version:
	@echo ":: Platform-$(P), Revision-${BUILD_FINGERPRINT}"
