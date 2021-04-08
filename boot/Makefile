
# Use firmware revision built/committed on: Nov 25 2020
HASH ?= e15ef4e4fe8be99cd816cec901d977224a1eb07e
BASE_URL := https://github.com/raspberrypi/firmware/blob/$(HASH)/boot

FIRMWARE := \
	LICENCE.broadcom \
	COPYING.linux \
	bootcode.bin \
	fixup.dat \
	fixup_cd.dat \
	start.elf \
	start_cd.elf \
	fixup4.dat \
	fixup4cd.dat \
	start4.elf \
	start4cd.elf \

all: $(FIRMWARE)

%:
	wget -q -O $@ $(BASE_URL)/$*?raw=true

clean:
	rm -f $(FIRMWARE)

.DELETE_ON_ERROR: $(FIRMWARE)
.PHONY: all clean
