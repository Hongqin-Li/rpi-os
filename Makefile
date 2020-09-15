# GNU Makefile doc: https://www.gnu.org/software/make/manual/html_node/index.html

# -fno-pie -fno-pic remove .got and data.rel sections, `objdump -t obj/kernel.o | sort` to see the difference
# -MMD -MP: generate .d files
# -Wl,--build-id=none: remove .note.gnu.build-id section, making the multiboot header in first 4KB
# -fno-omit-frame-pointer: make sure that %ebp is saved on stack, which can be used for tracing
# -mgeneral-regs-only: Use only general-purpose registers. ARM processors also have NEON registers. We don’t want the compiler to use them because they add additional complexity (since, for example, we will need to store the registers during a context switch).
# -nostartfiles: Don’t use standard startup files. Startup files are responsible for setting an initial stack pointer, initializing static data, and jumping to the main entry point. We are going to do all of this by ourselves.

CROSS := aarch64-linux-gnu
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump
OBJCOPY := $(CROSS)-objcopy

CC += -Wall -g
CC += -fno-pie -fno-pic -fno-stack-protector
CC += -static -fno-builtin -nostdlib -ffreestanding -nostartfiles
CC += -fno-omit-frame-pointer
CC += -Wl,--build-id=none
CC += -mgeneral-regs-only

# CC += -O2

SRC_DIRS := kern
BUILD_DIR = obj

KERN_ELF := $(BUILD_DIR)/kernel8.elf
IMG := $(BUILD_DIR)/kernel8.img

all: $(IMG)

# Automatically find sources and headers
SRCS := $(shell find $(SRC_DIRS) -name *.c -or -name *.S)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

CC += -Iinc -MMD -MP

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $<
$(BUILD_DIR)/%.S.o: %.S
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $<

$(KERN_ELF): kern/linker.ld $(OBJS)
	$(LD) -o $@ -T $< $(OBJS)
	$(OBJDUMP) -S -D $@ > $(basename $@).asm
	$(OBJDUMP) -x $@ > $(basename $@).hdr

$(IMG): $(KERN_ELF)
	$(OBJCOPY) -O binary $< $@

QEMU := qemu-system-aarch64 -M raspi3 -nographic -serial null -chardev stdio,id=uart1 -serial chardev:uart1 -monitor none

qemu: $(IMG) 
	$(QEMU) -kernel $<
qemu-gdb: $(IMG)
	$(QEMU) -kernel $< -S -gdb tcp::1234
gdb: 
	gdb-multiarch -n -x .gdbinit

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
