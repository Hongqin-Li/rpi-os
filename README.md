# Raspberry Pi 3 Operating System

Yet another unix-like toy operating system running on Raspberry Pi 3, which is built when I was preparing [labs](https://github.com/FDUCSLG/OS-2020Fall-Fudan/) for operating system course at Fudan University. However, existing operating systems on raspberry pi seems either too complicate (e.g. [linux](https://github.com/raspberrypi/linux), [circle](https://github.com/rsta2/circle)) or incomplete (e.g. [s-matyukevich's](https://github.com/s-matyukevich/raspberry-pi-os) and [bztsrc's](https://github.com/bztsrc/raspi3-tutorial) both lack multi-core support) for teaching. But they are still good projects, and it's highly recommended to have a try with them. Eventually, I decided to implement one from scratch, following the classic framework of [xv6](https://github.com/mit-pdos/xv6-public/).

## Features

- [x] Multi-core
- [x] Memory management
- [x] Virtual memory
- [x] Process management
- [x] Disk driver(EMMC): port [RPiHaribote](https://github.com/moizumi99/RPiHaribote/blob/master/sdcard.c)
- [x] File system: port xv6
- [x] C library: port [musl](https://musl.libc.org/)
- [x] Shell: port xv6
  - [x] Support argc, envp
  - [x] Support pipe
- [ ] Documentation...

## Development

Linux is required for building and Ubuntu is preferred.

- Install toolchains and libc by `make init` on Ubuntu 18.04 or higher.
- Emulate the kernel at `obj/kernel8.img` by `make qemu`.
- Create a bootable sd card image at `obj/sd.img` for rpi3 by `make`.
- Burn to a tf card using [Raspberry Pi Imager](https://www.raspberrypi.org/software/).

## Project structure

```
.
├── Makefile
├── mksd.mk: Part of Makefile for generating bootable image.
|
├── boot: Official boot loader.
├── libc: C library musl.
|
├── inc: Kernel headers.
├── kern: Kernel source code.
└── usr: User programs.
```

