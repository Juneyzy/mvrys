#  target to create object directory

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

#  applications object suffix rule

$(OBJ_DIR)/%.o: %.c
	$(COMPILE)

$(OBJ_DIR)/%.o: %.S
	$(ASSEMBLE)

#  application config check and rules

-include $(OBJS:.o=.d)

application-target: $(TARGET)


$(TARGET): $(OBJ_DIR) $(OBJS) $(LIBS_LIST)
	$(CC) $(OBJS) $(LDFLAGS_PATH) $(LIBS_LIST) $(LDFLAGS_GLOBAL) -o $@

$(TARGET).stp: $(TARGET)
	$(STRIP) -o $(TARGET).stp $(TARGET)

