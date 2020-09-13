#ifndef INC_PERIPHERALS_MBOX_H
#define INC_PERIPHERALS_MBOX_H

#include "peripherals/base.h"

#define MBOX_BASE     (MMIO_BASE + 0xB880)
#define MBOX_READ     (MBOX_BASE + 0x00)
#define MBOX_STATUS   (MBOX_BASE + 0x18)
#define MBOX_WRITE    (MBOX_BASE + 0x20)

#endif  /* !INC_PERIPHERALS_MBOX_H */
