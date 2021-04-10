//
// emmc.cpp
//
// Provides an interface to the EMMC controller and commands for interacting
// with an sd card
//
// Copyright (C) 2013 by John Cronin <jncronin@tysos.org>
//
// Modified for Circle by R. Stange
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// References:
//
// PLSS - SD Group Physical Layer Simplified Specification ver 3.00
// HCSS - SD Group Host Controller Simplified Specification ver 3.00
//
// Broadcom BCM2835 ARM Peripherals Guide
//

#include "mbox.h"
#include "base.h"
#include "sdhost.h"
#include "emmc.h"
#include "string.h"
#include "console.h"

/* External Mass Media Controller. */
#define ARM_EMMC_BASE   (MMIO_BASE + 0x300000)

/* External Mass Media Controller 2(Raspberry Pi 4 only). */
#define ARM_EMMC2_BASE  (MMIO_BASE + 0x340000)

/* Configuration options */

/*
 * According to the BCM2835 ARM Peripherals Guide the EMMC STATUS register
 * should not be used for polling. The original driver does not meet this
 * specification in this point but the modified driver should do so.
 * Define EMMC_POLL_STATUS_REG if you want the original function!
 */
//#define EMMC_POLL_STATUS_REG

// Enable 1.8V support
//#define SD_1_8V_SUPPORT

// Enable High Speed/SDR25 mode
//#define SD_HIGH_SPEED

// Enable 4-bit support
#define SD_4BIT_DATA

// SD Clock Frequencies (in Hz)
#define SD_CLOCK_ID         400000
#define SD_CLOCK_NORMAL     25000000
#define SD_CLOCK_HIGH       50000000
#define SD_CLOCK_100        100000000
#define SD_CLOCK_208        208000000

/*
 * Enable SDXC maximum performance mode.
 * Requires 150 mA power so disabled on the RPi for now.
 */
#define SDXC_MAXIMUM_PERFORMANCE

#ifndef USE_SDHOST

/* Enable card interrupts. */
//#define SD_CARD_INTERRUPTS

/* Allow old sdhci versions (may cause errors), required for QEMU. */
#define EMMC_ALLOW_OLD_SDHCI

#if RASPI <= 3
#define EMMC_BASE	ARM_EMMC_BASE
#else
#define EMMC_BASE	ARM_EMMC2_BASE
#endif

#define EMMC_ARG2		(EMMC_BASE + 0x00)
#define EMMC_BLKSIZECNT		(EMMC_BASE + 0x04)
#define EMMC_ARG1		(EMMC_BASE + 0x08)
#define EMMC_CMDTM		(EMMC_BASE + 0x0C)
#define EMMC_RESP0		(EMMC_BASE + 0x10)
#define EMMC_RESP1		(EMMC_BASE + 0x14)
#define EMMC_RESP2		(EMMC_BASE + 0x18)
#define EMMC_RESP3		(EMMC_BASE + 0x1C)
#define EMMC_DATA		(EMMC_BASE + 0x20)
#define EMMC_STATUS		(EMMC_BASE + 0x24)
#define EMMC_CONTROL0		(EMMC_BASE + 0x28)
#define EMMC_CONTROL1		(EMMC_BASE + 0x2C)
#define EMMC_INTERRUPT		(EMMC_BASE + 0x30)
#define EMMC_IRPT_MASK		(EMMC_BASE + 0x34)
#define EMMC_IRPT_EN		(EMMC_BASE + 0x38)
#define EMMC_CONTROL2		(EMMC_BASE + 0x3C)
#define EMMC_CAPABILITIES_0	(EMMC_BASE + 0x40)
#define EMMC_CAPABILITIES_1	(EMMC_BASE + 0x44)
#define EMMC_FORCE_IRPT		(EMMC_BASE + 0x50)
#define EMMC_BOOT_TIMEOUT	(EMMC_BASE + 0x70)
#define EMMC_DBG_SEL		(EMMC_BASE + 0x74)
#define EMMC_EXRDFIFO_CFG	(EMMC_BASE + 0x80)
#define EMMC_EXRDFIFO_EN	(EMMC_BASE + 0x84)
#define EMMC_TUNE_STEP		(EMMC_BASE + 0x88)
#define EMMC_TUNE_STEPS_STD	(EMMC_BASE + 0x8C)
#define EMMC_TUNE_STEPS_DDR	(EMMC_BASE + 0x90)
#define EMMC_SPI_INT_SPT	(EMMC_BASE + 0xF0)
#define EMMC_SLOTISR_VER	(EMMC_BASE + 0xFC)

#endif

#define SD_CMD_INDEX(a)             ((a) << 24)
#define SD_CMD_TYPE_NORMAL          0
#define SD_CMD_TYPE_SUSPEND         (1 << 22)
#define SD_CMD_TYPE_RESUME          (2 << 22)
#define SD_CMD_TYPE_ABORT           (3 << 22)
#define SD_CMD_TYPE_MASK            (3 << 22)
#define SD_CMD_ISDATA               (1 << 21)
#define SD_CMD_IXCHK_EN             (1 << 20)
#define SD_CMD_CRCCHK_EN            (1 << 19)
#define SD_CMD_RSPNS_TYPE_NONE      0   // For no response
#define SD_CMD_RSPNS_TYPE_136       (1 << 16)   // For response R2 (with CRC), R3,4 (no CRC)
#define SD_CMD_RSPNS_TYPE_48        (2 << 16)   // For responses R1, R5, R6, R7 (with CRC)
#define SD_CMD_RSPNS_TYPE_48B       (3 << 16)   // For responses R1b, R5b (with CRC)
#define SD_CMD_RSPNS_TYPE_MASK      (3 << 16)
#define SD_CMD_MULTI_BLOCK          (1 << 5)
#define SD_CMD_DAT_DIR_HC           0
#define SD_CMD_DAT_DIR_CH           (1 << 4)
#define SD_CMD_AUTO_CMD_EN_NONE     0
#define SD_CMD_AUTO_CMD_EN_CMD12    (1 << 2)
#define SD_CMD_AUTO_CMD_EN_CMD23    (2 << 2)
#define SD_CMD_BLKCNT_EN            (1 << 1)
#define SD_CMD_DMA                  1

#ifndef USE_SDHOST

#define SD_ERR_CMD_TIMEOUT	0
#define SD_ERR_CMD_CRC		1
#define SD_ERR_CMD_END_BIT	2
#define SD_ERR_CMD_INDEX	3
#define SD_ERR_DATA_TIMEOUT	4
#define SD_ERR_DATA_CRC		5
#define SD_ERR_DATA_END_BIT	6
#define SD_ERR_CURRENT_LIMIT	7
#define SD_ERR_AUTO_CMD12	8
#define SD_ERR_ADMA		9
#define SD_ERR_TUNING		10
#define SD_ERR_RSVD		11

#define SD_ERR_MASK_CMD_TIMEOUT		(1 << (16 + SD_ERR_CMD_TIMEOUT))
#define SD_ERR_MASK_CMD_CRC		(1 << (16 + SD_ERR_CMD_CRC))
#define SD_ERR_MASK_CMD_END_BIT		(1 << (16 + SD_ERR_CMD_END_BIT))
#define SD_ERR_MASK_CMD_INDEX		(1 << (16 + SD_ERR_CMD_INDEX))
#define SD_ERR_MASK_DATA_TIMEOUT	(1 << (16 + SD_ERR_CMD_TIMEOUT))
#define SD_ERR_MASK_DATA_CRC		(1 << (16 + SD_ERR_CMD_CRC))
#define SD_ERR_MASK_DATA_END_BIT	(1 << (16 + SD_ERR_CMD_END_BIT))
#define SD_ERR_MASK_CURRENT_LIMIT	(1 << (16 + SD_ERR_CMD_CURRENT_LIMIT))
#define SD_ERR_MASK_AUTO_CMD12		(1 << (16 + SD_ERR_CMD_AUTO_CMD12))
#define SD_ERR_MASK_ADMA		(1 << (16 + SD_ERR_CMD_ADMA))
#define SD_ERR_MASK_TUNING		(1 << (16 + SD_ERR_CMD_TUNING))

