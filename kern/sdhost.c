/*
 * BCM2835 SD host driver.
 *
 * Author:      Phil Elwell <phil@raspberrypi.org>
 *              Copyright (C) 2015-2016 Raspberry Pi (Trading) Ltd.
 *
 * Based on
 *  mmc-bcm2835.c by Gellert Weisz
 * which is, in turn, based on
 *  sdhci-bcm2708.c by Broadcom
 *  sdhci-bcm2835.c by Stephen Warren and Oleksandr Tymoshenko
 *  sdhci.c and sdhci-pci.c by Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gpio.h"
#include "mbox.h"
#include "sdhost.h"

#include "arm.h"
#include "string.h"
#include "console.h"

/* SDHOST Controller (SD Card) */
#define ARM_SDHOST_BASE            (MMIO_BASE + 0x202000)

#define FIFO_READ_THRESHOLD     4
#define FIFO_WRITE_THRESHOLD    4
#define ALLOW_CMD23_READ        1
#define ALLOW_CMD23_WRITE       0
#define ENABLE_LOG              1
#define SDDATA_FIFO_PIO_BURST   8
#define CMD_DALLY_US            1

#define SDCMD  0x00             /* Command to SD card              - 16 R/W */
#define SDARG  0x04             /* Argument to SD card             - 32 R/W */
#define SDTOUT 0x08             /* Start value for timeout counter - 32 R/W */
#define SDCDIV 0x0c             /* Start value for clock divider   - 11 R/W */
#define SDRSP0 0x10             /* SD card response (31:0)         - 32 R   */
#define SDRSP1 0x14             /* SD card response (63:32)        - 32 R   */
#define SDRSP2 0x18             /* SD card response (95:64)        - 32 R   */
#define SDRSP3 0x1c             /* SD card response (127:96)       - 32 R   */
#define SDHSTS 0x20             /* SD host status                  - 11 R   */
#define SDVDD  0x30             /* SD card power control           -  1 R/W */
#define SDEDM  0x34             /* Emergency Debug Mode            - 13 R/W */
#define SDHCFG 0x38             /* Host configuration              -  2 R/W */
#define SDHBCT 0x3c             /* Host byte count (debug)         - 32 R/W */
#define SDDATA 0x40             /* Data to/from SD card            - 32 R/W */
#define SDHBLC 0x50             /* Host block count (SDIO/SDHC)    -  9 R/W */

#define SDCMD_NEW_FLAG                  0x8000
#define SDCMD_FAIL_FLAG                 0x4000
#define SDCMD_BUSYWAIT                  0x800
#define SDCMD_NO_RESPONSE               0x400
#define SDCMD_LONG_RESPONSE             0x200
#define SDCMD_WRITE_CMD                 0x80
#define SDCMD_READ_CMD                  0x40
#define SDCMD_CMD_MASK                  0x3f

#define SDCDIV_MAX_CDIV                 0x7ff

#define SDHSTS_BUSY_IRPT                0x400
#define SDHSTS_BLOCK_IRPT               0x200
#define SDHSTS_SDIO_IRPT                0x100
#define SDHSTS_REW_TIME_OUT             0x80
#define SDHSTS_CMD_TIME_OUT             0x40
#define SDHSTS_CRC16_ERROR              0x20
#define SDHSTS_CRC7_ERROR               0x10
#define SDHSTS_FIFO_ERROR               0x08
/* Reserved */
/* Reserved */
#define SDHSTS_DATA_FLAG                0x01

#define SDHSTS_TRANSFER_ERROR_MASK      (SDHSTS_CRC7_ERROR|SDHSTS_CRC16_ERROR|SDHSTS_REW_TIME_OUT|SDHSTS_FIFO_ERROR)
#define SDHSTS_ERROR_MASK               (SDHSTS_CMD_TIME_OUT|SDHSTS_TRANSFER_ERROR_MASK)

#define SDHCFG_BUSY_IRPT_EN     (1<<10)
#define SDHCFG_BLOCK_IRPT_EN    (1<<8)
#define SDHCFG_SDIO_IRPT_EN     (1<<5)
#define SDHCFG_DATA_IRPT_EN     (1<<4)
#define SDHCFG_SLOW_CARD        (1<<3)
#define SDHCFG_WIDE_EXT_BUS     (1<<2)
#define SDHCFG_WIDE_INT_BUS     (1<<1)
#define SDHCFG_REL_CMD_LINE     (1<<0)

#define SDEDM_FORCE_DATA_MODE   (1<<19)
#define SDEDM_CLOCK_PULSE       (1<<20)
#define SDEDM_BYPASS            (1<<21)

#define SDEDM_WRITE_THRESHOLD_SHIFT 9
#define SDEDM_READ_THRESHOLD_SHIFT 14
#define SDEDM_THRESHOLD_MASK     0x1f

#define SDEDM_FSM_MASK           0xf
#define SDEDM_FSM_IDENTMODE      0x0
#define SDEDM_FSM_DATAMODE       0x1
#define SDEDM_FSM_READDATA       0x2
#define SDEDM_FSM_WRITEDATA      0x3
#define SDEDM_FSM_READWAIT       0x4
#define SDEDM_FSM_READCRC        0x5
#define SDEDM_FSM_WRITECRC       0x6
#define SDEDM_FSM_WRITEWAIT1     0x7
#define SDEDM_FSM_POWERDOWN      0x8
#define SDEDM_FSM_POWERUP        0x9
#define SDEDM_FSM_WRITESTART1    0xa
#define SDEDM_FSM_WRITESTART2    0xb
#define SDEDM_FSM_GENPULSES      0xc
#define SDEDM_FSM_WRITEWAIT2     0xd
#define SDEDM_FSM_STARTPOWDOWN   0xf

#define SDDATA_FIFO_WORDS        16

#define USE_CMD23_FLAGS          ((ALLOW_CMD23_READ * MMC_DATA_READ) | \
                  (ALLOW_CMD23_WRITE * MMC_DATA_WRITE))

#define MHZ 1000000

// Timer
#define CLOCKHZ    1000000

static void sdhost_init_gpio();
static int mmc_prepare_request(struct mmc_host *mmc,
                               struct mmc_request *req);
static void mmc_request_done(struct mmc_host *mmc,
                             struct mmc_request *req);

