/* See https://github.com/raspberrypi/firmware/wiki. */
#include "peripherals/mbox.h"
#include "peripherals/base.h"

#include "arm.h"
#include "console.h"
#include "memlayout.h"
#include "string.h"

#define VIDEOCORE_MBOX  (MMIO_BASE + 0x0000B880)
#define MBOX_READ       ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x00))
#define MBOX_POLL       ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x10))
#define MBOX_SENDER     ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x14))
#define MBOX_STATUS     ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x18))
#define MBOX_CONFIG     ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x1C))
#define MBOX_WRITE      ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x20))
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000

#define MBOX_RESP_OK    0x80000000
#define MBOX_RESP_ERR   0x80000001

#define MBOX_TAG_GET_ARM_MEMORY     0x00010005
#define MBOX_TAG_GET_CLOCK_RATE     0x00030002
#define MBOX_TAG_SET_SDHOST_CLOCK   0x00038042
#define MBOX_TAG_END                0x0
#define MBOX_TAG_REQUEST            0x0

#define MBOX_CLOCK_EMMC     0x1
#define MBOX_CLOCK_UART     0x2
#define MBOX_CLOCK_ARM      0x3
#define MBOX_CLOCK_CORE     0x4
#define MBOX_CLOCK_EMMC2    0xC

static int
mbox_read(uint8_t chan)
{
    while (1) {
        disb();
        while (*MBOX_STATUS & MBOX_EMPTY) ;
        disb();
        uint32_t r = *MBOX_READ;
        if ((r & 0xF) == chan) {
            return r >> 4;
        }
    }
    disb();
    return 0;
}

static void
mbox_write(uint32_t buf, uint8_t chan)
{
    disb();
    assert((buf & 0xF) == 0 && (chan & ~0xF) == 0);
    while (*MBOX_STATUS & MBOX_FULL) ;
    disb();
    *MBOX_WRITE = (buf & ~0xF) | chan;
    disb();
}

static int
mbox_send(volatile uint32_t buf[], int len)
{
    assert(len >= 3);
    disb();
    dccivac((void *)buf, len);
    disb();
    mbox_write(V2P(buf), 8);
    disb();
    mbox_read(8);
    disb();
    dccivac((void *)buf, len);
    disb();

    if (buf[1] == MBOX_RESP_OK) {
        return 0;
    } else if (buf[1] == MBOX_RESP_ERR) {
        debug("error parsing request buffer(partial response)");
        return -1;
    } else {
        error("invalid mbox resp");
        return -1;
    }
}

int
mbox_get_arm_memory()
{
    __attribute__((aligned(16)))
    volatile uint32_t buf[] = {32, 0, MBOX_TAG_GET_ARM_MEMORY, 8, MBOX_TAG_REQUEST, 0, 0, MBOX_TAG_END};

    asserts((V2P(buf) & 0xF) == 0, "Buffer should align to 16 bytes. ");
    assert(sizeof(buf) == buf[0]);

    if (mbox_send(buf, sizeof(buf)) < 0)
        return -1;

    if ((buf[4] >> 31) == 0) {
        debug("unexpected tag resp %d", buf[4]);
        return -1;
    }
    assert((buf[4] & 0x3FFFFFFF) == 8);

    asserts(buf[5] == 0, "Memory base address should be zero. ");
    asserts(buf[6] != 0, "Memory size should not be zero. ");
    return buf[6];
}

int
mbox_get_clock_rate(int clock_id)
{
    __attribute__((aligned(16)))
    volatile uint32_t buf[] = {32, 0, MBOX_TAG_GET_CLOCK_RATE, 8, MBOX_TAG_REQUEST, clock_id, 0, MBOX_TAG_END};
    asserts((V2P(buf) & 0xF) == 0, "Buffer should align to 16 bytes. ");
    assert(sizeof(buf) == buf[0]);
   
    if (mbox_send(buf, sizeof(buf)) < 0)
        return -1;

    if ((buf[4] >> 31) == 0) {
        debug("unexpected tag resp %d", buf[4]);
        return -1;
    }
    assert((buf[4] & 0x3FFFFFFF) == 8);
    return buf[6];
}

/*
 * Set clock state of sdhost, undocumented.
 * Return -1 if failed.
 */
int
mbox_set_sdhost_clock(uint32_t msg[3])
{
    __attribute__((aligned(16)))
    volatile uint32_t buf[] = {0, 0, MBOX_TAG_SET_SDHOST_CLOCK, 12, MBOX_TAG_REQUEST, msg[0], msg[1], msg[2], MBOX_TAG_END};
    buf[0] = sizeof(buf);
   
    if (mbox_send(buf, sizeof(buf)) < 0)
        return -1;
    
    memmove(msg, &buf[5], sizeof(msg));
    if ((buf[4] >> 31) == 0 || (buf[4] & 0x3FFFFFFF) != 12) {
        debug("unexpected tag resp %d", buf[4]);
        // qemu always failed
        return 0;
    }
    return 0;
}

int
mbox_set_clock_rate(int clock_id)
{
    // TODO:

}

void
mbox_test()
{
#ifdef DEBUG
    int x = mbox_get_arm_memory();
    assert(x != 0);
    x = mbox_get_clock_rate(MBOX_CLOCK_EMMC);
    assert(x != 0);
    info("pass");
#endif
}
