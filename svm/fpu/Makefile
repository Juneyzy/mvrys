TARGET = fpu

COMPILE = gcc
CPPCOMPILE = g++

LIBS += -L../../lib -lethxx -lsndfile\
	-L../lib -lspandsp -ltiff

LIBS += -ldl -ljson -lpthread -lm -lpcap -lyaml -lpcre -lstdc++

OBJS_LOCAL = fpu.o\
		thrdpool_test.o\
		fax_decode.o

OBJS_EXTERNAL = ../../libx/stdform.o\
		../../libx/G711.o\
		../../libx/std_audio.o\
		../../libx/mixer.o

CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
		-I ../include\
		-I ../../include\
		-I ../../libx

.PHONY: $TARGET clean

fpu: $(TARGET)
$(TARGET): $(OBJS_LOCAL) $(OBJS_EXTERNAL) 
	$(COMPILE) -o $@ $(OBJS_LOCAL) $(OBJS_EXTERNAL) $(LIBS)

$(OBJS_LOCAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@

$(OBJS_EXTERNAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@
clean:
	rm -rf $(OBJS_LOCAL) $(OBJS_EXTERNAL) $(TARGET) 