void sdhost_request_sync(struct bcm2835_host *host,
                         struct mmc_request *req);

static int sdhost_add_host(struct bcm2835_host *host);

static void sdhost_init_inner(struct bcm2835_host *host, int soft);
static void sdhost_wait_transfer_complete(struct bcm2835_host *host);
static void sdhost_read_block_pio(struct bcm2835_host *host);
static void sdhost_write_block_pio(struct bcm2835_host *host);
static void sdhost_transfer_pio(struct bcm2835_host *host);

static void sdhost_prepare_data(struct bcm2835_host *host,
                                struct mmc_command *cmd);
static int sdhost_send_command(struct bcm2835_host *host,
                               struct mmc_command *cmd);
static void sdhost_finish_data(struct bcm2835_host *host);
static void sdhost_finish_command(struct bcm2835_host *host);
static void sdhost_transfer_complete(struct bcm2835_host *host);

static void sdhost_busy_irq(struct bcm2835_host *host, uint32_t intmask);
static void sdhost_data_irq(struct bcm2835_host *host, uint32_t intmask);
static void sdhost_block_irq(struct bcm2835_host *host, uint32_t intmask);
static void sdhost_irq_handler(struct bcm2835_host *host);

static void sdhost_set_clock_inner(struct bcm2835_host *host,
                                   uint32_t clock);
static void sdhost_request(struct bcm2835_host *host, struct mmc_host *mmc,
                           struct mmc_request *mrq);
static void sdhost_set_ios(struct bcm2835_host *host, struct mmc_ios *ios);
static void sdhost_tasklet_finish(struct bcm2835_host *host);
static int sdhost_probe(struct bcm2835_host *host);

int
sdhost_init(struct bcm2835_host *host, void (*sleep_fn)(void *),
            void *sleep_arg)
{
    sdhost_init_gpio();
    int ret = sdhost_probe(host);
    host->sleep_fn = sleep_fn;
    host->sleep_arg = sleep_arg;
    if (ret != 0)
        return 0;
    return 1;
}

void
sdhost_intr(struct bcm2835_host *host)
{
    trace("begin");
    sdhost_irq_handler(host);
    trace("end");
}

void
sdhost_set_clock(struct bcm2835_host *host, uint32_t clock)
{
    host->mmc.ios.clock = clock;
    sdhost_set_ios(host, &host->mmc.ios);
}

void
sdhost_set_bus_width(struct bcm2835_host *host, uint32_t nbits)
{
    assert(nbits == 1 || nbits == 4);
    host->mmc.ios.bus_width = nbits == 4 ? MMC_BUS_WIDTH_4 : 0;

    sdhost_set_ios(host, &(host->mmc.ios));
}

int
sdhost_command(struct bcm2835_host *host, struct mmc_command *cmd,
               uint32_t nretries)
{
    assert(cmd != 0);
    cmd->retries = nretries;
    memset(cmd->resp, 0, sizeof(cmd->resp));

    struct mmc_request req;
    memset(&req, 0, sizeof(req));
    req.cmd = cmd;
    req.data = cmd->data;

    struct mmc_command stop_cmd;
    if (cmd->opcode == MMC_READ_MULTIPLE_BLOCK
        || cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) {
        assert(cmd->data != 0);

        memset(&stop_cmd, 0, sizeof(stop_cmd));
        stop_cmd.opcode = MMC_STOP_TRANSMISSION;
        stop_cmd.flags = MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_BUSY;

        req.stop = &stop_cmd;
    }

    int nerror = mmc_prepare_request(&host->mmc, &req);
    if (nerror != 0) {
        return nerror;
    }

    sdhost_request_sync(host, &req);

    if (cmd->error != 0) {
        return cmd->error;
    }

    if (cmd->data != 0 && cmd->data->error != 0) {
        return cmd->data->error;
    }

    if (req.stop != 0 && req.stop->error != 0) {
        return req.stop->error;
    }
    return 0;
}

void
sdhost_request_sync(struct bcm2835_host *host, struct mmc_request *req)
{
    assert(req != 0);
    req->done = 0;

    sdhost_request(host, &host->mmc, req);

    // Caller must hold host->lock.
    while (!req->done) {
        host->sleep_fn(host->sleep_arg);
        disb();
    }
}

static int
mmc_prepare_request(struct mmc_host *mmc, struct mmc_request *req)
{
    assert(req != 0);

    if (req->cmd) {
        req->cmd->error = 0;
        req->cmd->data = req->data;
    }

    if (req->sbc) {
        req->sbc->error = 0;
    }

    if (req->data) {
        size_t nreqsz = req->data->blocks * req->data->blksz;

        if (req->data->blksz > mmc->max_blk_size
            || req->data->blocks > mmc->max_blk_count
            || nreqsz > mmc->max_req_size || nreqsz != req->data->sg_len) {
            return -EINVAL;
        }

        req->data->error = 0;
        req->data->mrq = req;

        if (req->stop) {
            req->data->stop = req->stop;
            req->stop->error = 0;
        }
    }
    return 0;
}

static void
mmc_request_done(struct mmc_host *mmc, struct mmc_request *req)
{
    assert(req != 0);
    req->done = 1;
    disb();
}

static void
sg_miter_start(struct sg_mapping_iter *sg_miter, void *sg, uint32_t sg_len,
               uint32_t flags)
{
    assert(sg_miter != 0);
    assert(sg != 0);
    assert(sg_len > 0);

    sg_miter->addr = sg;
    sg_miter->length = sg_len;
    sg_miter->consumed = 0;
}

static int
sg_miter_next(struct sg_mapping_iter *sg_miter)
{
    assert(sg_miter != 0);
    assert(sg_miter->consumed <= sg_miter->length);

    sg_miter->length -= sg_miter->consumed;
    if (sg_miter->length == 0) {
        return 0;
    }

    sg_miter->addr = (uint8_t *) sg_miter->addr + sg_miter->consumed;
    sg_miter->consumed = 0;
    return 1;
}

static void
sg_miter_stop(struct sg_mapping_iter *sg_miter)
{
}

static void
write(uint32_t val, int reg)
{
    put32(ARM_SDHOST_BASE + reg, val);
}

static uint32_t
read(int reg)
{
    return get32(ARM_SDHOST_BASE + reg);
}

