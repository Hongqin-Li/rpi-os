# Raspberry Pi 3 Operating System

Yet another unix-like toy operating system running on Raspberry Pi 3, which is built when I was preparing [labs](https://github.com/FDUCSLG/OS-2020Fall-Fudan/) for operating system course at Fudan University, following the classic framework of [xv6](https://github.com/mit-pdos/xv6-public/).

## Related works

- [linux](https://github.com/raspberrypi/linux): real world operating system
- [circle](https://github.com/rsta2/circle): contains lots of portable drivers
- [s-matyukevich's](https://github.com/s-matyukevich/raspberry-pi-os)
- [bztsrc's](https://github.com/bztsrc/raspi3-tutorial)

## What's different?

- We use [musl](https://musl.libc.org/) as user programs libc instead of reinventing one.
- The set of syscalls supported by our kernel is a subset of linux's.
- Compared to xv6, we use a queue-based scheduler and hash pid for process sleeping and waking.

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

