TARGET = vrstools

COMPILE = gcc
CPPCOMPILE = g++

LIBS += -L ../lib -lethxx -lFrontEnd_old -lSpkAPI -lwav
LIBS += -ldl -ljson -lpthread -lm -lpcap -lyaml -lpcre -lstdc++

OBJS_LOCAL = vrs_tools.o\
		../libx/vrs_rule.o\
		../libx/vrs_model.o\
		../libx/vrs_senior.o

CPP_OBJS_LOCAL =  ../libx/model.o

CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
		-I ../libx\
		-I ../include\
		-I ../vpw

.PHONY: vpw clean

vpw: $(TARGET)
$(TARGET): $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) 
	$(COMPILE) -o $@ $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(LIBS)

$(OBJS_LOCAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@

$(CPP_OBJS_LOCAL): %.o : %.cpp
	$(CPPCOMPILE) $(CFLAGS_LOCAL) -c $< -o $@

clean:
	rm -rf $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(TARGET) 