static void
sdhost_init_gpio()
{
    for (int i = 0; i <= 5; i++) {
        int64_t npin, off, shift, sel, val;
        // Wifi
        npin = 34 + i;
        off = npin / 10 * 4, shift = (npin % 10) * 3;

        sel = GPIO_BASE + off;
        val = get32(sel);
        val &= ~(7 << shift);
        put32(sel, val);

        // SD card
        npin = 48 + i;
        off = npin / 10 * 4, shift = (npin % 10) * 3;

        sel = GPIO_BASE + off;
        val = get32(sel);
        val &= ~(7 << shift);
        val |= 4 << shift;      // Set alt0
        put32(sel, val);

        // See: http://www.raspberrypi.org/forums/viewtopic.php?t=163352&p=1059178#p1059178
        sel = GPPUDCLK0 + off;
        put32(GPPUD, i == 0 ? 0 : 2);   // Pull mode off/up
        delayus(5);             // Delay 1 us.
        put32(sel, 1 << (npin % 32));
        delayus(5);
        put32(GPPUD, 0);
        put32(sel, 0);
    }
}


static void
sdhost_set_power(int on)
{
    write(on ? 1 : 0, SDVDD);
}

static void
sdhost_reset_internal(struct bcm2835_host *host)
{
    sdhost_set_power(0);

    write(0, SDCMD);
    write(0, SDARG);
    write(0xf00000, SDTOUT);
    write(0, SDCDIV);
    write(0x7f8, SDHSTS);       /* Write 1s to clear */
    write(0, SDHCFG);
    write(0, SDHBCT);
    write(0, SDHBLC);

    /* Limit fifo usage due to silicon bug */
    uint32_t temp = read(SDEDM);
    temp &= ~((SDEDM_THRESHOLD_MASK << SDEDM_READ_THRESHOLD_SHIFT) |
              (SDEDM_THRESHOLD_MASK << SDEDM_WRITE_THRESHOLD_SHIFT));
    temp |= (FIFO_READ_THRESHOLD << SDEDM_READ_THRESHOLD_SHIFT) |
        (FIFO_WRITE_THRESHOLD << SDEDM_WRITE_THRESHOLD_SHIFT);
    write(temp, SDEDM);
    delayus(10 * 1000);
    sdhost_set_power(1);
    delayus(10 * 1000);
    host->clock = 0;
//     host->sectors = 0;
    write(host->hcfg, SDHCFG);
    write(SDCDIV_MAX_CDIV, SDCDIV);
    dsb();
}

static void
sdhost_reset(struct bcm2835_host *host)
{
    sdhost_reset_internal(host);
}

static void
sdhost_init_inner(struct bcm2835_host *host, int soft)
{
    trace("soft %d", soft);

    /* Set interrupt enables */
    host->hcfg = SDHCFG_BUSY_IRPT_EN;

    sdhost_reset_internal(host);

    if (soft) {
        /* force clock reconfiguration */
        host->clock = 0;
        sdhost_set_ios(host, &host->mmc.ios);
    }
}

static void
sdhost_wait_transfer_complete(struct bcm2835_host *host)
{
    int timediff;
    uint32_t alternate_idle;
    uint32_t edm;

    alternate_idle = (host->mrq->data->flags & MMC_DATA_READ) ?
        SDEDM_FSM_READWAIT : SDEDM_FSM_WRITESTART1;

    edm = read(SDEDM);

    timediff = 0;

    while (1) {
        uint32_t fsm = edm & SDEDM_FSM_MASK;
        if ((fsm == SDEDM_FSM_IDENTMODE) || (fsm == SDEDM_FSM_DATAMODE))
            break;
        if (fsm == alternate_idle) {
            write(edm | SDEDM_FORCE_DATA_MODE, SDEDM);
            break;
        }

        timediff++;
        if (timediff == 100000) {
            error("still waiting after %d retries", timediff);
            // dumpregs();
            host->mrq->data->error = -ETIMEDOUT;
            return;
        }
        dsb();
        edm = read(SDEDM);
    }
}


static void
sdhost_read_block_pio(struct bcm2835_host *host)
{
    size_t blksize = host->data->blksz;

    uint64_t start = timestamp();

    while (blksize) {
        int copy_words;
        uint32_t hsts = 0;

        if (!sg_miter_next(&host->sg_miter)) {
            host->data->error = -EINVAL;
            break;
        }

        size_t len = MIN(host->sg_miter.length, blksize);
        if (len % 4) {
            host->data->error = -EINVAL;
            break;
        }

        blksize -= len;
        host->sg_miter.consumed = len;

        uint32_t *buf = (uint32_t *) host->sg_miter.addr;

        copy_words = len / 4;

        while (copy_words) {
            int burst_words, words;
            uint32_t edm;

            burst_words = SDDATA_FIFO_PIO_BURST;
            if (burst_words > copy_words)
                burst_words = copy_words;
            edm = read(SDEDM);
            words = ((edm >> 4) & 0x1f);

            if (words < burst_words) {
                int fsm_state = (edm & SDEDM_FSM_MASK);
                if ((fsm_state != SDEDM_FSM_READDATA) &&
                    (fsm_state != SDEDM_FSM_READWAIT) &&
                    (fsm_state != SDEDM_FSM_READCRC)) {
                    hsts = read(SDHSTS);
                    debug("fsm 0x%x, hsts 0x%x", fsm_state, hsts);
                    if (hsts & SDHSTS_ERROR_MASK)
                        break;
                }

                if (timestamp() - start > host->pio_timeout) {
                    error("PIO read timeout - EDM 0x%x", edm);
                    hsts = SDHSTS_REW_TIME_OUT;
                    break;
                }
                delayus(((burst_words - words) *
                         host->ns_per_fifo_word) / 1000);
                continue;
            } else if (words > copy_words) {
                words = copy_words;
            }

            copy_words -= words;

            while (words) {
                *(buf++) = read(SDDATA);
                words--;
            }
        }

        if (hsts & SDHSTS_ERROR_MASK)
            break;
    }
    sg_miter_stop(&host->sg_miter);
}

