#ifndef INC_BSP_BASE_H
#define INC_BSP_BASE_H

#include "memlayout.h"

#if RASPI == 3
#define MMIO_BASE   (KERNBASE + 0x3F000000)
#elif RASPI == 4
#define MMIO_BASE   (KERNBASE + 0xFE000000)
#endif

#define LOCAL_BASE  (MMIO_BASE + 0x01000000)

#endif