#define SD_COMMAND_COMPLETE     1
#define SD_TRANSFER_COMPLETE    (1 << 1)
#define SD_BLOCK_GAP_EVENT      (1 << 2)
#define SD_DMA_INTERRUPT        (1 << 3)
#define SD_BUFFER_WRITE_READY   (1 << 4)
#define SD_BUFFER_READ_READY    (1 << 5)
#define SD_CARD_INSERTION       (1 << 6)
#define SD_CARD_REMOVAL         (1 << 7)
#define SD_CARD_INTERRUPT       (1 << 8)

#endif

#define SD_RESP_NONE        SD_CMD_RSPNS_TYPE_NONE
#define SD_RESP_R1          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R1b         (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R2          (SD_CMD_RSPNS_TYPE_136 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R3          SD_CMD_RSPNS_TYPE_48
#define SD_RESP_R4          SD_CMD_RSPNS_TYPE_136
#define SD_RESP_R5          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R5b         (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R6          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R7          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)

#define SD_DATA_READ        (SD_CMD_ISDATA | SD_CMD_DAT_DIR_CH)
#define SD_DATA_WRITE       (SD_CMD_ISDATA | SD_CMD_DAT_DIR_HC)

#define SD_CMD_RESERVED(a)  0xffffffff

#define SUCCESS(self)       (self->last_cmd_success)
#define FAIL(self)          (self->last_cmd_success == 0)

#ifndef USE_SDHOST

#define TIMEOUT(self)       (FAIL(self) && (self->last_error == 0))
#define CMD_TIMEOUT(self)   (FAIL(self) && (self->last_error & (1 << 16)))
#define CMD_CRC(self)       (FAIL(self) && (self->last_error & (1 << 17)))
#define CMD_END_BIT(self)   (FAIL(self) && (self->last_error & (1 << 18)))
#define CMD_INDEX(self)     (FAIL(self) && (self->last_error & (1 << 19)))
#define DATA_TIMEOUT(self)  (FAIL(self) && (self->last_error & (1 << 20)))
#define DATA_CRC(self)      (FAIL(self) && (self->last_error & (1 << 21)))
#define DATA_END_BIT(self)  (FAIL(self) && (self->last_error & (1 << 22)))
#define CURRENT_LIMIT(self) (FAIL(self) && (self->last_error & (1 << 23)))
#define ACMD12_ERROR(self)  (FAIL(self) && (self->last_error & (1 << 24)))
#define ADMA_ERROR(self)    (FAIL(self) && (self->last_error & (1 << 25)))
#define TUNING_ERROR(self)  (FAIL(self) && (self->last_error & (1 << 26)))

#else

#define TIMEOUT(self)       (FAIL(self) && self->last_error == ETIMEDOUT)

#endif

#define SD_VER_UNKNOWN      0
#define SD_VER_1            1
#define SD_VER_1_1          2
#define SD_VER_2            3
#define SD_VER_3            4
#define SD_VER_4            5

const char *sd_versions[] = {
    "unknown",
    "1.0 or 1.01",
    "1.10",
    "2.00",
    "3.0x",
    "4.xx"
};

#ifndef USE_SDHOST
const char *err_irpts[] = {
    "CMD_TIMEOUT",
    "CMD_CRC",
    "CMD_END_BIT",
    "CMD_INDEX",
    "DATA_TIMEOUT",
    "DATA_CRC",
    "DATA_END_BIT",
    "CURRENT_LIMIT",
    "AUTO_CMD12",
    "ADMA",
    "TUNING",
    "RSVD"
};
#endif

const uint32_t sd_commands[] = {
    SD_CMD_INDEX(0),
    SD_CMD_RESERVED(1),
    SD_CMD_INDEX(2) | SD_RESP_R2,
    SD_CMD_INDEX(3) | SD_RESP_R6,
    SD_CMD_INDEX(4),
    SD_CMD_INDEX(5) | SD_RESP_R4,
    SD_CMD_INDEX(6) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(7) | SD_RESP_R1b,
    SD_CMD_INDEX(8) | SD_RESP_R7,
    SD_CMD_INDEX(9) | SD_RESP_R2,
    SD_CMD_INDEX(10) | SD_RESP_R2,
    SD_CMD_INDEX(11) | SD_RESP_R1,
    SD_CMD_INDEX(12) | SD_RESP_R1b | SD_CMD_TYPE_ABORT,
    SD_CMD_INDEX(13) | SD_RESP_R1,
    SD_CMD_RESERVED(14),
    SD_CMD_INDEX(15),
    SD_CMD_INDEX(16) | SD_RESP_R1,
    SD_CMD_INDEX(17) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(18) | SD_RESP_R1 | SD_DATA_READ | SD_CMD_MULTI_BLOCK | SD_CMD_BLKCNT_EN | SD_CMD_AUTO_CMD_EN_CMD12,    // SD_CMD_AUTO_CMD_EN_CMD12 not in original driver
    SD_CMD_INDEX(19) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(20) | SD_RESP_R1b,
    SD_CMD_RESERVED(21),
    SD_CMD_RESERVED(22),
    SD_CMD_INDEX(23) | SD_RESP_R1,
    SD_CMD_INDEX(24) | SD_RESP_R1 | SD_DATA_WRITE,
    SD_CMD_INDEX(25) | SD_RESP_R1 | SD_DATA_WRITE | SD_CMD_MULTI_BLOCK | SD_CMD_BLKCNT_EN | SD_CMD_AUTO_CMD_EN_CMD12,   // SD_CMD_AUTO_CMD_EN_CMD12 not in original driver
    SD_CMD_RESERVED(26),
    SD_CMD_INDEX(27) | SD_RESP_R1 | SD_DATA_WRITE,
    SD_CMD_INDEX(28) | SD_RESP_R1b,
    SD_CMD_INDEX(29) | SD_RESP_R1b,
    SD_CMD_INDEX(30) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_RESERVED(31),
    SD_CMD_INDEX(32) | SD_RESP_R1,
    SD_CMD_INDEX(33) | SD_RESP_R1,
    SD_CMD_RESERVED(34),
    SD_CMD_RESERVED(35),
    SD_CMD_RESERVED(36),
    SD_CMD_RESERVED(37),
    SD_CMD_INDEX(38) | SD_RESP_R1b,
    SD_CMD_RESERVED(39),
    SD_CMD_RESERVED(40),
    SD_CMD_RESERVED(41),
    SD_CMD_RESERVED(42) | SD_RESP_R1,
    SD_CMD_RESERVED(43),
    SD_CMD_RESERVED(44),
    SD_CMD_RESERVED(45),
    SD_CMD_RESERVED(46),
    SD_CMD_RESERVED(47),
    SD_CMD_RESERVED(48),
    SD_CMD_RESERVED(49),
    SD_CMD_RESERVED(50),
    SD_CMD_RESERVED(51),
    SD_CMD_RESERVED(52),
    SD_CMD_RESERVED(53),
    SD_CMD_RESERVED(54),
    SD_CMD_INDEX(55) | SD_RESP_R1,
    SD_CMD_INDEX(56) | SD_RESP_R1 | SD_CMD_ISDATA,
    SD_CMD_RESERVED(57),
    SD_CMD_RESERVED(58),
    SD_CMD_RESERVED(59),
    SD_CMD_RESERVED(60),
    SD_CMD_RESERVED(61),
    SD_CMD_RESERVED(62),
    SD_CMD_RESERVED(63)
};

