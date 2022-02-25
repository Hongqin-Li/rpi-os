# Raspberry Pi Operating System

Yet another unix-like toy operating system running on Raspberry Pi 3/4, which is built when I was preparing [labs](https://github.com/FDUCSLG/OS-2020Fall-Fudan/) for operating system course at Fudan University, following the classic framework of [xv6](https://github.com/mit-pdos/xv6-public/).

Tested on Raspberry Pi 3A+, 3B+, 4B.

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

- [x] AArch64 only
- [x] Basic multi-core support
- [x] Memory management
- [x] Virtual memory without swapping
- [x] Process management
- [x] Disk driver(EMMC): ported from [circle](https://github.com/rsta2/circle/tree/master/addon/SDCard)
- [x] File system: ported from xv6
- [x] C library: [musl](https://musl.libc.org/)
- [x] Shell: ported from xv6
  - [x] Support argc, envp
  - [x] Support pipe

## Prerequisite

For Ubuntu 20.04 on x86, just run `make init` and skip this section.

### GCC toolchain

If you are cross compiling on macOS,

1. run `brew install zstd`,
2. install aarch64-unknown-linux-gnu by [macos-cross-toolchains](https://github.com/messense/homebrew-macos-cross-toolchains),
3. change the prefix such as `CROSS := aarch64-unknown-linux-gnu-` in config.mk.

If you are native compiling, i.e., running natively on ARMv8, set `CROSS := ` in config.mk to use local gcc.

If `$(CROSS)gcc --version` gives out version less than 9.3.0, remove `-mno-outline-atomics` from Makefile.

### QEMU

You can install QEMU (>= 6.0) by package manager or compile it from source such as

```
git clone https://github.com/qemu/qemu.git
mkdir -p qemu/build
(cd qemu/build && ../configure --target-list=aarch64-softmmu && make -j8)
```

On some OS such as CentOS, you may also need to install the following dependencies

```
yum install ninja-build
yum install pixman-devel.aarch64
```

Then add the generated `qemu-system-aarch64` to PATH or just modify the `QEMU` variable in config.mk.

### Build musl

First, fetch musl by `git submodule update --init --recursive`.

Then if you are cross compiling, run `(cd libc && export CROSS_COMPILE=X && ./configure --target=aarch64)`, where `X` is the `CROSS` variable in config.mk. Otherwise, run `(cd libc && ./configure)`.

### File system tools

mtools is used to build our fat32 file system image, which can be installed by package manager, e.g, `sudo apt install mtools` on Ubuntu and `brew install mtools` on macOS.

sfdisk is used to create MBR-partitioned disk images. It has already been installed on most Linux distributions. But for macOS,

1. run `brew install util-linux` to install sfdisk,
2. modify the PATH according to the instructions given by brew.

## Development

- `make qemu`: Emulate the kernel at `obj/kernel8.img`.
- `make`: Create a bootable sd card image at `obj/sd.img` for Raspberry Pi 3, which can be burned to a tf card using [Raspberry Pi Imager](https://www.raspberrypi.org/software/).
- `make lint`: Lint source code of kernel and user programs.

### Raspberry Pi 4

It works on Pi 4 as well. Change `RASPI := 3` to `RASPI := 4` in Makefile, run `make clean && make` and have fun with your Pi 4.

### Logging level

Logging level is controlled via compiler option `-DLOG_XXX` in Makefile, where `XXX` can be one of

- `ERROR`
- `WARN`
- `INFO`
- `DEBUG`
- `TRACE`

Defaults to `-DLOG_INFO`.

### Debug mode

Enabling debug mode via compiler option `-DDEBUG` in Makefile will incorporate runtime assertions,
testing and memory usage profiling(see below). Defaults to `-DNOT_DEBUG`.

### Profile

We can inspect the information of processes and memory usage(shown when `-DDEBUG`) by `Ctrl+P`.
This may output something like

```
1 sleep  init
2 run    idle
3 runble idle
4 run    idle
5 run    idle
6 sleep  sh fa: 1  
```

where each row contains the pid, state, name and father's pid of each process.

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