static void
sdhost_write_block_pio(struct bcm2835_host *host)
{
    size_t blksize = host->data->blksz;

    uint64_t start = timestamp();

    while (blksize) {
        int copy_words;
        uint32_t hsts = 0;

        if (!sg_miter_next(&host->sg_miter)) {
            host->data->error = -EINVAL;
            break;
        }

        size_t len = MIN(host->sg_miter.length, blksize);
        if (len % 4) {
            host->data->error = -EINVAL;
            break;
        }

        blksize -= len;
        host->sg_miter.consumed = len;

        uint32_t *buf = (uint32_t *) host->sg_miter.addr;

        copy_words = len / 4;

        while (copy_words) {
            int burst_words, words;
            uint32_t edm;

            burst_words = SDDATA_FIFO_PIO_BURST;
            if (burst_words > copy_words)
                burst_words = copy_words;
            edm = read(SDEDM);
            words = SDDATA_FIFO_WORDS - ((edm >> 4) & 0x1f);

            if (words < burst_words) {
                int fsm_state = (edm & SDEDM_FSM_MASK);
                if ((fsm_state != SDEDM_FSM_WRITEDATA) &&
                    (fsm_state != SDEDM_FSM_WRITESTART1) &&
                    (fsm_state != SDEDM_FSM_WRITESTART2)) {
                    hsts = read(SDHSTS);
                    debug("fsm 0x%x, hsts 0x%x", fsm_state, hsts);
                    if (hsts & SDHSTS_ERROR_MASK)
                        break;
                }

                if (timestamp() - start > host->pio_timeout) {
                    error("PIO write timeout - EDM 0x%x", edm);
                    hsts = SDHSTS_REW_TIME_OUT;
                    break;
                }
                delayus(((burst_words - words) *
                         host->ns_per_fifo_word) / 1000);
                continue;
            } else if (words > copy_words) {
                words = copy_words;
            }
            copy_words -= words;

            while (words) {
                write(*(buf++), SDDATA);
                words--;
            }
        }
        if (hsts & SDHSTS_ERROR_MASK)
            break;
    }
    sg_miter_stop(&host->sg_miter);
}


static void
sdhost_transfer_pio(struct bcm2835_host *host)
{
    assert(host->data);

    int is_read = (host->data->flags & MMC_DATA_READ) != 0;
    if (is_read)
        sdhost_read_block_pio(host);
    else
        sdhost_write_block_pio(host);

    uint32_t sdhsts = read(SDHSTS);
    if (sdhsts & (SDHSTS_CRC16_ERROR |
                  SDHSTS_CRC7_ERROR | SDHSTS_FIFO_ERROR)) {
        error("%s transfer error - HSTS 0x%x", is_read ? "read" : "write",
              sdhsts);
        host->data->error = -EILSEQ;
    } else if ((sdhsts & (SDHSTS_CMD_TIME_OUT | SDHSTS_REW_TIME_OUT))) {
        error("%s timeout error - HSTS 0x%x", is_read ? "read" : "write",
              sdhsts);
        host->data->error = -ETIMEDOUT;
    }
}

static void
sdhost_set_transfer_irqs(struct bcm2835_host *host)
{
    uint32_t all_irqs =
        SDHCFG_DATA_IRPT_EN | SDHCFG_BLOCK_IRPT_EN | SDHCFG_BUSY_IRPT_EN;

    host->hcfg =
        (host->hcfg & ~all_irqs) | SDHCFG_DATA_IRPT_EN |
        SDHCFG_BUSY_IRPT_EN;

    write(host->hcfg, SDHCFG);
}

static void
sdhost_prepare_data(struct bcm2835_host *host, struct mmc_command *cmd)
{
    struct mmc_data *data = cmd->data;

    assert(!(host->data));

    host->data = data;
    if (!data)
        return;

    /* Sanity checks */
    assert(!(data->blksz * data->blocks > 524288));
    assert(!(data->blksz > host->mmc.max_blk_size));
    assert(!(data->blocks > 65535));

    host->data_complete = 0;
    host->data->bytes_xfered = 0;

//     if (!host->sectors && host->mmc->card) {
//         struct mmc_card *card = host->mmc->card;
//         if (!mmc_card_sd(card) && mmc_card_blockaddr(card)) {
//             /*
//              * The EXT_CSD sector count is in number of 512 byte
//              * sectors.
//              */
//             host->sectors = card->ext_csd.sectors;
//         } else {
//             /*
//              * The CSD capacity field is in units of read_blkbits.
//              * set_capacity takes units of 512 bytes.
//              */
//             host->sectors = card->csd.capacity <<
//                 (card->csd.read_blkbits - 9);
//         }
//     }

    int flags = SG_MITER_ATOMIC;

    if (data->flags & MMC_DATA_READ)
        flags |= SG_MITER_TO_SG;
    else
        flags |= SG_MITER_FROM_SG;
    sg_miter_start(&host->sg_miter, data->sg, data->sg_len, flags);
    host->blocks = data->blocks;

    sdhost_set_transfer_irqs(host);

    write(data->blksz, SDHBCT);
    write(data->blocks, SDHBLC);

    assert(host->data);
}


