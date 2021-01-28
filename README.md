# Raspberry Pi 3 Operating System

This is yet another unix-like toy operating system on raspberry pi 3. I built it for preparing lecture [labs](https://github.com/FDUCSLG/OS-2020Fall-Fudan/) when I was a TA of OS lecture at Fudan University. However, operating systems on raspberry pi seems either too complicate (e.g. [linux](https://github.com/raspberrypi/linux), [circle](https://github.com/rsta2/circle)) or incomplete (e.g. [s-matyukevich's](https://github.com/s-matyukevich/raspberry-pi-os) and [bztsrc's](https://github.com/bztsrc/raspi3-tutorial) both lack multi-core support) for teaching. Inappropriate though they are, it's still highly recommended to have a try with them when working on raspberry pi with its poorly-documented hardware. Eventually, I decided to implement it from scratch, with my limited knowledge about [xv6](https://github.com/mit-pdos/xv6-public/), which is one of the best teaching operating system by MIT. Since xv6 is the first os I have learned, the general framework is adopted from it with a little optimization.

Several parts of it are still in progress.

## Features

- [x] Multi-core
- [x] Memory management
- [x] Virtual memory
- [ ] Process management
- [ ] Disk Driver (EMMC)
- [ ] File system
- [x] C library: port [musl](https://musl.libc.org/)
- [ ] Compiler
- [ ] Documentation...

FS TODO:
- [ ] mkfs
- [x] sd
- [x] bio
- [x] log
- [ ] inode(fs.c)
- [ ] file
- [ ] mkfs

## Development

Linux is required for building and Ubuntu is preferred.

- Install toolchains and libc by `make init` on Ubuntu 18.04 or higher.
- Emulate the kernel at `obj/kernel8.img` by `make qemu`.
- Create a bootable sd card image at `obj/sd.img` for rpi3 by `make`.
- Burn to a tf card using [Raspberry Pi Imager](https://www.raspberrypi.org/software/).

## Project structure

