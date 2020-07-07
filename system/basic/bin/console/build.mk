CONSOLE_OBJS = $(ROOT_DIR)/bin/console/console.o

CONSOLE = $(TARGET_DIR)/$(ROOT_DIR)/bin/console

PROGS += $(CONSOLE)
CLEAN += $(CONSOLE_OBJS)

$(CONSOLE): $(CONSOLE_OBJS)
	$(LD) -Ttext=100 $(CONSOLE_OBJS) -o $(CONSOLE) $(LDFLAGS) -lewokc -lc -lgraph -lconsole
