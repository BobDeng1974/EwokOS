RASPI_TTYD_OBJS = $(ROOT_DIR)/drivers/arch/raspi/ttyd/ttyd.o

RASPI_TTYD = $(TARGET_DIR)/$(ROOT_DIR)/drivers/raspi/ttyd

PROGS += $(RASPI_TTYD)
CLEAN += $(RASPI_TTYD_OBJS)

$(RASPI_TTYD): $(RASPI_TTYD_OBJS)
	$(LD) -Ttext=100 $(RASPI_TTYD_OBJS) -o $(RASPI_TTYD) $(LDFLAGS) -lewokc -lc