static int
sdhost_send_command(struct bcm2835_host *host, struct mmc_command *cmd)
{
//     WARN_ON(host->cmd);

    if (cmd->data) {
        trace("send_command %d 0x%x "
              "(flags 0x%x) - %s %d*%d",
              cmd->opcode, cmd->arg, cmd->flags,
              (cmd->data->flags & MMC_DATA_READ) ?
              "read" : "write", cmd->data->blocks, cmd->data->blksz);
    } else {
        trace("send_command %d 0x%x (flags 0x%x)",
              cmd->opcode, cmd->arg, cmd->flags);
    }

    /* Wait max 100 ms */
    uint32_t timeout = 10000;

    while (read(SDCMD) & SDCMD_NEW_FLAG) {
        if (timeout == 0) {
            warn("previous command never completed");
            // tasklet_schedule(&host->finish_tasklet);
            sdhost_tasklet_finish(host);
            return 0;
        }
        timeout--;
        delayus(10);
    }

    int delay = (10000 - timeout) / 100;
    if (delay > host->max_delay) {
        host->max_delay = delay;
        warn("controller hung for %d ms", host->max_delay);
    }

    //     timeout = jiffies;
//     if (!cmd->data && cmd->busy_timeout > 9000)
//         timeout += DIV_ROUND_UP(cmd->busy_timeout, 1000) * HZ + HZ;
//     else
//         timeout += 10 * HZ;
//     mod_timer(&host->timer, timeout);

    host->cmd = cmd;

    /* Clear any error flags */
    uint32_t sdhsts = read(SDHSTS);
    if (sdhsts & SDHSTS_ERROR_MASK)
        write(sdhsts, SDHSTS);

    if ((cmd->flags & MMC_RSP_136) && (cmd->flags & MMC_RSP_BUSY)) {
        error("unsupported response type");
        cmd->error = -EINVAL;
        // tasklet_schedule(&host->finish_tasklet);
        sdhost_tasklet_finish(host);
        return 0;
    }

    sdhost_prepare_data(host, cmd);

    write(cmd->arg, SDARG);

    uint32_t sdcmd = cmd->opcode & SDCMD_CMD_MASK;

    host->use_busy = 0;
    if (!(cmd->flags & MMC_RSP_PRESENT)) {
        sdcmd |= SDCMD_NO_RESPONSE;
    } else {
        if (cmd->flags & MMC_RSP_136)
            sdcmd |= SDCMD_LONG_RESPONSE;
        if (cmd->flags & MMC_RSP_BUSY) {
            sdcmd |= SDCMD_BUSYWAIT;
            host->use_busy = 1;
        }
    }

    if (cmd->data) {
        if (host->delay_after_this_stop) {
            uint64_t now = timestamp() * (1000000U / CLOCKHZ);
            uint64_t time_since_stop = now - host->stop_time;
            if (time_since_stop < host->delay_after_this_stop)
                delayus(host->delay_after_this_stop - time_since_stop);
        }

        host->delay_after_this_stop = host->delay_after_stop;
//         if ((cmd->data->flags & MMC_DATA_READ) && !host->use_sbc) {
//             /* See if read crosses one of the hazardous sectors */
//             u32 first_blk, last_blk;
//
//             /* Intentionally include the following sector because
//                without CMD23/SBC the read may run on. */
//             first_blk = host->mrq->cmd->arg;
//             last_blk = first_blk + cmd->data->blocks;
//
//             if (((last_blk >= (host->sectors - 64)) &&
//                  (first_blk <= (host->sectors - 64))) ||
//                 ((last_blk >= (host->sectors - 32)) &&
//                  (first_blk <= (host->sectors - 32)))) {
//                 host->delay_after_this_stop =
//                     max(250u, host->delay_after_stop);
//             }
//         }

        if (cmd->data->flags & MMC_DATA_WRITE)
            sdcmd |= SDCMD_WRITE_CMD;
        if (cmd->data->flags & MMC_DATA_READ)
            sdcmd |= SDCMD_READ_CMD;
    }

    write(sdcmd | SDCMD_NEW_FLAG, SDCMD);

    trace("finish");
    return 1;
}


static void
sdhost_finish_data(struct bcm2835_host *host)
{
    struct mmc_data *data = host->data;
    assert(data);

    trace("finish_data(error %d, stop %d, sbc %d)",
          data->error, data->stop ? 1 : 0, host->mrq->sbc ? 1 : 0);

    host->hcfg &= ~(SDHCFG_DATA_IRPT_EN | SDHCFG_BLOCK_IRPT_EN);
    write(host->hcfg, SDHCFG);

    data->bytes_xfered = data->error ? 0 : (data->blksz * data->blocks);

    host->data_complete = 1;

    if (host->cmd) {
        /*
         * Data managed to finish before the
         * command completed. Make sure we do
         * things in the proper order.
         */
        debug("finished early - HSTS 0x%x", read(SDHSTS));
    } else {
        sdhost_transfer_complete(host);
    }
}


static void
sdhost_transfer_complete(struct bcm2835_host *host)
{
    assert(!(host->cmd));
    assert(host->data);
    assert(host->data_complete);

    struct mmc_data *data = host->data;
    host->data = 0;

    trace("transfer_complete(error %d, stop %d)",
          data->error, data->stop ? 1 : 0);

    /*
     * Need to send CMD12 if -
     * a) open-ended multiblock transfer (no CMD23)
     * b) error in multiblock transfer
     */
    if (host->mrq->stop && (data->error || !host->use_sbc)) {
        if (sdhost_send_command(host, host->mrq->stop)) {
            /* No busy, so poll for completion */
            if (!host->use_busy)
                sdhost_finish_command(host);

            if (host->delay_after_this_stop)
                host->stop_time = timestamp() * (1000000U / CLOCKHZ);
        }
    } else {
        sdhost_wait_transfer_complete(host);
        // tasklet_schedule(&host->finish_tasklet);
        sdhost_tasklet_finish(host);
    }
}


/*
 * If irq_flags is valid, the caller is in a thread context and is allowed
 * to sleep.
 */
