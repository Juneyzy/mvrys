

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_$(d) :=\
			$(OBJ_DIR)/package.o\
			$(OBJ_DIR)/web_srv.o\
			$(OBJ_DIR)/web_json.o

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -g -O3\
								-DSEMP_PLATFORM=${SEMP_PLATFORM} -DRELEASED=${RELEASED}\
								-I$(d)\
								-I$(d)/../../cluster\
								-I$(d)/../../lib\
								-I$(d)/../../components/json\
								-I$(d)/../../rt

#-I$(d)/../rpc\
#-I$(d)/../cluster\

#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d))

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

-include $(DEPS_$(d))

#  standard component Makefile footer

d   := $(dirstack_$(sp))
sp  := $(basename $(sp))