const uint32_t sd_acommands[] = {
    SD_CMD_RESERVED(0),
    SD_CMD_RESERVED(1),
    SD_CMD_RESERVED(2),
    SD_CMD_RESERVED(3),
    SD_CMD_RESERVED(4),
    SD_CMD_RESERVED(5),
    SD_CMD_INDEX(6) | SD_RESP_R1,
    SD_CMD_RESERVED(7),
    SD_CMD_RESERVED(8),
    SD_CMD_RESERVED(9),
    SD_CMD_RESERVED(10),
    SD_CMD_RESERVED(11),
    SD_CMD_RESERVED(12),
    SD_CMD_INDEX(13) | SD_RESP_R1,
    SD_CMD_RESERVED(14),
    SD_CMD_RESERVED(15),
    SD_CMD_RESERVED(16),
    SD_CMD_RESERVED(17),
    SD_CMD_RESERVED(18),
    SD_CMD_RESERVED(19),
    SD_CMD_RESERVED(20),
    SD_CMD_RESERVED(21),
    SD_CMD_INDEX(22) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(23) | SD_RESP_R1,
    SD_CMD_RESERVED(24),
    SD_CMD_RESERVED(25),
    SD_CMD_RESERVED(26),
    SD_CMD_RESERVED(27),
    SD_CMD_RESERVED(28),
    SD_CMD_RESERVED(29),
    SD_CMD_RESERVED(30),
    SD_CMD_RESERVED(31),
    SD_CMD_RESERVED(32),
    SD_CMD_RESERVED(33),
    SD_CMD_RESERVED(34),
    SD_CMD_RESERVED(35),
    SD_CMD_RESERVED(36),
    SD_CMD_RESERVED(37),
    SD_CMD_RESERVED(38),
    SD_CMD_RESERVED(39),
    SD_CMD_RESERVED(40),
    SD_CMD_INDEX(41) | SD_RESP_R3,
    SD_CMD_INDEX(42) | SD_RESP_R1,
    SD_CMD_RESERVED(43),
    SD_CMD_RESERVED(44),
    SD_CMD_RESERVED(45),
    SD_CMD_RESERVED(46),
    SD_CMD_RESERVED(47),
    SD_CMD_RESERVED(48),
    SD_CMD_RESERVED(49),
    SD_CMD_RESERVED(50),
    SD_CMD_INDEX(51) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_RESERVED(52),
    SD_CMD_RESERVED(53),
    SD_CMD_RESERVED(54),
    SD_CMD_RESERVED(55),
    SD_CMD_RESERVED(56),
    SD_CMD_RESERVED(57),
    SD_CMD_RESERVED(58),
    SD_CMD_RESERVED(59),
    SD_CMD_RESERVED(60),
    SD_CMD_RESERVED(61),
    SD_CMD_RESERVED(62),
    SD_CMD_RESERVED(63)
};

// The actual command indices
#define GO_IDLE_STATE           0
#define ALL_SEND_CID            2
#define SEND_RELATIVE_ADDR      3
#define SET_DSR                 4
#define IO_SET_OP_COND          5
#define SWITCH_FUNC             6
#define SELECT_CARD             7
#define DESELECT_CARD           7
#define SELECT_DESELECT_CARD    7
#define SEND_IF_COND            8
#define SEND_CSD                9
#define SEND_CID                10
#define VOLTAGE_SWITCH          11
#define STOP_TRANSMISSION       12
#define SEND_STATUS             13
#define GO_INACTIVE_STATE       15
#define SET_BLOCKLEN            16
#define READ_SINGLE_BLOCK       17
#define READ_MULTIPLE_BLOCK     18
#define SEND_TUNING_BLOCK       19
#define SPEED_CLASS_CONTROL     20
#define SET_BLOCK_COUNT         23
#define WRITE_BLOCK             24
#define WRITE_MULTIPLE_BLOCK    25
#define PROGRAM_CSD             27
#define SET_WRITE_PROT          28
#define CLR_WRITE_PROT          29
#define SEND_WRITE_PROT         30
#define ERASE_WR_BLK_START      32
#define ERASE_WR_BLK_END        33
#define ERASE                   38
#define LOCK_UNLOCK             42
#define APP_CMD                 55
#define GEN_CMD                 56

#define IS_APP_CMD              0x80000000
#define ACMD(a)                 (a | IS_APP_CMD)
#define SET_BUS_WIDTH           (6 | IS_APP_CMD)
#define SD_STATUS               (13 | IS_APP_CMD)
#define SEND_NUM_WR_BLOCKS      (22 | IS_APP_CMD)
#define SET_WR_BLK_ERASE_COUNT  (23 | IS_APP_CMD)
#define SD_SEND_OP_COND         (41 | IS_APP_CMD)
#define SET_CLR_CARD_DETECT     (42 | IS_APP_CMD)
#define SEND_SCR                (51 | IS_APP_CMD)

#ifndef USE_SDHOST

#define SD_RESET_CMD            (1 << 25)
#define SD_RESET_DAT            (1 << 26)
#define SD_RESET_ALL            (1 << 24)

#define SD_GET_CLOCK_DIVIDER_FAIL	0xffffffff

#endif

#define SD_BLOCK_SIZE        512


static int emmc_ensure_data_mode(struct emmc *self);
static int emmc_do_data_command(struct emmc *self, int is_write,
                                uint8_t * buf, size_t buf_size,
                                uint32_t block_no);
static size_t emmc_do_read(struct emmc *self, uint8_t * buf,
                           size_t buf_size, uint32_t block_no);
static size_t emmc_do_write(struct emmc *self, uint8_t * buf,
                            size_t buf_size, uint32_t block_no);

static int emmc_card_init(struct emmc *self);
static int emmc_card_reset(struct emmc *self);

static int emmc_issue_command(struct emmc *self, uint32_t cmd,
                              uint32_t arg, int timeout);
static void emmc_issue_command_int(struct emmc *self, uint32_t reg,
                                   uint32_t arg, int timeout);

static inline uint32_t
be2le32(uint32_t x)
{
    return ((x & 0x000000FF) << 24)
        | ((x & 0x0000FF00) << 8)
        | ((x & 0x00FF0000) >> 8)
        | ((x & 0xFF000000) >> 24);
}

void
emmc_intr(struct emmc *self)
{
#ifdef USE_SDHOST
    sdhost_intr(&self->host);
#endif
}

void
emmc_clear_interrupt()
{
    uint32_t irpts = get32(EMMC_INTERRUPT);
    debug("irpts: 0x%x", irpts);
    put32(EMMC_INTERRUPT, irpts);
}

int
emmc_init(struct emmc *self, void (*sleep_fn)(void *), void *sleep_arg)
{
#ifndef USE_SDHOST

#if RASPI == 3
    // TODO: Initialize gpio on pi3
#elif RASPI == 4
    // Disable 1.8V supply.
    // if (mbox_set_gpio_state(MBOX_EXP_GPIO_BASE + 4, 0) < 0) {
    //     error("failed to set gpio state");
    //     return -1;
    // }
    // delayus(5000);
#endif

#else

    if (!sdhost_init(&self->host, sleep_fn, sleep_arg)) {
        return -1;
    }

#endif

    if (emmc_card_init(self) != 0) {
        return -1;
    }
    return 0;
}

size_t
emmc_read(struct emmc *self, void *buf, size_t cnt)
{
    if (self->ull_offset % SD_BLOCK_SIZE != 0) {
        return -1;
    }
    uint32_t nblock = self->ull_offset / SD_BLOCK_SIZE;

    if (emmc_do_read(self, (uint8_t *) buf, cnt, nblock) != cnt) {
        return -1;
    }
    return cnt;
}

size_t
emmc_write(struct emmc *self, void *buf, size_t cnt)
{
    if (self->ull_offset % SD_BLOCK_SIZE != 0) {
        return -1;
    }
    uint32_t nblock = self->ull_offset / SD_BLOCK_SIZE;

    if (emmc_do_write(self, (uint8_t *) buf, cnt, nblock) != cnt) {
        return -1;
    }
    return cnt;
}

uint64_t
emmc_seek(struct emmc *self, uint64_t off)
{
    self->ull_offset = off;
    return off;
}

#ifndef USE_SDHOST

static int
emmc_power_on()
{
    if (mbox_set_power_state(MBOX_DEVICE_SDCARD, 1, 1) != 1) {
        error("failed to power on sd card");
        return -1;
    }
    return 0;
}

static void
emmc_power_off()
{
    // Power off the SD card.
    uint32_t control0 = get32(EMMC_CONTROL0);
    // Set SD Bus Power bit off in Power Control Register
    control0 &= ~(1 << 8);
    put32(EMMC_CONTROL0, control0);
}