static void
sdhost_finish_command(struct bcm2835_host *host)
{
    uint32_t sdcmd;

    trace("finish_command(0x%x)", read(SDCMD));

    assert(!(!host->cmd || !host->mrq));

    /* Poll quickly at first. */

    uint32_t retries = host->cmd_quick_poll_retries;
    if (!retries) {
        /* Work out how many polls take 1us by timing 10us */
        int us_diff;

        retries = 1;
        do {
            retries *= 2;

            uint64_t start = timestamp();

            for (uint32_t i = 0; i < retries; i++) {
                dsb();
                sdcmd = read(SDCMD);
            }

            uint64_t now = timestamp();
            us_diff = (now - start) * (1000000 / CLOCKHZ);
        } while (us_diff < 10);

        host->cmd_quick_poll_retries =
            ((retries * us_diff + 9) * CMD_DALLY_US) / 10 + 1;
        retries = 1;            // We've already waited long enough this time
    }

    for (sdcmd = read(SDCMD);
         (sdcmd & SDCMD_NEW_FLAG) && retries; retries--) {
        dsb();
        sdcmd = read(SDCMD);
    }

    if (!retries) {
//         if (!irq_flags) {
//             /* Schedule the work */
//             schedule_work(&host->cmd_wait_wq);
//             return;
//         }

        /* Wait max 100 ms */
        uint64_t start = timestamp();
        while (timestamp() - start < CLOCKHZ / 10) {
            delayus(10);
            sdcmd = read(SDCMD);
            if (!(sdcmd & SDCMD_NEW_FLAG))
                break;
        }
    }

    /* Check for errors */
    if (sdcmd & SDCMD_NEW_FLAG) {
        error("command %d never completed.", host->cmd->opcode);
        // dumpregs();
        host->cmd->error = -EILSEQ;
        // tasklet_schedule(&host->finish_tasklet);
        sdhost_tasklet_finish(host);
        return;
    } else if (sdcmd & SDCMD_FAIL_FLAG) {
        uint32_t sdhsts = read(SDHSTS);

        /* Clear the errors */
        write(SDHSTS_ERROR_MASK, SDHSTS);

        info("error detected: CMD 0x%x, HSTS 0x%x, EDM 0x%x",
             sdcmd, sdhsts, read(SDEDM));

        if ((sdhsts & SDHSTS_CRC7_ERROR) && (host->cmd->opcode == 1)) {
            info("ignoring CRC7 error for CMD1");
        } else {
            uint32_t edm, fsm;

            if (sdhsts & SDHSTS_CMD_TIME_OUT) {
                warn("command %d timeout", host->cmd->opcode);
                host->cmd->error = -ETIMEDOUT;
            } else {
                warn("unexpected command %d error", host->cmd->opcode);
                host->cmd->error = -EILSEQ;
            }

            edm = read(SDEDM);
            fsm = edm & SDEDM_FSM_MASK;
            if (fsm == SDEDM_FSM_READWAIT || fsm == SDEDM_FSM_WRITESTART1)
                write(edm | SDEDM_FORCE_DATA_MODE, SDEDM);
            // tasklet_schedule(&host->finish_tasklet);
            sdhost_tasklet_finish(host);
            return;
        }
    }

    if (host->cmd->flags & MMC_RSP_PRESENT) {
        if (host->cmd->flags & MMC_RSP_136) {
            int i;
            for (i = 0; i < 4; i++)
                host->cmd->resp[3 - i] = read(SDRSP0 + i * 4);
            trace("finish_command 0x%x 0x%x 0x%x 0x%x",
                  host->cmd->resp[0],
                  host->cmd->resp[1],
                  host->cmd->resp[2], host->cmd->resp[3]);
        } else {
            host->cmd->resp[0] = read(SDRSP0);
            trace("finish_command 0x%x", host->cmd->resp[0]);
        }
    }

    if (host->cmd == host->mrq->sbc) {
        /* Finished CMD23, now send actual command. */
        host->cmd = 0;
        if (sdhost_send_command(host, host->mrq->cmd)) {
            /* PIO starts after irq */

            if (!host->use_busy)
                sdhost_finish_command(host);
        }
    } else if (host->cmd == host->mrq->stop) {
        /* Finished CMD12 */
        // tasklet_schedule(&host->finish_tasklet);
        sdhost_tasklet_finish(host);
    } else {
        /* Processed actual command. */
        host->cmd = 0;
        if (!host->data) {
            // tasklet_schedule(&host->finish_tasklet);
            sdhost_tasklet_finish(host);
        } else if (host->data_complete) {
            sdhost_transfer_complete(host);
        }
    }
}

static void
sdhost_busy_irq(struct bcm2835_host *host, uint32_t intmask)
{
    if (!host->cmd) {
        error("got command busy interrupt 0x%x even "
              "though no command operation was in progress", intmask);
        // dumpregs();
        return;
    }

    if (!host->use_busy) {
        error("got command busy interrupt 0x%x even "
              "though not expecting one.", intmask);
        // dumpregs();
        return;
    }
    host->use_busy = 0;

    if (intmask & SDHSTS_ERROR_MASK) {
        error("sdhost_busy_irq: intmask 0x%x, data %p", intmask,
              host->mrq->data);
        if (intmask & SDHSTS_CRC7_ERROR) {
            host->cmd->error = -EILSEQ;
        } else if (intmask & (SDHSTS_CRC16_ERROR | SDHSTS_FIFO_ERROR)) {
            if (host->mrq->data)
                host->mrq->data->error = -EILSEQ;
            else
                host->cmd->error = -EILSEQ;
        } else if (intmask & SDHSTS_REW_TIME_OUT) {
            if (host->mrq->data)
                host->mrq->data->error = -ETIMEDOUT;
            else
                host->cmd->error = -ETIMEDOUT;
        } else if (intmask & SDHSTS_CMD_TIME_OUT) {
            host->cmd->error = -ETIMEDOUT;
        }

        // if (host->debug) {
        //      dumpregs();
        // }
    } else
        sdhost_finish_command(host);
}


static void
sdhost_data_irq(struct bcm2835_host *host, uint32_t intmask)
{
    /*
     * There are no dedicated data/space available interrupt
     * status bits, so it is necessary to use the single shared
     * data/space available FIFO status bits. It is therefore not
     * an error to get here when there is no data transfer in
     * progress.
     */
    if (!host->data)
        return;

    if (intmask & (SDHSTS_CRC16_ERROR |
                   SDHSTS_FIFO_ERROR | SDHSTS_REW_TIME_OUT)) {
        if (intmask & (SDHSTS_CRC16_ERROR | SDHSTS_FIFO_ERROR))
            host->data->error = -EILSEQ;
        else
            host->data->error = -ETIMEDOUT;

        // if (host->debug) {
        //      dumpregs();
        // }
    }

    if (host->data->error) {
        sdhost_finish_data(host);
    } else if (host->data->flags & MMC_DATA_WRITE) {
        trace("data write");
        /* Use the block interrupt for writes after the first block. */
        host->hcfg &= ~(SDHCFG_DATA_IRPT_EN);
        host->hcfg |= SDHCFG_BLOCK_IRPT_EN;
        write(host->hcfg, SDHCFG);
        sdhost_transfer_pio(host);
    } else {
        sdhost_transfer_pio(host);
        host->blocks--;
        if ((host->blocks == 0) || host->data->error)
            sdhost_finish_data(host);
    }
}

static void
sdhost_block_irq(struct bcm2835_host *host, uint32_t intmask)
{
    if (!host->data) {
        error("got block interrupt 0x%x even "
              "though no data operation was in progress.", intmask);
        // dumpregs();
        return;
    }

    if (intmask & (SDHSTS_CRC16_ERROR |
                   SDHSTS_FIFO_ERROR | SDHSTS_REW_TIME_OUT)) {
        if (intmask & (SDHSTS_CRC16_ERROR | SDHSTS_FIFO_ERROR))
            host->data->error = -EILSEQ;
        else
            host->data->error = -ETIMEDOUT;

        // if (host->debug) {
        //      dumpregs();
        // }
    }

    assert(host->blocks);
    if (host->data->error || (--host->blocks == 0)) {
        sdhost_finish_data(host);
    } else {
        sdhost_transfer_pio(host);
    }
}

