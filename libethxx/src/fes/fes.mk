

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_$(d) :=\
            $(OBJ_DIR)/auth.o\
            $(OBJ_DIR)/capture.o\
            $(OBJ_DIR)/routers.o\
            $(OBJ_DIR)/routers_ops.o\
            $(OBJ_DIR)/rt_ethxx_trapper.o\
            $(OBJ_DIR)/rt_ethxx_capture.o\
            $(OBJ_DIR)/rt_ethxx_pcap.o\
            $(OBJ_DIR)/rt_ethxx_parser.o\
            $(OBJ_DIR)/rt_ethxx_reporter.o\
            $(OBJ_DIR)/session.o
		

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
								-I$(d)\
								-I$(d)/../lib\
								-I$(d)/../rpc\
								-I$(d)/../cluster\
								-I$(d)/../components/yaml


#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d))

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

-include $(DEPS_$(d))

#  standard component Makefile footer

d   := $(dirstack_$(sp))
sp  := $(basename $(sp))
