

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_RPC_PROBE :=\
                 $(OBJ_DIR)/rpc_probe.o

OBJS_JSON_DECODER :=\
                    $(OBJ_DIR)/json_decoder.o
                    
OBJS_$(d) :=\
			$(OBJ_DIR)/cluster_probe.o\
			$(OBJ_DIR)/cluster_decoder.o\
            $(OBJS_RPC_PROBE)\
            $(OBJS_JSON_DECODER)
		

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -W -Wall -Wunused-parameter -g -O3\
								-I$(d)\
                                -I$(d)/../rt\
								-I$(d)/../lib\
								-I$(d)/../rpc\
								-I$(d)/../components/json\
								-I$(d)/../components/yaml\
								-I$(d)/../components/command-line-engine\


#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d))

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

-include $(DEPS_$(d))

#  standard component Makefile footer

d   := $(dirstack_$(sp))
sp  := $(basename $(sp))