static void
sdhost_irq_handler(struct bcm2835_host *host)
{
    uint32_t intmask = read(SDHSTS);

    write(SDHSTS_BUSY_IRPT | SDHSTS_BLOCK_IRPT
          | SDHSTS_SDIO_IRPT | SDHSTS_DATA_FLAG, SDHSTS);

    if (intmask & SDHSTS_BLOCK_IRPT) {
        sdhost_block_irq(host, intmask);
    }

    if (intmask & SDHSTS_BUSY_IRPT) {
        sdhost_busy_irq(host, intmask);
    }

    /* There is no true data interrupt status bit, so it is
       necessary to qualify the data flag with the interrupt
       enable bit */
    if ((intmask & SDHSTS_DATA_FLAG) && (host->hcfg & SDHCFG_DATA_IRPT_EN)) {
        sdhost_data_irq(host, intmask);
    }
    dsb();
}

static void
sdhost_set_clock_inner(struct bcm2835_host *host, uint32_t clock)
{
    uint32_t input_clock = clock;

    trace("clock %d", clock);

    if (host->overclock_50 && (clock == 50 * MHZ))
        clock = host->overclock_50 * MHZ + (MHZ - 1);

    /*
     * The SDCDIV register has 11 bits, and holds (div - 2).
     * But in data mode the max is 50MHz wihout a minimum, and only the
     * bottom 3 bits are used. Since the switch over is automatic (unless
     * we have marked the card as slow...), chosen values have to make
     * sense in both modes.
     * Ident mode must be 100-400KHz, so can range check the requested
     * clock. CMD15 must be used to return to data mode, so this can be
     * monitored.
     *
     * clock 250MHz -> 0->125MHz, 1->83.3MHz, 2->62.5MHz, 3->50.0MHz
     *                     4->41.7MHz, 5->35.7MHz, 6->31.3MHz, 7->27.8MHz
     *
     *       623->400KHz/27.8MHz
     *       reset value (507)->491159/50MHz
     *
     * BUT, the 3-bit clock divisor in data mode is too small if the
     * core clock is higher than 250MHz, so instead use the SLOW_CARD
     * configuration bit to force the use of the ident clock divisor
     * at all times.
     */

    host->mmc.actual_clock = 0;

    if (host->firmware_sets_cdiv) {
        uint32_t msg[3] = { clock, 0, 0 };
        mbox_set_sdhost_clock(msg);
        clock = MAX(msg[1], msg[2]);
    } else {
        if (clock < 100000) {
            /*
             * Can't stop the clock, but make it as slow as
             * possible to show willing
             */
            host->cdiv = SDCDIV_MAX_CDIV;
            write(host->cdiv, SDCDIV);
            dsb();
            return;
        }

        int div = host->max_clk / clock;
        if (div < 2)
            div = 2;
        if ((host->max_clk / div) > clock)
            div++;
        div -= 2;

        if (div > SDCDIV_MAX_CDIV)
            div = SDCDIV_MAX_CDIV;

        clock = host->max_clk / (div + 2);

        host->cdiv = div;
        write(host->cdiv, SDCDIV);

        trace("clock=%d -> max_clk=%d, cdiv=0x%x (actual clock %d)",
              input_clock, host->max_clk, host->cdiv, clock);
    }

    /* Calibrate some delays */

    host->ns_per_fifo_word = (1000000000 / clock) *
        ((host->mmc.caps & MMC_CAP_4_BIT_DATA) ? 8 : 32);

    if (input_clock == 50 * MHZ) {
        if (clock > input_clock) {
            /* Save the closest value, to make it easier
               to reduce in the event of error */
            host->overclock_50 = (clock / MHZ);

            if (clock != host->overclock) {
                info("overclocking to %dHz", clock);
                host->overclock = clock;
            }
        } else if (host->overclock) {
            host->overclock = 0;
            if (clock == 50 * MHZ)
                warn("cancelling overclock");
        }
    } else if (input_clock == 0) {
        /* Reset the preferred overclock when the clock is stopped.
         * This always happens during initialisation. */
        host->overclock_50 = host->user_overclock_50;
        host->overclock = 0;
    }

    /* Set the timeout to 500ms */
    write(clock / 2, SDTOUT);

    host->mmc.actual_clock = clock;
    host->clock = input_clock;
    host->reset_clock = 0;

    dsb();
}

static void
sdhost_request(struct bcm2835_host *host, struct mmc_host *mmc,
               struct mmc_request *mrq)
{
    /* Reset the error statuses in case this is a retry */
    if (mrq->sbc)
        mrq->sbc->error = 0;
    if (mrq->cmd)
        mrq->cmd->error = 0;
    if (mrq->data)
        mrq->data->error = 0;
    if (mrq->stop)
        mrq->stop->error = 0;

    if (mrq->data && !IS_POWER_OF_2(mrq->data->blksz)) {
        error("unsupported block size (%d bytes)", mrq->data->blksz);
        mrq->cmd->error = -EINVAL;
        mmc_request_done(mmc, mrq);
        return;
    }

    if (host->reset_clock)
        sdhost_set_clock_inner(host, host->clock);

    assert(host->mrq == 0);
    host->mrq = mrq;

    uint32_t edm = read(SDEDM);
    uint32_t fsm = edm & SDEDM_FSM_MASK;

    if ((fsm != SDEDM_FSM_IDENTMODE) && (fsm != SDEDM_FSM_DATAMODE)) {
        warn("previous command (%d) not complete (EDM 0x%x)",
             read(SDCMD) & SDCMD_CMD_MASK, edm);
        // dumpregs();
        mrq->cmd->error = -EILSEQ;
        // tasklet_schedule(&host->finish_tasklet);
        sdhost_tasklet_finish(host);
        dsb();
        return;
    }

    host->use_sbc = !!mrq->sbc &&
        (host->mrq->data->flags & USE_CMD23_FLAGS);
    if (host->use_sbc) {
        if (sdhost_send_command(host, mrq->sbc)) {
            if (!host->use_busy)
                sdhost_finish_command(host);
        }
    } else if (sdhost_send_command(host, mrq->cmd)) {
        /* PIO starts after irq */

        if (!host->use_busy)
            sdhost_finish_command(host);
    }

