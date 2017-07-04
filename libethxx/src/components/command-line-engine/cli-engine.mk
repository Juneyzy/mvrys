

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

OBJS_$(d) :=\
			$(OBJ_DIR)/buffer.o\
			$(OBJ_DIR)/command.o\
			$(OBJ_DIR)/filter.o\
			$(OBJ_DIR)/hash.o\
			$(OBJ_DIR)/log.o\
			$(OBJ_DIR)/memory.o\
			$(OBJ_DIR)/prefix.o\
			$(OBJ_DIR)/sockunion.o\
			$(OBJ_DIR)/thread.o\
			$(OBJ_DIR)/vector.o\
			$(OBJ_DIR)/vty.o
		

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
