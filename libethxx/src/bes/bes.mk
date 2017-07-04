

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_$(d) :=\
            $(OBJ_DIR)/ui.o\
            $(OBJ_DIR)/ui_a29.o\
            $(OBJ_DIR)/ui_local.o\
            $(OBJS_MA)
		

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -W -DSPASR_BRANCH_TARGET=$(BRANCH_TARGET) -Wall -Wunused-parameter -g -O3\
								-I$(d)\
								-I$(d)/../lib\
								-I$(d)/../cluster\
								-I$(d)/../fes\
								-I$(d)/../bes/json_package\
								-I$(d)/../components/json\
								-I$(d)/../components/command-line-engine\
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
