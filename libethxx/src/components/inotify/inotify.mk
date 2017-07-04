

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_$(d) :=\
			$(OBJ_DIR)/inotifytools.o\
			$(OBJ_DIR)/redblack.o\
            $(OBJ_DIR)/common.o
		

CFLAGS_LOCAL += -I$(d)
$(OBJS_$(d)):  CFLAGS_LOCAL := -std=gnu99 -g -O3\
								-I$(d)


#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d))

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

-include $(DEPS_$(d))

#  standard component Makefile footer

d   := $(dirstack_$(sp))
sp  := $(basename $(sp))