static int
emmc_timeout_wait(uint64_t reg, uint32_t mask, int value, uint64_t us)
{
    uint64_t start = timestamp();
    uint64_t dt = us * (timerfreq() / 1000000);
    do {
        if ((get32(reg) & mask) ? value : !value) {
            return 0;
        }
    } while (timestamp() - start < dt);

    return -1;
}

/* Get the current base clock rate in Hz. */
static uint32_t
emmc_get_base_clock()
{
#if RASPI <= 3
    uint32_t clock_id = MBOX_CLOCK_EMMC;
#else
    uint32_t clock_id = MBOX_CLOCK_EMMC2;
#endif
    uint32_t rate = mbox_get_clock_rate(clock_id);
    if (rate < 0) {
        error("failed to get clock rate");
        return 0;
    }
    debug("base clock rate is %u Hz", rate);
    return rate;
}


// Set the clock dividers to generate a target value
static uint32_t
emmc_get_clock_divider(uint32_t base_clock, uint32_t target_rate)
{
    // TODO: implement use of preset value registers

    uint32_t targetted_divisor = 1;
    if (target_rate <= base_clock) {
        targetted_divisor = base_clock / target_rate;
        if (base_clock % target_rate) {
            targetted_divisor--;
        }
    }

    // Decide on the clock mode to use
    // Currently only 10-bit divided clock mode is supported

#ifndef EMMC_ALLOW_OLD_SDHCI
    if (self->hci_ver >= 2) {
#endif
        // HCI version 3 or greater supports 10-bit divided clock mode
        // This requires a power-of-two divider

        // Find the first bit set
        int divisor = -1;
        for (int first_bit = 31; first_bit >= 0; first_bit--) {
            uint32_t bit_test = (1 << first_bit);
            if (targetted_divisor & bit_test) {
                divisor = first_bit;
                targetted_divisor &= ~bit_test;
                if (targetted_divisor) {
                    // The divisor is not a power-of-two, increase it
                    divisor++;
                }

                break;
            }
        }

        if (divisor == -1) {
            divisor = 31;
        }
        if (divisor >= 32) {
            divisor = 31;
        }

        if (divisor != 0) {
            divisor = (1 << (divisor - 1));
        }

        if (divisor >= 0x400) {
            divisor = 0x3ff;
        }

        uint32_t freq_select = divisor & 0xff;
        uint32_t upper_bits = (divisor >> 8) & 0x3;
        uint32_t ret = (freq_select << 8) | (upper_bits << 6) | (0 << 5);

        int denominator = 1;
        if (divisor != 0) {
            denominator = divisor * 2;
        }
        int actual_clock = base_clock / denominator;
        debug
            ("base_clock: %d, target_rate: %d, divisor: 0x%x, actual_clock: %d, ret: 0x%x",
             base_clock, target_rate, divisor, actual_clock, ret);

        return ret;
#ifndef EMMC_ALLOW_OLD_SDHCI
    } else {
        error("unsupported host version");
        return SD_GET_CLOCK_DIVIDER_FAIL;
    }
#endif
}


// Switch the clock rate whilst running
static int
emmc_switch_clock_rate(uint32_t base_clock, uint32_t target_rate)
{
    // Decide on an appropriate divider
    uint32_t divider = emmc_get_clock_divider(base_clock, target_rate);
    if (divider == SD_GET_CLOCK_DIVIDER_FAIL) {
        debug("failed to get a valid divider for target rate %d Hz",
              target_rate);
        return -1;
    }

    // Wait for the command inhibit (CMD and DAT) bits to clear
    while (get32(EMMC_STATUS) & 3) {
        delayus(1000);
    }

    // Set the SD clock off
    uint32_t control1 = get32(EMMC_CONTROL1);
    control1 &= ~(1 << 2);
    put32(EMMC_CONTROL1, control1);
    delayus(2000);

    // Write the new divider
    control1 &= ~0xffe0;        // Clear old setting + clock generator select
    control1 |= divider;
    put32(EMMC_CONTROL1, control1);
    delayus(2000);

    // Enable the SD clock
    control1 |= (1 << 2);
    put32(EMMC_CONTROL1, control1);
    delayus(2000);

    trace("set clock rate to %d Hz successfully", target_rate);
    return 0;
}

static int
emmc_reset_cmd()
{
    uint32_t control1 = get32(EMMC_CONTROL1);
    control1 |= SD_RESET_CMD;
    put32(EMMC_CONTROL1, control1);

    if (emmc_timeout_wait(EMMC_CONTROL1, SD_RESET_CMD, 0, 1000000) < 0) {
        error("CMD line did not reset properly");
        return -1;
    }
    return 0;
}

int
emmc_reset_dat()
{
    uint32_t control1 = get32(EMMC_CONTROL1);
    control1 |= SD_RESET_DAT;
    put32(EMMC_CONTROL1, control1);

    if (emmc_timeout_wait(EMMC_CONTROL1, SD_RESET_DAT, 0, 1000000) < 0) {
        error("DAT line did not reset properly");

        return -1;
    }
    return 0;
}

