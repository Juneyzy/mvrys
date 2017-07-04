VER = $(shell cat package/vpw-info.yaml | grep " version:" | cut -b 12-15)
BUILD   = $(shell svn info | grep 'Last Changed Rev:' | cut -b 19-24)
BSTR = $(shell printf %05d $(BUILD))

TARGET = vrs_work

COMPILE = gcc
CPPCOMPILE = g++

LIBS += -L ../lib -lethxx -lFrontEnd_old -lSpkAPI -lwav -lpcap -lpfring -lapr-1 -laprutil-1
LIBS += -ldl -ljson -lpthread -lm -lyaml -lpcre -lstdc++

OBJS_LOCAL = vpw.o\
		vpw_dms_agent.o\
		vrs_session.o\
		vpw_init.o\
		../libx/vrs.o\
		../libx/tool.o\
		../libx/vrs_model.o\
		../libx/vrs_pkt.o\
		../libx/vrs_senior.o\
		../libx/vrs_rule.o\

CPP_OBJS_LOCAL =  ../libx/model.o

CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3 -I ../libx -I ../include -I ../vpm -I ../include/apr

.PHONY: vpw clean

vpw: $(TARGET)
$(TARGET): $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) 
	$(COMPILE) -o $@ $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(LIBS)

$(OBJS_LOCAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@

$(CPP_OBJS_LOCAL): %.o : %.cpp
	$(CPPCOMPILE) $(CFLAGS_LOCAL) -c $< -o $@

tarball: vpw
	if [ ! -d "package/bin" ]; then mkdir -p "package/bin"; fi
	if [ ! -d "package/lib" ]; then mkdir -p "package/lib"; fi
	if [ ! -d "package/etc" ]; then mkdir -p "package/etc"; fi
	if [ ! -d "../release" ]; then mkdir -p "../release"; fi
	cp vrs_work package/bin/vpw
	cp ../lib/*.so           package/lib/
	awk '($$2== "BUILDSTR") gsub("BUILDSTR","$(BSTR)")' package/vpw-info.yaml > package/vpw.yaml
	cd package && tar cpfz vpw-$(VER).$(BSTR).tar.gz bin etc lib SysFile  Makefile vpw.yaml
	cd ..
	mv package/vpw-$(VER).$(BSTR).tar.gz ../release/

clean:
	rm -rf warehouse logs $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(TARGET) 
	rm -rf package/bin package/lib package/vpw.yaml


