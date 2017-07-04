TARGET = delete

COMPILE = gcc

LIBS += -ldl -lpthread 

OBJS_LOCAL = delete.o\

CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\

$(TARGET): $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) 
	$(COMPILE) -o $@ $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(LIBS)

$(OBJS_LOCAL): %.o : %.c
	$(COMPILE) $(CFLAGS_LOCAL) -c $< -o $@

clean:
	rm -rf $(OBJS_LOCAL) $(CPP_OBJS_LOCAL) $(TARGET) 



