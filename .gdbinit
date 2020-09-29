set architecture aarch64
file obj/kernel8.elf
target remote localhost:1234

# Modify the following path to support pwndbg
source /mnt/d/Workspace/github/pwndbg/gdbinit.py
