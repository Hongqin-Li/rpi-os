#ifndef INC_SDHOST_H
#define INC_SDHOST_H

#include "types.h"

#if RASPI == 3
// FIXME: Use sdhost and reserve sdhci for wifi.
// #define USE_SDHOST
#endif

struct sg_mapping_iter
{
#define SG_MITER_ATOMIC         (1 << 0)
#define SG_MITER_TO_SG          (1 << 1)
#define SG_MITER_FROM_SG        (1 << 2)
    void    *addr;
    size_t  length;
    size_t  consumed;
};

struct mmc_ios
{
    uint32_t    clock;
    uint8_t     bus_width;
#define MMC_BUS_WIDTH_4         (1 << 0)
    uint8_t     power_mode;
    uint8_t     timing;
    uint8_t     signal_voltage;
    uint8_t     drv_type;
};

struct mmc_host
{
    uint32_t    caps;
#define MMC_CAP_4_BIT_DATA      (1 << 0)
#define MMC_CAP_SD_HIGHSPEED    (1 << 1)
#define MMC_CAP_MMC_HIGHSPEED   (1 << 2)
#define MMC_CAP_NEEDS_POLL      (1 << 3)
#define MMC_CAP_HW_RESET        (1 << 4)
#define MMC_CAP_ERASE           (1 << 5)
#define MMC_CAP_CMD23           (1 << 6)
    uint32_t    f_min;
    uint32_t    f_max;
    uint32_t    actual_clock;
    uint32_t    max_busy_timeout;
    uint16_t    max_segs;
    uint32_t    max_seg_size;
    uint32_t    max_req_size;
    uint32_t    max_blk_size;
    uint32_t    max_blk_count;
    uint32_t    ocr_avail;
#define MMC_VDD_32_33           (1 << 20)
#define MMC_VDD_33_34           (1 << 21)
    struct mmc_ios  ios;
};

struct mmc_command;
struct mmc_request;

struct mmc_data
{
    uint32_t    flags;
#define MMC_DATA_READ           (1 << 0)
#define MMC_DATA_WRITE          (1 << 1)
    uint32_t    blksz;
    uint32_t    blocks;
    void        *sg;
    uint32_t    sg_len;
    uint32_t    bytes_xfered;
    int         error;
    struct mmc_command  *stop;
    struct mmc_request  *mrq;
};

struct mmc_command
{
    uint32_t        opcode;
#define MMC_SEND_OP_COND            1
#define SD_IO_SEND_OP_COND          5
#define MMC_STOP_TRANSMISSION       12
#define MMC_SEND_STATUS             13
#define MMC_READ_MULTIPLE_BLOCK     18
#define MMC_WRITE_MULTIPLE_BLOCK    25
#define SD_APP_OP_COND              41
    uint32_t        arg;
    uint32_t        flags;
#define MMC_RSP_PRESENT         (1 << 0)
#define MMC_RSP_136             (1 << 1)
#define MMC_RSP_CRC             (1 << 2)
#define MMC_RSP_BUSY            (1 << 3)
    uint32_t        resp[4];
    uint32_t        retries;
    int             error;
    struct mmc_data *data;
};

struct mmc_request
{
    struct mmc_command  *sbc;
    struct mmc_command  *cmd;
    struct mmc_data     *data;
    struct mmc_command  *stop;
    volatile int        done;
};

/* MMC errors. */
#define EINVAL      1
#define ETIMEDOUT   2
#define EILSEQ      3
#define ENOTSUP     4


struct bcm2835_host
{
    void (*sleep_fn)(void *);       /* Callback function when waiting for interrupt. */
    void *sleep_arg;                /* Argument passed with sleep_fn. */
    struct mmc_host mmc;
    uint32_t        pio_timeout;    /* In CLOCKHZ ticks */
    uint32_t        clock;          /* Current clock speed */
    uint32_t        max_clk;        /* Max possible freq */

//    tasklet_struct        finish_tasklet;    /* Tasklet structures */
//
//    work_struct        cmd_wait_wq;    /* Workqueue function */
//
//    timer_list        timer;        /* Timer for timeouts */

    struct sg_mapping_iter  sg_miter;   /* SG state for PIO */
    uint32_t                blocks;     /* remaining PIO blocks */

    int irq;        /* Device IRQ */

    uint32_t cmd_quick_poll_retries;
    uint32_t ns_per_fifo_word;

    /* cached registers */
    uint32_t hcfg;
    uint32_t cdiv;

    struct mmc_request  *mrq;   /* Current request */
    struct mmc_command  *cmd;   /* Current command */
    struct mmc_data     *data;  /* Current data request */
    unsigned data_complete:1;   /* Data finished before cmd */
    unsigned use_busy:1;        /* Wait for busy interrupt */
    unsigned use_sbc:1;         /* Send CMD23 */
    unsigned debug:1;        /* Enable debug output */
    unsigned firmware_sets_cdiv:1;    /* Let the firmware manage the clock */
    unsigned reset_clock:1;        /* Reset the clock fore the next request */

    int      max_delay;         /* maximum length of time spent waiting */
    uint64_t stop_time;         /* when the last stop was issued */
    uint64_t delay_after_stop;      /* minimum time between stop and subsequent data transfer */
    uint64_t delay_after_this_stop; /* minimum time between this stop and subsequent data transfer */
    uint32_t user_overclock_50; /* User's preferred frequency to use when 50MHz is requested (in MHz) */
    uint32_t overclock_50;      /* frequency to use when 50MHz is requested (in MHz) */
    uint32_t overclock;         /* Current frequency if overclocked, else zero */

//     uint32_t            sectors;    /* Cached card size in sectors */
};

int sdhost_init(struct bcm2835_host *host, void (*sleep_fn)(void *), void *sleep_arg);
int sdhost_command(struct bcm2835_host *host, struct mmc_command *cmd, uint32_t nretries);
void sdhost_set_clock(struct bcm2835_host *host, uint32_t clockhz);
void sdhost_set_bus_width(struct bcm2835_host *host, uint32_t nbits);
void sdhost_intr(struct bcm2835_host *host);

#endif
