

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

LIBRARY := $(OBJ_DIR)/librecv.a
#  component specification

OBJS_$(d) :=\
            $(OBJ_DIR)/rt_yaml.o\
            $(OBJ_DIR)/rt_mq.o\
            $(OBJ_DIR)/rt_stdlib.o\
            $(OBJ_DIR)/rt_rbtree.o\
            $(OBJ_DIR)/rt_socket.o\
            $(OBJ_DIR)/rt_sync.o\
            $(OBJ_DIR)/rt_task.o\
            $(OBJ_DIR)/rt_stack.o\
            $(OBJ_DIR)/rt_string.o\
            $(OBJ_DIR)/rt_errno.o\
            $(OBJ_DIR)/rt_file.o\
            $(OBJ_DIR)/rt_time.o\
            $(OBJ_DIR)/rt_list.o\
            $(OBJ_DIR)/rt_logging.o\
            $(OBJ_DIR)/rt_logging_filters.o\
            $(OBJ_DIR)/rt_enum.o\
            $(OBJ_DIR)/rt_util.o\
            $(OBJ_DIR)/rt_hash.o\
            $(OBJ_DIR)/rt_pool.o\
            $(OBJ_DIR)/rt_mempool.o\
            $(OBJ_DIR)/rt_signal.o\
            $(OBJ_DIR)/rt_named_memory.o\
            $(OBJ_DIR)/rt_thrdpool.o\
            $(OBJ_DIR)/rt_tmr.o\
            $(OBJ_DIR)/rt_timer.o
		

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3 -D_GNU_SOURCE -D__USE_XOPEN\
                                -I$(d)\
                                -I$(d)/../components/yaml\
                                -I$(d)/../components/command-line-engine


#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

#LIBS_LIST   :=  $(LIBS_LIST) $(LIBRARY)
LIBS_LIST   :=  $(LIBS_LIST)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d))

-include $(DEPS_$(d))

#$(LIBRARY): $(OBJS)
#    $(MYARCHIVE)

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

#  standard component Makefile footer

d   := $(dirstack_$(sp))
sp  := $(basename $(sp))
