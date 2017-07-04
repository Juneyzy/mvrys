VER = $(shell cat package/vpm-info.yaml | grep " version:" | cut -b 12-15)
BUILD   = $(shell svn info | grep 'Last Changed Rev:' | cut -b 19-24)
BSTR = $(shell printf %05d $(BUILD))

TARGET = vrs_manager

COMPILE = gcc
CPPCOMPILE = g++

LIBS += -L ../lib -lethxx -lFrontEnd_old -lSpkAPI -lwav -lapr-1 -laprutil-1 -lpcap
LIBS += -ldl -ljson -lpthread -lm -lyaml -lpcre -lstdc++ -locilib

OBJS_LOCAL = vpm.o\
		vpm_dms_agent.o\
		vpm_init.o\
		vpm_boost.o \
		../libx/tool.o\
		../libx/vrs_model.o\
		../libx/vrs_rule.o\
		../libx/vrs_pkt.o\
		../libx/vrs.o\
		../libx/vrs_oci.o\
		../libx/vrs_senior.o

CPP_OBJS_LOCAL =  ../libx/model.o

CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
		-I ../libx\
		-I ../include\
		-I ../vpw \
		-I ../include/apr\
		-I ../vpm 

.PHONY: vpm clean

vpm: $(TARGET)
$(TARGET): $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) 
	$(COMPILE) -o $@ $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(LIBS)

$(OBJS_LOCAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@

$(CPP_OBJS_LOCAL): %.o : %.cpp
	$(CPPCOMPILE) $(CFLAGS_LOCAL) -c $< -o $@

tarball: vpm
	if [ ! -d "package/bin" ]; then mkdir -p "package/bin"; fi
	if [ ! -d "package/lib" ]; then mkdir -p "package/lib"; fi
	if [ ! -d "package/etc" ]; then mkdir -p "package/etc"; fi
	if [ ! -d "../release" ]; then mkdir -p "../release"; fi
	cp vrs_manager package/bin/vpm
	cp ../lib/*.so* 	 package/lib/
	awk '($$2== "BUILDSTR") gsub("BUILDSTR","$(BSTR)")' package/vpm-info.yaml > package/vpm.yaml
	cd package && tar cpfz vpm-$(VER).$(BSTR).tar.gz bin etc lib SysFile Makefile vpm.yaml 
	cd ..
	mv package/vpm-$(VER).$(BSTR).tar.gz ../release/

clean:
	rm -rf $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(TARGET) 
	rm -rf package/bin package/lib package/vpm.yaml


