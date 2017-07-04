

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_$(d) :=\
			$(OBJ_DIR)/rt.o\
			$(OBJ_DIR)/rt_json.o\
            $(OBJ_DIR)/rt_throughput.o\
            $(OBJ_DIR)/rt_throughput_proto.o\
            $(OBJ_DIR)/rt_throughput_message.o\
            $(OBJ_DIR)/rt_throughput_sum.o\
            $(OBJ_DIR)/rt_throughput_exce.o\
            $(OBJ_DIR)/rt_throughput_proto_enum.o\
            $(OBJ_DIR)/rt_inotify.o\
            $(OBJ_DIR)/oracle_client.o\
            $(OBJ_DIR)/rt_clue.o
		

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
                                -I$(d)\
                                -I$(d)/../cluster\
                                -I$(d)/../rpc\
                                -I$(d)/../components/json\
                                -I$(d)/../components/inotify\
                                -I$(d)/../components/yaml\
                                -I$(d)/../bes/json_package\
                                -I$(d)/../lib 


#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d))

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

-include $(DEPS_$(d))

#  standard component Makefile footer

d   := $(dirstack_$(sp))
sp  := $(basename $(sp))
