CROSS := aarch64-linux-gnu
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump
OBJCOPY := $(CROSS)-objcopy

CFLAGS := -Wall -g -O2 \
          -fno-pie -fno-pic -fno-stack-protector \
          -fno-zero-initialized-in-bss \
          -static -fno-builtin -nostdlib -ffreestanding -nostartfiles \
          -mgeneral-regs-only \
          -MMD -MP \

CFLAGS += -Iinc
SRC_DIRS := kern
BUILD_DIR = obj

KERN_ELF := $(BUILD_DIR)/kernel8.elf
KERN_IMG := $(BUILD_DIR)/kernel8.img
SD_IMG := $(BUILD_DIR)/sd.img

all: $(KERN_IMG) $(SD_IMG)

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

qemu: $(KERN_IMG)
	$(QEMU) -kernel $<
qemu-gdb: $(KERN_IMG)
	$(QEMU) -kernel $< -S -gdb tcp::1234
gdb: 
	gdb-multiarch -n -x .gdbinit

clean:
	rm -r $(BUILD_DIR)

.PHONY: all clean qemu qemu-gdb gdb
