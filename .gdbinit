set architecture aarch64

# add-symbol-file obj/boot/boot.o
# symbol-file obj/kernel8.elf
file obj/kernel8.elf

target remote localhost:1234