void
emmc_issue_command_int(struct emmc *self, uint32_t cmd_reg,
                       uint32_t argument, int timeout)
{
    self->last_cmd_reg = cmd_reg;
    self->last_cmd_success = 0;

    // This is as per HCSS 3.7.1.1/3.7.2.2

#ifdef EMMC_POLL_STATUS_REG
    // Check Command Inhibit
    while (get32(EMMC_STATUS) & 1) {
        delayus(1000);
    }

    // Is the command with busy?
    if ((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) {
        // With busy

        // Is is an abort command?
        if ((cmd_reg & SD_CMD_TYPE_MASK) != SD_CMD_TYPE_ABORT) {
            // Not an abort command

            // Wait for the data line to be free
            while (get32(EMMC_STATUS) & 2) {
                delayus(1000);
            }
        }
    }
#endif

    // Set block size and block count
    // For now, block size = 512 bytes, block count = 1,
    if (self->blocks_to_transfer > 0xffff) {
        debug("blocks_to_transfer too great (%d)",
              self->blocks_to_transfer);
        self->last_cmd_success = 0;
        return;
    }
    uint32_t blksizecnt =
        self->block_size | (self->blocks_to_transfer << 16);
    put32(EMMC_BLKSIZECNT, blksizecnt);

    // Set argument 1 reg
    put32(EMMC_ARG1, argument);

    // Set command reg
    put32(EMMC_CMDTM, cmd_reg);

    // delayus(2000);

    // Wait for command complete interrupt
    emmc_timeout_wait(EMMC_INTERRUPT, 0x8001, 1, timeout);
    uint32_t irpts = get32(EMMC_INTERRUPT);

    // Clear command complete status
    put32(EMMC_INTERRUPT, 0xffff0001);

    // Test for errors
    if ((irpts & 0xffff0001) != 1) {
        warn("rrror occured whilst waiting for command complete interrupt");
        self->last_error = irpts & 0xffff0000;
        self->last_interrupt = irpts;

        return;
    }

    // delayus(2000);

    // Get response data
    switch (cmd_reg & SD_CMD_RSPNS_TYPE_MASK) {
    case SD_CMD_RSPNS_TYPE_48:
    case SD_CMD_RSPNS_TYPE_48B:
        self->last_r0 = get32(EMMC_RESP0);
        break;

    case SD_CMD_RSPNS_TYPE_136:
        self->last_r0 = get32(EMMC_RESP0);
        self->last_r1 = get32(EMMC_RESP1);
        self->last_r2 = get32(EMMC_RESP2);
        self->last_r3 = get32(EMMC_RESP3);
        break;
    }

    // If with data, wait for the appropriate interrupt
    if (cmd_reg & SD_CMD_ISDATA) {
        uint32_t wr_irpt;
        int is_write = 0;
        if (cmd_reg & SD_CMD_DAT_DIR_CH) {
            wr_irpt = (1 << 5); // read
        } else {
            is_write = 1;
            wr_irpt = (1 << 4); // write
        }

        if (self->blocks_to_transfer > 1) {
            trace("multi-block transfer");
        }

        assert(((uint64_t) self->buf & 3) == 0);
        uint32_t *pData = (uint32_t *) self->buf;

        for (int nBlock = 0; nBlock < self->blocks_to_transfer; nBlock++) {
            emmc_timeout_wait(EMMC_INTERRUPT, wr_irpt | 0x8000, 1,
                              timeout);
            irpts = get32(EMMC_INTERRUPT);
            put32(EMMC_INTERRUPT, 0xffff0000 | wr_irpt);

            if ((irpts & (0xffff0000 | wr_irpt)) != wr_irpt) {
                warn("error occured whilst waiting for data ready interrupt");

                self->last_error = irpts & 0xffff0000;
                self->last_interrupt = irpts;
                return;
            }

            // Transfer the block
            assert(self->block_size <= 1024);   // internal FIFO size of EMMC
            size_t length = self->block_size;
            assert((length & 3) == 0);

            if (is_write) {
                for (; length > 0; length -= 4) {
                    put32(EMMC_DATA, *pData++);
                }
            } else {
                for (; length > 0; length -= 4) {
                    *pData++ = get32(EMMC_DATA);
                }
            }
        }
        trace("block transfer complete");
    }

    // Wait for transfer complete (set if read/write transfer or with busy)
    if ((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B
        || (cmd_reg & SD_CMD_ISDATA)) {
#ifdef EMMC_POLL_STATUS_REG
        // First check command inhibit (DAT) is not already 0
        if ((get32(EMMC_STATUS) & 2) == 0) {
            put32(EMMC_INTERRUPT, 0xffff0002);
        } else
#endif
        {
            emmc_timeout_wait(EMMC_INTERRUPT, 0x8002, 1, timeout);
            irpts = get32(EMMC_INTERRUPT);
            put32(EMMC_INTERRUPT, 0xffff0002);

            // Handle the case where both data timeout and transfer complete
            //  are set - transfer complete overrides data timeout: HCSS 2.2.17
            if (((irpts & 0xffff0002) != 2)
                && ((irpts & 0xffff0002) != 0x100002)) {
                warn("error occured whilst waiting for transfer complete interrupt");
                self->last_error = irpts & 0xffff0000;
                self->last_interrupt = irpts;
                return;
            }

            put32(EMMC_INTERRUPT, 0xffff0002);
        }
    }

    // Return success
    self->last_cmd_success = 1;
}

/* Handle a card interrupt. */
void
emmc_handle_card_interrupt(struct emmc *self)
{
    uint32_t status = get32(EMMC_STATUS);

    trace("status: 0x%x", status);

    // Get the card status
    if (self->card_rca) {
        emmc_issue_command_int(self, sd_commands[SEND_STATUS],
                               self->card_rca << 16, 500000);
        if (FAIL(self)) {
            warn("unable to get card status");
        } else {
            trace("card status: 0x%x", self->last_r0);
        }
    } else {
        debug("no card currently selected");
    }
}

void
emmc_handle_interrupts(struct emmc *self)
{
    uint32_t irpts = get32(EMMC_INTERRUPT);
    uint32_t reset_mask = 0;

    if (irpts & SD_COMMAND_COMPLETE) {
        trace("spurious command complete interrupt");
        reset_mask |= SD_COMMAND_COMPLETE;
    }

    if (irpts & SD_TRANSFER_COMPLETE) {
        trace("spurious transfer complete interrupt");
        reset_mask |= SD_TRANSFER_COMPLETE;
    }

    if (irpts & SD_BLOCK_GAP_EVENT) {
        trace("spurious block gap event interrupt");
        reset_mask |= SD_BLOCK_GAP_EVENT;
    }

    if (irpts & SD_DMA_INTERRUPT) {
        trace("spurious DMA interrupt");
        reset_mask |= SD_DMA_INTERRUPT;
    }

    if (irpts & SD_BUFFER_WRITE_READY) {
        trace("spurious buffer write ready interrupt");
        reset_mask |= SD_BUFFER_WRITE_READY;
        emmc_reset_dat();
    }

    if (irpts & SD_BUFFER_READ_READY) {
        trace("spurious buffer read ready interrupt");
        reset_mask |= SD_BUFFER_READ_READY;
        emmc_reset_dat();
    }

    if (irpts & SD_CARD_INSERTION) {
        trace("card insertion detected");
        reset_mask |= SD_CARD_INSERTION;
    }

    if (irpts & SD_CARD_REMOVAL) {
        trace("card removal detected");
        reset_mask |= SD_CARD_REMOVAL;
        self->card_removal = 1;
    }

    if (irpts & SD_CARD_INTERRUPT) {
        trace("card interrupt detected");
        emmc_handle_card_interrupt(self);
        reset_mask |= SD_CARD_INTERRUPT;
    }

    if (irpts & 0x8000) {
        debug("spurious error interrupt: 0x%x", irpts);
        reset_mask |= 0xffff0000;
    }

    put32(EMMC_INTERRUPT, reset_mask);
}

#else

static void
emmc_issue_command_int(struct emmc *self, uint32_t reg, uint32_t arg,
                       int timeout)
{
    self->last_cmd_reg = reg;
    self->last_cmd_success = 0;

    // Set block size and block count
    // For now, block size = 512 bytes, block count = 1,
    if (self->blocks_to_transfer > 0xffff) {
        debug("blocks_to_transfer too great: %d",
              self->blocks_to_transfer);
        return;
    }

    struct mmc_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = reg >> 24;
    cmd.arg = arg;

    switch (reg & SD_CMD_RSPNS_TYPE_MASK) {
    case SD_CMD_RSPNS_TYPE_48:
        cmd.flags |= MMC_RSP_PRESENT;
        break;

    case SD_CMD_RSPNS_TYPE_48B:
        cmd.flags |= MMC_RSP_PRESENT | MMC_RSP_BUSY;
        break;

    case SD_CMD_RSPNS_TYPE_136:
        cmd.flags |= MMC_RSP_PRESENT | MMC_RSP_136;
        break;
    }

    if (reg & SD_CMD_CRCCHK_EN) {
        cmd.flags |= MMC_RSP_CRC;
    }

    struct mmc_data data;
    if (reg & SD_CMD_ISDATA) {
        memset(&data, 0, sizeof(data));
        data.flags |=
            reg & SD_CMD_DAT_DIR_CH ? MMC_DATA_READ : MMC_DATA_WRITE;
        data.blksz = self->block_size;
        data.blocks = self->blocks_to_transfer;
        data.sg = self->buf;
        data.sg_len = self->block_size * self->blocks_to_transfer;
        trace("is data, sg %p", data.sg);

        cmd.data = &data;
    }

    int err = sdhost_command(&self->host, &cmd, 0);
    if (err != 0) {
        assert(err < 0);
        self->last_error = -err;
        return;
    }

    // Get response data
    switch (reg & SD_CMD_RSPNS_TYPE_MASK) {
    case SD_CMD_RSPNS_TYPE_48:
    case SD_CMD_RSPNS_TYPE_48B:
        self->last_r0 = cmd.resp[0];
        break;

    case SD_CMD_RSPNS_TYPE_136:
        self->last_r0 = cmd.resp[3];
        self->last_r1 = cmd.resp[2];
        self->last_r2 = cmd.resp[1];
        self->last_r3 = cmd.resp[0];
        break;
    }

    // Return success
    self->last_cmd_success = 1;
}

#endif


static int
emmc_card_init(struct emmc *self)
{
#ifndef USE_SDHOST
    if (emmc_power_on() != 0) {
        error("BCM2708 controller did not power on successfully");
        return -1;
    }
    info("poweron");
#endif

    // Check the sanity of the sd_commands and sd_acommands structures
    assert(sizeof(sd_commands) == (64 * sizeof(uint32_t)));
    assert(sizeof(sd_acommands) == (64 * sizeof(uint32_t)));

#ifndef USE_SDHOST
    // Read the controller version
    uint32_t ver = get32(EMMC_SLOTISR_VER);
    uint32_t sdversion = (ver >> 16) & 0xff;
    uint32_t vendor = ver >> 24;
    uint32_t slot_status = ver & 0xff;
    trace("vendor 0x%x, SD version 0x%x, slot status 0x%x", vendor,
          sdversion, slot_status);
    self->hci_ver = sdversion;
    if (self->hci_ver < 2) {
#ifdef EMMC_ALLOW_OLD_SDHCI
        warn("old SDHCI version detected");
#else
        error("only SDHCI versions >= 3.0 are supported");
        return -1;
#endif
    }

#endif

    // The SEND_SCR command may fail with a DATA_TIMEOUT on the Raspberry Pi 4
    // for unknown reason. As a workaround the whole card reset is retried.
    int ret;
    for (int ntries = 3; ntries > 0; ntries--) {
        ret = emmc_card_reset(self);
        if (ret != -2) {
            break;
        }
        warn("card reset failed, retrying.");
    }
    return ret;
}

static int
emmc_ensure_data_mode(struct emmc *self)
{
    if (self->card_rca == 0) {
        // Try again to initialise the card
        int ret = emmc_card_reset(self);
        if (ret != 0) {
            return ret;
        }
    }

    trace("obtaining status register for card_rca 0x%x: ", self->card_rca);

    if (!emmc_issue_command
        (self, SEND_STATUS, self->card_rca << 16, 500000)) {
        warn("error sending CMD13");
        self->card_rca = 0;

        return -1;
    }

    uint32_t status = self->last_r0;
    uint32_t cur_state = (status >> 9) & 0xf;
    trace("status 0x%x", cur_state);
    if (cur_state == 3) {
        // Currently in the stand-by state - select it
        if (!emmc_issue_command
            (self, SELECT_CARD, self->card_rca << 16, 500000)) {
            warn("no response from CMD17");
            self->card_rca = 0;

            return -1;
        }
    } else if (cur_state == 5) {
        // In the data transfer state - cancel the transmission
        if (!emmc_issue_command(self, STOP_TRANSMISSION, 0, 500000)) {
            warn("no response from CMD12");
            self->card_rca = 0;

            return -1;
        }
#ifndef USE_SDHOST
        // Reset the data circuit
        emmc_reset_dat();
#endif
    } else if (cur_state != 4) {
        // Not in the transfer state, re-initialise.
        int ret = emmc_card_reset(self);
        if (ret != 0) {
            return ret;
        }
    }

    // Check again that we're now in the correct mode
    if (cur_state != 4) {
        if (!emmc_issue_command
            (self, SEND_STATUS, self->card_rca << 16, 500000)) {
            warn("no response from CMD13");
            self->card_rca = 0;

            return -1;
        }
        status = self->last_r0;
        cur_state = (status >> 9) & 0xf;
        trace("recheck status 0x%x", cur_state);

        if (cur_state != 4) {
            warn("unable to initialise SD card to data mode (state 0x%x)",
                 cur_state);
            self->card_rca = 0;

            return -1;
        }
    }

    return 0;
}

static int
emmc_do_data_command(struct emmc *self, int is_write, uint8_t * buf,
                     size_t buf_size, uint32_t block_no)
{

    // PLSS table 4.20 - SDSC cards use byte addresses rather than block addresses
    if (!self->card_supports_sdhc) {
        block_no *= SD_BLOCK_SIZE;
    }

    // This is as per HCSS 3.7.2.1
    if (buf_size < self->block_size) {
        warn("buffer size (%d) less than block size (%d)", buf_size,
             self->block_size);

        return -1;
    }

    self->blocks_to_transfer = buf_size / self->block_size;
    if (buf_size % self->block_size) {
        warn("buffer size (%d) not a multiple of block size (%d)",
             buf_size, self->block_size);

        return -1;
    }
    self->buf = buf;

    // Decide on the command to use
    int command;
    if (is_write) {
        if (self->blocks_to_transfer > 1) {
            command = WRITE_MULTIPLE_BLOCK;
        } else {
            command = WRITE_BLOCK;
        }
    } else {
        if (self->blocks_to_transfer > 1) {
            command = READ_MULTIPLE_BLOCK;
        } else {
            command = READ_SINGLE_BLOCK;
        }
    }

    int retry_count = 0;
    int max_retries = 3;
    while (retry_count < max_retries) {
        if (emmc_issue_command(self, command, block_no, 5000000)) {
            break;
        } else {
            warn("error sending CMD%d", command);
            trace("error = 0x%x", self->last_error);

            if (++retry_count < max_retries) {
                trace("Retrying");
            } else {
                trace("Giving up");
            }
        }
    }

    if (retry_count == max_retries) {
        self->card_rca = 0;
        return -1;
    }
    return 0;
}


static size_t
emmc_do_read(struct emmc *self, uint8_t * buf, size_t buf_size,
             uint32_t block_no)
{
    // Check the status of the card
    if (emmc_ensure_data_mode(self) != 0) {
        return -1;
    }

    trace("reading from block %u", block_no);

    if (emmc_do_data_command(self, 0, buf, buf_size, block_no) < 0) {
        return -1;
    }

    trace("success");
    return buf_size;
}

static size_t
emmc_do_write(struct emmc *self, uint8_t * buf, size_t buf_size,
              uint32_t block_no)
{
    // Check the status of the card
    if (emmc_ensure_data_mode(self) != 0) {
        return -1;
    }
    trace("writing to block %u", block_no);

    if (emmc_do_data_command(self, 1, buf, buf_size, block_no) < 0) {
        return -1;
    }

    trace("success");

    return buf_size;
}


static int
emmc_card_reset(struct emmc *self)
{

#ifndef USE_SDHOST
    trace("resetting controller");

    uint32_t control1 = get32(EMMC_CONTROL1);
    control1 |= (1 << 24);
    // Disable clock
    control1 &= ~(1 << 2);
    control1 &= ~(1 << 0);
    put32(EMMC_CONTROL1, control1);
    if (emmc_timeout_wait(EMMC_CONTROL1, 7 << 24, 0, 1000000) < 0) {
        error("controller did not reset properly");
        return -1;
    }
    debug("control0: 0x%x, control1: 0x%x, control2: 0x%x",
          get32(EMMC_CONTROL0), get32(EMMC_CONTROL1),
          get32(EMMC_CONTROL2));

#if RASPI >= 4
    // Enable SD Bus Power VDD1 at 3.3V
    uint32_t control0 = get32(EMMC_CONTROL0);
    control0 |= 0x0F << 8;
    put32(EMMC_CONTROL0, control0);
    delayus(2000);
#endif

    // Check for a valid card
    trace("checking for an inserted card");
    emmc_timeout_wait(EMMC_STATUS, 1 << 16, 1, 500000);
    uint32_t status_reg = get32(EMMC_STATUS);
    if ((status_reg & (1 << 16)) == 0) {
        warn("no card inserted");
        return -1;
    }
    debug("status: 0x%x", status_reg);

    // Clear control2
    put32(EMMC_CONTROL2, 0);

    // Get the base clock rate
    uint32_t base_clock = emmc_get_base_clock();
    if (base_clock == 0) {
        warn("assuming clock rate to be 100MHz");
        base_clock = 100000000;
    }

    // Set clock rate to something slow
    trace("setting clock rate");
    control1 = get32(EMMC_CONTROL1);
    control1 |= 1;              // enable clock

    // Set to identification frequency (400 kHz)
    uint32_t f_id = emmc_get_clock_divider(base_clock, SD_CLOCK_ID);
    if (f_id == SD_GET_CLOCK_DIVIDER_FAIL) {
        debug("unable to get a valid clock divider for ID frequency");
        return -1;
    }
    control1 |= f_id;

    // was not masked out and or'd with (7 << 16) in original driver
    control1 &= ~(0xF << 16);
    control1 |= (11 << 16);     // data timeout = TMCLK * 2^24

    put32(EMMC_CONTROL1, control1);

    if (emmc_timeout_wait(EMMC_CONTROL1, 2, 1, 1000000) < 0) {
        error("clock did not stabilise within 1 second");
        return -1;
    }
    trace("control0: 0x%x, control1: 0x%x",
          get32(EMMC_CONTROL0), get32(EMMC_CONTROL1));

    // Enable the SD clock
    trace("enabling SD clock");
    delayus(2000);
    control1 = get32(EMMC_CONTROL1);
    control1 |= 4;
    put32(EMMC_CONTROL1, control1);
    delayus(2000);

    // Mask off sending interrupts to the ARM
    put32(EMMC_IRPT_EN, 0);
    // Reset interrupts
    put32(EMMC_INTERRUPT, 0xffffffff);
    // Have all interrupts sent to the INTERRUPT register
    uint32_t irpt_mask = 0xffffffff & (~SD_CARD_INTERRUPT);
#ifdef SD_CARD_INTERRUPTS
    irpt_mask |= SD_CARD_INTERRUPT;
#endif
    put32(EMMC_IRPT_MASK, irpt_mask);

    delayus(2000);

#else
    sdhost_set_clock(&self->host, SD_CLOCK_ID);
#endif

    // >> Prepare the device structure
    self->device_id[0] = 0;
    self->device_id[1] = 0;
    self->device_id[2] = 0;
    self->device_id[3] = 0;

    self->card_supports_sdhc = 0;
    self->card_supports_hs = 0;
    self->card_supports_18v = 0;
    self->card_ocr = 0;
    self->card_rca = 0;

#ifndef USE_SDHOST
    self->last_interrupt = 0;
#endif

    self->last_error = 0;

    self->failed_voltage_switch = 0;

    self->last_cmd_reg = 0;
    self->last_cmd = 0;
    self->last_cmd_success = 0;
    self->last_r0 = 0;
    self->last_r1 = 0;
    self->last_r2 = 0;
    self->last_r3 = 0;

    self->buf = 0;
    self->blocks_to_transfer = 0;
    self->block_size = 0;
#ifndef USE_SDHOST
    self->card_removal = 0;
    self->base_clock = 0;
#endif
    // << Prepare the device structure

#ifndef USE_SDHOST
    self->base_clock = base_clock;
#endif

    // Send CMD0 to the card (reset to idle state)
    if (!emmc_issue_command(self, GO_IDLE_STATE, 0, 500000)) {
        error("no CMD0 response");
        return -1;
    }

    // Send CMD8 to the card
    // Voltage supplied = 0x1 = 2.7-3.6V (standard)
    // Check pattern = 10101010b (as per PLSS 4.3.13) = 0xAA
    // Note a timeout error on the following command (CMD8) is normal and expected if the SD card version is less than 2.0
    emmc_issue_command(self, SEND_IF_COND, 0x1aa, 500000);
    int v2_later = 0;
    if (TIMEOUT(self)) {
        v2_later = 0;
    }
#ifndef USE_SDHOST
    else if (CMD_TIMEOUT(self)) {
        if (emmc_reset_cmd() == -1) {
            return -1;
        }
        put32(EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        v2_later = 0;
    }
#endif
    else if (FAIL(self)) {
        error("failed to send CMD8");
        return -1;
    } else {
        if ((self->last_r0 & 0xfff) != 0x1aa) {
            error("unusable card");
            return -1;
        } else {
            v2_later = 1;
        }
    }


    // Here we are supposed to check the response to CMD5 (HCSS 3.6)
    // It only returns if the card is a SDIO card
    // Note that a timeout error on the following command (CMD5) is normal and expected if the card is not a SDIO card.
    emmc_issue_command(self, IO_SET_OP_COND, 0, 10000);
    if (!TIMEOUT(self)) {
#ifndef USE_SDHOST
        if (CMD_TIMEOUT(self)) {
            if (emmc_reset_cmd() == -1) {
                return -1;
            }

            put32(EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        } else
#endif
        {
            error("found SDIO card, unimplemented");
            return -1;
        }
    }

    // Call an inquiry ACMD41 (voltage window = 0) to get the OCR
    if (!emmc_issue_command(self, ACMD(41), 0, 500000)) {
        error("failed to inquiry ACMD41");
        return -1;
    }

    // Call initialization ACMD41
    int busy = 1;
    while (busy) {
        uint32_t v2_flags = 0;
        if (v2_later) {
            // Set SDHC support
            v2_flags |= (1 << 30);

            // Set 1.8v support
#ifdef SD_1_8V_SUPPORT
            if (!self->failed_voltage_switch) {
                v2_flags |= (1 << 24);
            }
#endif
#ifdef SDXC_MAXIMUM_PERFORMANCE
            // Enable SDXC maximum performance
            v2_flags |= (1 << 28);
#endif
        }

        if (!emmc_issue_command
            (self, ACMD(41), 0x00ff8000 | v2_flags, 500000)) {
            error("error issuing ACMD41");
            return -1;
        }

        if ((self->last_r0 >> 31) & 1) {
            // Initialization is complete
            self->card_ocr = (self->last_r0 >> 8) & 0xffff;
            self->card_supports_sdhc = (self->last_r0 >> 30) & 0x1;
#ifdef SD_1_8V_SUPPORT
            if (!self->failed_voltage_switch) {
                self->card_supports_18v = (self->last_r0 >> 24) & 0x1;
            }
#endif
            busy = 0;
        } else {
            // Card is still busy
            delayus(500000);
        }

    }

    debug("OCR: 0x%x, 1.8v support: %d, SDHC support: %d", self->card_ocr,
          self->card_supports_18v, self->card_supports_sdhc);

    // At this point, we know the card is definitely an SD card, so will definitely
    // support SDR12 mode which runs at 25 MHz
#ifndef USE_SDHOST
    emmc_switch_clock_rate(base_clock, SD_CLOCK_NORMAL);
#else
    sdhost_set_clock(&self->host, SD_CLOCK_NORMAL);
#endif

    // A small wait before the voltage switch
    delayus(5000);

#ifndef USE_SDHOST

    // Switch to 1.8V mode if possible
    if (self->card_supports_18v) {
        trace("switching to 1.8V mode");
        // As per HCSS 3.6.1

        // Send VOLTAGE_SWITCH
        if (!emmc_issue_command(self, VOLTAGE_SWITCH, 0, 500000)) {
            error("error issuing VOLTAGE_SWITCH");
            self->failed_voltage_switch = 1;
            emmc_power_off();
            return -1;
        }

        // Disable SD clock
        control1 = get32(EMMC_CONTROL1);
        control1 &= ~(1 << 2);
        put32(EMMC_CONTROL1, control1);

        // Check DAT[3:0]
        status_reg = get32(EMMC_STATUS);
        uint32_t dat30 = (status_reg >> 20) & 0xf;
        if (dat30 != 0) {
            error("DAT[3:0] did not settle to 0");
            self->failed_voltage_switch = 1;
            emmc_power_off();
            return -1;
        }

        // Set 1.8V signal enable to 1
        uint32_t control0 = get32(EMMC_CONTROL0);
        control0 |= (1 << 8);
        put32(EMMC_CONTROL0, control0);

        // Wait 5 ms
        delayus(5000);

        // Check the 1.8V signal enable is set
        control0 = get32(EMMC_CONTROL0);
        if (((control0 >> 8) & 1) == 0) {
            error("controller did not keep 1.8V signal enable high");
            self->failed_voltage_switch = 1;
            emmc_power_off();
            return -1;
        }

        // Re-enable the SD clock
        control1 = get32(EMMC_CONTROL1);
        control1 |= (1 << 2);
        put32(EMMC_CONTROL1, control1);

        delayus(10000);

        // Check DAT[3:0]
        status_reg = get32(EMMC_STATUS);
        dat30 = (status_reg >> 20) & 0xf;
        if (dat30 != 0xf) {
            error("DAT[3:0] did not settle to 1111b (%01x)", dat30);
            self->failed_voltage_switch = 1;
            emmc_power_off();

            return -1;
        }
        trace("voltage switch complete");
    }

#endif

    // Send CMD2 to get the cards CID
    if (!emmc_issue_command(self, ALL_SEND_CID, 0, 500000)) {
        error("error sending ALL_SEND_CID");
        return -1;
    }
    self->device_id[0] = self->last_r0;
    self->device_id[1] = self->last_r1;
    self->device_id[2] = self->last_r2;
    self->device_id[3] = self->last_r3;
    debug("card CID: 0x%x, 0x%x, 0x%x, 0x%x", self->device_id[3],
          self->device_id[2], self->device_id[1], self->device_id[0]);


    // Send CMD3 to enter the data state
    if (!emmc_issue_command(self, SEND_RELATIVE_ADDR, 0, 500000)) {
        error("error sending SEND_RELATIVE_ADDR");
        return -1;
    }


    uint32_t cmd3_resp = self->last_r0;
    trace("cmd3 response: 0x%x", cmd3_resp);

    self->card_rca = (cmd3_resp >> 16) & 0xffff;
    uint32_t crc_error = (cmd3_resp >> 15) & 0x1;
    uint32_t illegal_cmd = (cmd3_resp >> 14) & 0x1;
    uint32_t error = (cmd3_resp >> 13) & 0x1;
    uint32_t status = (cmd3_resp >> 9) & 0xf;
    uint32_t ready = (cmd3_resp >> 8) & 0x1;

    if (crc_error || illegal_cmd || error || !ready) {
        error("CMD3 response error");
        return -1;
    }
    debug("RCA: 0x%x", self->card_rca);


    // Now select the card (toggles it to transfer state)
    if (!emmc_issue_command
        (self, SELECT_CARD, self->card_rca << 16, 500000)) {
        error("error sending CMD7");
        return -1;
    }

    uint32_t cmd7_resp = self->last_r0;
    status = (cmd7_resp >> 9) & 0xf;

    if ((status != 3) && (status != 4)) {
        error("invalid status %d", status);
        return -1;
    }

    // If not an SDHC card, ensure BLOCKLEN is 512 bytes
    if (!self->card_supports_sdhc) {
        if (!emmc_issue_command(self, SET_BLOCKLEN, SD_BLOCK_SIZE, 500000)) {
            error("error sending SET_BLOCKLEN");
            return -1;
        }
    }

#ifndef USE_SDHOST
    uint32_t controller_block_size = get32(EMMC_BLKSIZECNT);
    controller_block_size &= (~0xfff);
    controller_block_size |= 0x200;
    put32(EMMC_BLKSIZECNT, controller_block_size);
#endif

    // Get the cards SCR register
    self->buf = &(self->scr.scr[0]);
    self->block_size = 8;
    self->blocks_to_transfer = 1;

    emmc_issue_command(self, SEND_SCR, 0, 1000000);

    self->block_size = SD_BLOCK_SIZE;
    if (FAIL(self)) {
        error("error sending SEND_SCR");
        return -2;
    }

    // Determine card version
    // Note that the SCR is big-endian
    uint32_t scr0 = be2le32(self->scr.scr[0]);
    self->scr.sd_version = SD_VER_UNKNOWN;
    uint32_t sd_spec = (scr0 >> (56 - 32)) & 0xf;
    uint32_t sd_spec3 = (scr0 >> (47 - 32)) & 0x1;
    uint32_t sd_spec4 = (scr0 >> (42 - 32)) & 0x1;
    self->scr.sd_bus_widths = (scr0 >> (48 - 32)) & 0xf;
    if (sd_spec == 0) {
        self->scr.sd_version = SD_VER_1;
    } else if (sd_spec == 1) {
        self->scr.sd_version = SD_VER_1_1;
    } else if (sd_spec == 2) {
        if (sd_spec3 == 0) {
            self->scr.sd_version = SD_VER_2;
        } else if (sd_spec3 == 1) {
            if (sd_spec4 == 0) {
                self->scr.sd_version = SD_VER_3;
            } else if (sd_spec4 == 1) {
                self->scr.sd_version = SD_VER_4;
            }
        }
    }
    debug("SCR: version %s, bus_widths 0x%x",
          sd_versions[self->scr.sd_version], self->scr.sd_bus_widths);


#ifdef SD_HIGH_SPEED
    // If card supports CMD6, read switch information from card
    if (self->scr.sd_version >= SD_VER_1_1) {
        // 512 bit response
        uint8_t cmd6_resp[64];
        self->buf = &cmd6_resp[0];
        self->block_size = 64;

        // CMD6 Mode 0: Check Function (Group 1, Access Mode)
        if (!emmc_issue_command(self, SWITCH_FUNC, 0x00fffff0, 100000)) {
            error("error sending SWITCH_FUNC (Mode 0)");
        } else {
            // Check Group 1, Function 1 (High Speed/SDR25)
            self->card_supports_hs = (cmd6_resp[13] >> 1) & 0x1;

            // Attempt switch if supported
            if (self->card_supports_hs) {
                trace("switching to %s mode",
                      self->card_supports_18v ? "SDR25" : "High Speed");

                // CMD6 Mode 1: Set Function (Group 1, Access Mode = High Speed/SDR25)
                if (!emmc_issue_command
                    (self, SWITCH_FUNC, 0x80fffff1, 100000)) {
                    error("failed to switch to %s mode",
                          self->card_supports_18v ? "SDR25" :
                          "High Speed");
                } else {
                    // Success; switch clock to 50MHz
#ifndef USE_SDHOST
                    emmc_switch_clock_rate(base_clock, SD_CLOCK_HIGH);
#else
                    sdhost_set_clock(&self->host, SD_CLOCK_HIGH);
#endif
                    trace("switch to 50MHz clock complete");
                }
            }
        }

        // Restore block size
        self->block_size = SD_BLOCK_SIZE;
    }
#endif



    if (self->scr.sd_bus_widths & 4) {
        // Set 4-bit transfer mode (ACMD6)
        // See HCSS 3.4 for the algorithm
#ifdef SD_4BIT_DATA
        trace("switching to 4-bit data mode");
#ifndef USE_SDHOST
        // Disable card interrupt in host
        uint32_t old_irpt_mask = get32(EMMC_IRPT_MASK);
        uint32_t new_iprt_mask = old_irpt_mask & ~(1 << 8);
        put32(EMMC_IRPT_MASK, new_iprt_mask);
#endif
        // Send ACMD6 to change the card's bit mode
        if (!emmc_issue_command(self, SET_BUS_WIDTH, 2, 500000)) {
            error("failed to switch to 4-bit data mode");
        } else {
#ifndef USE_SDHOST
            // Change bit mode for Host
            uint32_t control0 = get32(EMMC_CONTROL0);
            control0 |= 0x2;
            put32(EMMC_CONTROL0, control0);

            // Re-enable card interrupt in host
            put32(EMMC_IRPT_MASK, old_irpt_mask);
#else
            // Change bit mode for Host
            sdhost_set_bus_width(&self->host, 4);
#endif
            trace("switch to 4-bit complete");
        }
#endif
    }

    info("found valid version %s SD card",
         sd_versions[self->scr.sd_version]);

#ifndef USE_SDHOST
    // Reset interrupt register
    put32(EMMC_INTERRUPT, 0xffffffff);
#endif

    return 0;
}

static int
emmc_issue_command(struct emmc *self, uint32_t cmd, uint32_t arg,
                   int timeout)
{
#ifndef USE_SDHOST
    // First, handle any pending interrupts
    emmc_handle_interrupts(self);

    // Stop the command issue if it was the card remove interrupt that was handled
    if (self->card_removal) {
        self->last_cmd_success = 0;
        return 0;
    }
#endif

    // Now run the appropriate commands by calling IssueCommandInt()
    if (cmd & IS_APP_CMD) {
        cmd &= 0xff;
        trace("issuing command ACMD%d", cmd);

        if (sd_acommands[cmd] == SD_CMD_RESERVED(0)) {
            error("invalid command ACMD%d", cmd);
            self->last_cmd_success = 0;
            return 0;
        }
        self->last_cmd = APP_CMD;

        uint32_t rca = 0;
        if (self->card_rca) {
            rca = self->card_rca << 16;
        }
        emmc_issue_command_int(self, sd_commands[APP_CMD], rca, timeout);
        if (self->last_cmd_success) {
            self->last_cmd = cmd | IS_APP_CMD;
            emmc_issue_command_int(self, sd_acommands[cmd], arg, timeout);
        }
    } else {
        if (sd_commands[cmd] == SD_CMD_RESERVED(0)) {
            error("invalid command CMD%d", cmd);
            self->last_cmd_success = 0;
            return 0;
        }

        self->last_cmd = cmd;
        emmc_issue_command_int(self, sd_commands[cmd], arg, timeout);
    }
    return self->last_cmd_success;
}
