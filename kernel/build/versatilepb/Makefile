# tools
AR = arm-none-eabi-ar
AS = arm-none-eabi-as
CC = arm-none-eabi-gcc
CXX = arm-none-eabi-c++
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump

export QEMU_AUDIO_DRV=none

ARCH_DIR=$(SRC_DIR)/basic/arch/sys
BSP_DIR=$(SRC_DIR)/basic/arch/bsp
ARCH=arm/v6
BSP=versatilepb

SYS_FULL_DIR = ../../../system
SRC_DIR=../..
BUILD_DIR = .

include config.mk

# flags

ifeq ($(DEBUG),yes)
OPTM = -g
else
OPTM = -O2
endif

CFLAGS += $(OPTM) \
	-I. -I$(SRC_DIR)/basic/lib/include \
	-I$(SRC_DIR)/basic/kernel/include \
	-I$(SRC_DIR)/basic/dev/include \
	-I$(SRC_DIR)/extra/dev/include \
	-I$(SRC_DIR)/extra/lib/include \
	-I$(ARCH_DIR)/$(ARCH) \
	-I$(BSP_DIR)/lib/bcm283x/include \
	-I$(ARCH_DIR)/arm/common/include \
	-marm $(ARCH_CFLAGS) \
	-Wstrict-prototypes \
	-pedantic -Wall -Wextra -msoft-float -fPIC -mapcs-frame \
	-fno-builtin-printf \
	-fno-builtin-strcpy \
	-Wno-overlength-strings \
	-fno-builtin-exit \
	-fno-builtin-stdio \
	-fno-builtin-memset \
	-fno-builtin-memcpy \
	-fno-builtin-strchr \
	-fno-builtin-strcmp \
	-fno-builtin-strlen \
	-fno-builtin-strncpy \
	-fno-builtin-strncmp \
	-std=c99

ARCH_OBJS = \
	$(ARCH_DIR)/$(ARCH)/boot.o \
	$(ARCH_DIR)/$(ARCH)/start.o \
	$(ARCH_DIR)/$(ARCH)/system.o \
	$(ARCH_DIR)/$(ARCH)/mmu_pte_flags.o \
	$(BSP_DIR)/$(BSP)/hw_info.o \
	$(BSP_DIR)/$(BSP)/irq.o \
	$(BSP_DIR)/$(BSP)/timer.o \
	$(BSP_DIR)/$(BSP)/sd.o \
	$(BSP_DIR)/$(BSP)/uart.o \
	$(ARCH_DIR)/arm/common/src/interrupt.o \
	$(ARCH_DIR)/arm/common/src/memcpy.o \
	$(ARCH_DIR)/arm/common/src/mmu_arch.o \
	$(BSP_DIR)/lib/bcm283x/src/gpio.o  \
	$(BSP_DIR)/lib/bcm283x/src/sd.o  \
	$(BSP_DIR)/lib/bcm283x/src/mailbox.o \
	$(BSP_DIR)/lib/bcm283x/src/pl011_uart.o \
	$(BSP_DIR)/lib/bcm283x/src/mini_uart.o \
	$(BSP_DIR)/lib/bcm283x/src/gpio_actled.o 

QEMU_CMD = qemu-system-arm
OS_IMG = kernel.img
OS_ELF = kernel.elf

LIB_DIR = $(SRC_DIR)/basic/lib/src

LIB_OBJS = \
	$(LIB_DIR)/kstring.o \
	$(LIB_DIR)/vprintf.o \
	$(LIB_DIR)/proto.o \
	$(LIB_DIR)/basic_math.o \
	$(LIB_DIR)/queue.o \
	$(LIB_DIR)/kprintf.o

ifeq ($(SYS_LOAD_MODE), sd)
LOAD_DIR=$(SRC_DIR)/loadinit/sd
LOAD_OBJS = \
	$(LOAD_DIR)/ext2read.o \
	$(LOAD_DIR)/loadinit.o
CFLAGS += -I$(LOAD_DIR) -DLOAD_SD
QEMU_ARGS += -sd $(SYS_FULL_DIR)/build/sd.ext2
else
LOAD_DIR=$(SRC_DIR)/loadinit/romfs
LOAD_OBJS = \
	$(LOAD_DIR)/romfs_data.o \
	$(LOAD_DIR)/romfs.o \
	$(LOAD_DIR)/loadinit.o
CFLAGS += -I$(LOAD_DIR)
endif

DEV_DIR = $(SRC_DIR)/basic/dev/src
DEV_OBJS = \
	$(DEV_DIR)/dev.o 

KERNEL_DIR = $(SRC_DIR)/basic/kernel/src
KERNEL_OBJS = \
	$(KERNEL_DIR)/mm/kalloc.o \
	$(KERNEL_DIR)/mm/trunkmem.o \
	$(KERNEL_DIR)/mm/kmalloc.o \
	$(KERNEL_DIR)/mm/shm.o \
	$(KERNEL_DIR)/mm/mmu.o \
	$(KERNEL_DIR)/irq.o \
	$(KERNEL_DIR)/svc.o \
	$(KERNEL_DIR)/proc.o \
	$(KERNEL_DIR)/ipc.o \
	$(KERNEL_DIR)/signal.o \
	$(KERNEL_DIR)/schedule.o \
	$(KERNEL_DIR)/systemc.o \
	$(KERNEL_DIR)/kevqueue.o \
	$(KERNEL_DIR)/kernel.o

OBJS = $(ARCH_OBJS) \
	$(LIB_OBJS) \
	$(LOAD_OBJS) \
	$(KERNEL_OBJS) \
	$(DEV_OBJS) 

LDS = mkos.lds.S
LDFLAGS = -L $(ARCH_DIR)/$(ARCH)

ASFLAGS = $(OPTM) -I. -I$(SRC_DIR)/basic/include -marm $(ARCH_CFLAGS)

all: $(OS_IMG) 

$(OS_IMG): $(OBJS) $(LDS)
	$(LD) $(OBJS) $(LDFLAGS) -T $(LDS) -o $(BUILD_DIR)/$(OS_ELF)
	$(OBJCOPY) -O binary $(BUILD_DIR)/$(OS_ELF) $(BUILD_DIR)/$(OS_IMG)

asm:
	$(OBJDUMP) -D $(BUILD_DIR)/$(OS_ELF) > $(BUILD_DIR)/kernel.asm

run: 
	$(QEMU_CMD) $(QEMU_FLAGS) -kernel $(BUILD_DIR)/$(OS_IMG) $(QEMU_ARGS)

runasm: 
	$(QEMU_CMD) $(QEMU_FLAGS) -kernel $(BUILD_DIR)/$(OS_IMG) $(QEMU_ARGS) -d in_asm

debug:
	$(QEMU_CMD) $(QEMU_FLAGS) -gdb tcp::26000 -S -kernel $(BUILD_DIR)/$(OS_IMG) $(QEMU_ARGS)

debugasm:
	$(QEMU_CMD) $(QEMU_FLAGS) -d in_asm -gdb tcp::26000 -S -kernel $(BUILD_DIR)/$(OS_IMG)

gdb: 
	echo "target remote :26000" > /tmp/gdbinit
	arm-none-eabi-gdb $(BUILD_DIR)/$(OS_ELF) -x /tmp/gdbinit
	#gdb-multiarch $(BUILD_DIR)/$(OS_ELF) -x /tmp/gdbinit #for ubuntu 18.04 without arm-none-eabi-gdb

clean:
	rm -f $(OBJS)
	rm -f $(OS_IMG) $(OS_ELF)