    dsb();
}

static void
sdhost_set_ios(struct bcm2835_host *host, struct mmc_ios *ios)
{
    debug
        ("ios clock %d, pwr %d, bus_width %d, timing %d, vdd %d, drv_type %d",
         ios->clock, ios->power_mode, ios->bus_width, ios->timing,
         ios->signal_voltage, ios->drv_type);

    /* set bus width */
    host->hcfg &= ~SDHCFG_WIDE_EXT_BUS;
    if (ios->bus_width == MMC_BUS_WIDTH_4)
        host->hcfg |= SDHCFG_WIDE_EXT_BUS;

    host->hcfg |= SDHCFG_WIDE_INT_BUS;

    /* Disable clever clock switching, to cope with fast core clocks */
    host->hcfg |= SDHCFG_SLOW_CARD;

    write(host->hcfg, SDHCFG);

    dsb();

    if (!ios->clock || ios->clock != host->clock)
        sdhost_set_clock_inner(host, ios->clock);
}

static void
sdhost_tasklet_finish(struct bcm2835_host *host)
{
    /*
     * If this tasklet gets rescheduled while running, it will
     * be run again afterwards but without any active request.
     */
    if (!host->mrq) {
        return;
    }

    //     del_timer(&host->timer);

    struct mmc_request *mrq = host->mrq;

    /* Drop the overclock after any data corruption, or after any
     * error while overclocked. Ignore errors for status commands,
     * as they are likely when a card is ejected. */
    if (host->overclock) {
        if ((mrq->cmd && mrq->cmd->error &&
             (mrq->cmd->opcode != MMC_SEND_STATUS)) ||
            (mrq->data && mrq->data->error) ||
            (mrq->stop && mrq->stop->error) ||
            (mrq->sbc && mrq->sbc->error)) {
            host->overclock_50--;
            warn("reducing overclock due to errors");
            host->reset_clock = 1;
            mrq->cmd->error = -ETIMEDOUT;
            mrq->cmd->retries = 1;
        }
    }

    host->mrq = 0;
    host->cmd = 0;
    host->data = 0;

    dsb();

    /* The SDHOST block doesn't report any errors for a disconnected
       interface. All cards and SDIO devices should report some supported
       voltage range, so a zero response to SEND_OP_COND, IO_SEND_OP_COND
       or APP_SEND_OP_COND can be treated as an error. */
    if (((mrq->cmd->opcode == MMC_SEND_OP_COND) ||
         (mrq->cmd->opcode == SD_IO_SEND_OP_COND) ||
         (mrq->cmd->opcode == SD_APP_OP_COND)) &&
        (mrq->cmd->error == 0) && (mrq->cmd->resp[0] == 0)) {
        mrq->cmd->error = -ETIMEDOUT;
        debug("faking timeout due to zero OCR");
    }

    mmc_request_done(&host->mmc, mrq);
}

static int
sdhost_add_host(struct bcm2835_host *host)
{
    struct mmc_host *mmc = &(host->mmc);
    if (!mmc->f_max || mmc->f_max > host->max_clk)
        mmc->f_max = host->max_clk;
    mmc->f_min = host->max_clk / SDCDIV_MAX_CDIV;

    mmc->max_busy_timeout = (~(unsigned int)0) / (mmc->f_max / 1000);

    trace("f_max %d, f_min %d, max_busy_timeout %d",
          mmc->f_max, mmc->f_min, mmc->max_busy_timeout);

    /* host controller capabilities */
    mmc->caps |=
        MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED |
        MMC_CAP_NEEDS_POLL | MMC_CAP_HW_RESET | MMC_CAP_ERASE |
        ((ALLOW_CMD23_READ | ALLOW_CMD23_WRITE) * MMC_CAP_CMD23);

    mmc->max_segs = 128;
    mmc->max_req_size = 524288;
    mmc->max_seg_size = mmc->max_req_size;
    mmc->max_blk_size = 512;
    mmc->max_blk_count = 65535;

    /* report supported voltage ranges */
    mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

//     tasklet_init(&host->finish_tasklet,
//         bcm2835_sdhost_tasklet_finish, (unsigned long)host);
//
//     INIT_WORK(&host->cmd_wait_wq, bcm2835_sdhost_cmd_wait_work);
//
//     timer_setup(&host->timer, bcm2835_sdhost_timeout, 0);

    sdhost_init_inner(host, 0);

    // FIXME: enable irq
    // m_pInterruptSystem->ConnectIRQ (host->irq, irq_stub, this);

    dsb();
    return 0;
}

static int
sdhost_probe(struct bcm2835_host *host)
{
    int ret = 0;
    memset(host, 0, sizeof(*host));

    // host->debug = SDHOST_DEBUG;
    // pr_debug("probe");

    host->pio_timeout = 500 * CLOCKHZ / 1000;
    host->max_delay = 1;        /* Warn if over 1ms */

    /* Read any custom properties */
    host->delay_after_stop = 0;
    host->user_overclock_50 = 0;

//     clk = devm_clk_get(dev, NULL);
//     if (IS_ERR(clk)) {
//         ret = PTR_ERR(clk);
//         if (ret == -EPROBE_DEFER)
//             dev_info(dev, "could not get clk, deferring probe\n");
//         else
//             dev_err(dev, "could not get clk\n");
//         goto err;
//     }

    host->max_clk = mbox_get_clock_rate(MBOX_CLOCK_CORE);

    // FIXME:
    // host->irq = ARM_IRQ_SDIO;

    host->mmc.caps |= MMC_CAP_4_BIT_DATA;

    uint32_t msg[3];
    msg[0] = 0U;
    msg[1] = ~0U;
    msg[2] = ~0U;

    if (mbox_set_sdhost_clock(msg) < 0) {
        error("failed to set sdhost clock");
        goto err;
    }

    host->firmware_sets_cdiv = (msg[1] != ~0U);
    if (host->firmware_sets_cdiv)
        debug("firmware sets clock divider");

    if ((ret = sdhost_add_host(host)))
        goto err;

    return 0;

  err:
    debug("err %d", ret);
    return ret;
}
