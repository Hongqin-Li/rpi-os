RASPI := 3
ARCH := aarch64
CROSS := aarch64-linux-gnu-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy

CFLAGS := -Wall -g -O2 \
          -fno-pie -fno-pic -fno-stack-protector \
          -fno-zero-initialized-in-bss \
          -static -fno-builtin -nostdlib -nostdinc -ffreestanding -nostartfiles \
          -mgeneral-regs-only \
          -MMD -MP \
		  -Iinc -Ilibc/obj/include -Ilibc/arch/aarch64 -Ilibc/include -Ilibc/arch/generic

CFLAGS += -DNOT_DEBUG -DLOG_DEBUG -DRASPI=$(RASPI)

CFLAGS += -mlittle-endian -mcmodel=small -mno-outline-atomics

ifeq ($(strip $(RASPI)), 3)
CFLAGS += -mcpu=cortex-a53 -mtune=cortex-a53
else ifeq ($(strip $(RASPI)), 4)
CFLAGS += -mcpu=cortex-a72
else
$(error RASPI must be set to 3 or 4)
endif

SRC_DIRS := kern
BUILD_DIR = obj

KERN_ELF := $(BUILD_DIR)/kernel8.elf
KERN_IMG := $(BUILD_DIR)/kernel8.img
SD_IMG := $(BUILD_DIR)/sd.img

all:
	$(MAKE) -C boot
	$(MAKE) -C usr
	$(MAKE) $(SD_IMG)

# Automatically find sources and headers
SRCS := $(shell find $(SRC_DIRS) -name *.c -or -name *.S)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_DIR)/%.S.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERN_ELF): kern/linker.ld $(OBJS)
	$(LD) -o $@ -T $< $(OBJS)
	$(OBJDUMP) -S -d $@ > $(basename $@).asm
	$(OBJDUMP) -x $@ > $(basename $@).hdr

$(KERN_IMG): $(KERN_ELF)
	$(OBJCOPY) -O binary $< $@

-include mksd.mk

QEMU := qemu-system-aarch64 -M raspi3 -nographic -serial null -serial mon:stdio -drive file=$(SD_IMG),if=sd,format=raw

qemu: all
	$(QEMU) -kernel $(KERN_IMG)
qemu-gdb: all
	$(QEMU) -kernel $(KERN_IMG) -S -gdb tcp::1234
gdb: 
	gdb-multiarch -n -x .gdbinit

init:
	sudo apt install -y gcc-aarch64-linux-gnu gdb-multiarch
	sudo apt install -y qemu-system-arm qemu-efi-aarch64 qemu-utils
	sudo apt install -y mtools
	sudo apt install -y indent
	git submodule update --init --recursive
	(cd libc && export CROSS_COMPILE=$(CROSS) && ./configure --target=$(ARCH))

LINT_SRC := $(shell find $(SRC_DIRS) usr -name *.c)
LINT_TMP := $(LINT_SRC:%=%~)
lint:
	indent -kr -psl -ss -nut -ncs $(LINT_SRC)
	rm $(LINT_TMP)

clean:
	$(MAKE) -C usr clean
	# $(MAKE) -C libc clean
	# $(MAKE) -C boot clean
	rm -rf $(BUILD_DIR)

.PHONY: init all lint clean qemu qemu-gdb gdb
