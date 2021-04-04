#ifndef INC_PERIPHERALS_MBOX_H
#define INC_PERIPHERALS_MBOX_H

#include "peripherals/base.h"

#include "types.h"

#define MBOX_CLOCK_EMMC     0x1
#define MBOX_CLOCK_UART     0x2
#define MBOX_CLOCK_ARM      0x3
#define MBOX_CLOCK_CORE     0x4
#define MBOX_CLOCK_EMMC2    0xC

int mbox_get_arm_memory();
int mbox_get_clock_rate(int);
int mbox_set_sdhost_clock(uint32_t msg[3]);

#endif
