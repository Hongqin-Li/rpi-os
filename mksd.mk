BOOT_IMG := $(BUILD_DIR)/boot.img
FS_IMG := $(BUILD_DIR)/fs.img

SECTOR_SIZE := 512

SECTORS := 256*1024
BOOT_OFFSET := 2048
BOOT_SECTORS= 128*1024
FS_OFFSET := $$(($(BOOT_OFFSET)+$(BOOT_SECTORS)))
FS_SECTORS := $$(($(SECTORS)-$(FS_OFFSET)))

.DELETE_ON_ERROR: $(BOOT_IMG) $(SD_IMG)

$(BOOT_IMG): $(KERN_IMG) boot/bootcode.bin boot/start.elf boot/config.txt
	dd if=/dev/zero of=$@ seek=$$(($(BOOT_SECTORS) - 1)) bs=$(SECTOR_SIZE) count=1
	# -F 32 specify FAT32
	# -s 1 specify one sector per cluster so that we can create a smaller one
	mkfs.vfat -F 32 -s 1 $@
	# Install mtools by `sudo apt install mtools`
	# and copy files into boot partition
	mcopy -i $@ boot/bootcode.bin ::bootcode.bin
	mcopy -i $@ boot/start.elf ::start.elf
	mcopy -i $@ boot/config.txt ::config.txt
	mcopy -i $@ $< ::$(notdir $<)

$(SD_IMG): $(BOOT_IMG)
	dd if=/dev/zero of=$@ seek=$$(($(SECTORS) - 1)) bs=$(SECTOR_SIZE) count=1
	printf "                                                                \
	  $(BOOT_OFFSET), $$(($(BOOT_SECTORS)*$(SECTOR_SIZE)/1024))K, c,\n      \
	  $(FS_OFFSET), $$(($(FS_SECTORS)*$(SECTOR_SIZE)/1024))K, L,\n          \
	" | sfdisk $@
	dd if=$(BOOT_IMG) of=$@ seek=$(BOOT_OFFSET) conv=notrunc
