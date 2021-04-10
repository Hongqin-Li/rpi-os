
# Use firmware revision built/committed on: Nov 25 2020
# HASH ?= e15ef4e4fe8be99cd816cec901d977224a1eb07e
HASH ?= 8c7c52466505df5d420a5cb9131ec29205bcecf8
BASE_URL := https://github.com/raspberrypi/firmware/blob/$(HASH)/boot

CC8 ?=aarch64-linux-gnu-gcc
LD8 ?=aarch64-linux-gnu-ld
OBJCOPY8 ?=aarch64-linux-gnu-objcopy
OBJDUMP8 ?=aarch64-linux-gnu-objdump -maarch64

FIRMWARE := \
	bootcode.bin \
	fixup.dat \
	fixup_cd.dat \
	start.elf \
	start_cd.elf \
	fixup4.dat \
	fixup4cd.dat \
	start4.elf \
	start4cd.elf \
	LICENCE.broadcom \
	COPYING.linux
	# bcm2711-rpi-4-b.dtb \
	# bcm2711-rpi-400.dtb \
	# bcm2711-rpi-cm4.dtb \

all: $(FIRMWARE) armstub8-rpi4.bin

%8-rpi4.o: %8.S
	# $(CC8) -DGIC=1 -DBCM2711=1 -c $< -o $@
	$(CC8) -DBCM2711=1 -c $< -o $@

%8-rpi4.elf: %8-rpi4.o
	$(LD8) --section-start=.text=0 $< -o $@

%8-rpi4.tmp: %8-rpi4.elf
	$(OBJCOPY8) $< -O binary $@

%8-rpi4.bin: %8-rpi4.tmp
	dd if=$< ibs=256 of=$@ conv=sync

%:
	wget -q -O $@ $(BASE_URL)/$*?raw=true

clean:
	rm -f $(FIRMWARE)
	rm -f armstub8-rpi4.bin

.DELETE_ON_ERROR: $(FIRMWARE) armstub8-rpi4.bin
.PHONY: all clean
