#ifndef INC_BASE_H
#define INC_BASE_H

#include "memlayout.h"

#if RASPI == 3
#define MMIO_BASE   (KERNBASE + 0x3F000000)
#define LOCAL_BASE  (KERNBASE + 0x40000000)
#elif RASPI == 4
#define MMIO_BASE   (KERNBASE + 0xFE000000)
// See 6.5.2 https://datasheets.raspberrypi.org/bcm2711/bcm2711-peripherals.pdf
#define LOCAL_BASE  (KERNBASE + 0xFF800000)
#endif


#endif
