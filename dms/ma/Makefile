TARGET = ma

COMPILE = gcc
CPPCOMPILE = g++

LIBS += -L ../../lib -lethxx
LIBS += -ldl -ljson -lpthread -lm -lpcap -lyaml -lpcre -lstdc++

OBJS_LOCAL =ma.o\


CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
		-I ../include\
		-I ../../libx\
		-I ../../include

.PHONY: $TARGET clean

vpw: $(TARGET)
$(TARGET): $(OBJS_LOCAL) 
	$(COMPILE) -o $@ $(OBJS_LOCAL) $(LIBS)

$(OBJS_LOCAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@

clean:
	rm -rf $(OBJS_LOCAL) $(TARGET) 



