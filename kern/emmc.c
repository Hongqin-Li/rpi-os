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

#include "emmc.h"
#include "string.h"
#include "console.h"
//
// Configuration options
//

#define EMMC_DEBUG
//#define EMMC_DEBUG2

//
// According to the BCM2835 ARM Peripherals Guide the EMMC STATUS register
// should not be used for polling. The original driver does not meet this
// specification in this point but the modified driver should do so.
// Define EMMC_POLL_STATUS_REG if you want the original function!
//
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

// Enable SDXC maximum performance mode
// Requires 150 mA power so disabled on the RPi for now
#define SDXC_MAXIMUM_PERFORMANCE

#define SD_CMD_INDEX(a)            ((a) << 24)
#define SD_CMD_TYPE_NORMAL        0
#define SD_CMD_TYPE_SUSPEND        (1 << 22)
#define SD_CMD_TYPE_RESUME        (2 << 22)
#define SD_CMD_TYPE_ABORT        (3 << 22)
#define SD_CMD_TYPE_MASK        (3 << 22)
#define SD_CMD_ISDATA            (1 << 21)
#define SD_CMD_IXCHK_EN            (1 << 20)
#define SD_CMD_CRCCHK_EN        (1 << 19)
#define SD_CMD_RSPNS_TYPE_NONE        0 // For no response
#define SD_CMD_RSPNS_TYPE_136        (1 << 16)  // For response R2 (with CRC), R3,4 (no CRC)
#define SD_CMD_RSPNS_TYPE_48        (2 << 16)   // For responses R1, R5, R6, R7 (with CRC)
#define SD_CMD_RSPNS_TYPE_48B        (3 << 16)  // For responses R1b, R5b (with CRC)
#define SD_CMD_RSPNS_TYPE_MASK      (3 << 16)
#define SD_CMD_MULTI_BLOCK        (1 << 5)
#define SD_CMD_DAT_DIR_HC        0
#define SD_CMD_DAT_DIR_CH        (1 << 4)
#define SD_CMD_AUTO_CMD_EN_NONE        0
#define SD_CMD_AUTO_CMD_EN_CMD12    (1 << 2)
#define SD_CMD_AUTO_CMD_EN_CMD23    (2 << 2)
#define SD_CMD_BLKCNT_EN        (1 << 1)
#define SD_CMD_DMA                  1

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

#define TIMEOUT(self)       (FAIL(self) && self->last_error == ETIMEDOUT)

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

#define SD_BLOCK_SIZE        512


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

int
emmc_init(struct emmc *self)
{
    if (!sdhost_initialize(&self->host)) {
        return 0;
    }
    if (emmc_card_init(self) != 0) {
        return 0;
    }
    return 1;
}

int
emmc_read(struct emmc *self, void *buf, size_t cnt)
{

}

int
emmc_write(struct emmc *self, void *buf, size_t cnt)
{

}

uint64_t
emmc_seek(struct emmc *self, uint64_t off)
{
    self->ull_offset = off;
    return off;
}

static int
emmc_card_init(struct emmc *self)
{
    // Check the sanity of the sd_commands and sd_acommands structures
    assert(sizeof(sd_commands) == (64 * sizeof(uint32_t)));
    assert(sizeof(sd_acommands) == (64 * sizeof(uint32_t)));

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
emmc_card_reset(struct emmc *self)
{
    sdhost_set_clock(&self->host, SD_CLOCK_ID);

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
    // << Prepare the device structure


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
    } else if (FAIL(self)) {
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
        error("found SDIO card, unimplemented");
        return -1;
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
    sdhost_set_clock(&self->host, SD_CLOCK_NORMAL);

    // A small wait before the voltage switch
    delayus(5000);

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


    // Get the cards SCR register
    self->buf = &self->scr->scr[0];
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
    uint32_t scr0 = be2le32(self->scr->scr[0]);
    self->scr->sd_version = SD_VER_UNKNOWN;
    uint32_t sd_spec = (scr0 >> (56 - 32)) & 0xf;
    uint32_t sd_spec3 = (scr0 >> (47 - 32)) & 0x1;
    uint32_t sd_spec4 = (scr0 >> (42 - 32)) & 0x1;
    self->scr->sd_bus_widths = (scr0 >> (48 - 32)) & 0xf;
    if (sd_spec == 0) {
        self->scr->sd_version = SD_VER_1;
    } else if (sd_spec == 1) {
        self->scr->sd_version = SD_VER_1_1;
    } else if (sd_spec == 2) {
        if (sd_spec3 == 0) {
            self->scr->sd_version = SD_VER_2;
        } else if (sd_spec3 == 1) {
            if (sd_spec4 == 0) {
                self->scr->sd_version = SD_VER_3;
            } else if (sd_spec4 == 1) {
                self->scr->sd_version = SD_VER_4;
            }
        }
    }
    debug("SCR: version %s, bus_widths 0x%x",
          sd_versions[self->scr->sd_version], self->scr->sd_bus_widths);


#ifdef SD_HIGH_SPEED
    // If card supports CMD6, read switch information from card
    if (self->scr->sd_version >= SD_VER_1_1) {
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
                    sdhost_set_clock(&self->host, SD_CLOCK_HIGH);
                    trace("switch to 50MHz clock complete");
                }
            }
        }

        // Restore block size
        self->block_size = SD_BLOCK_SIZE;
    }
#endif



    if (self->scr->sd_bus_widths & 4) {
        // Set 4-bit transfer mode (ACMD6)
        // See HCSS 3.4 for the algorithm
#ifdef SD_4BIT_DATA
        // Send ACMD6 to change the card's bit mode
        if (!emmc_issue_command(self, SET_BUS_WIDTH, 2, 500000)) {
            error("failed to switch to 4-bit data mode");
        } else {
            // Change bit mode for Host
            sdhost_set_bus_width(&self->host, 4);
            trace("switch to 4-bit complete");
        }
#endif
    }

    info("found valid version %s SD card",
         sd_versions[self->scr->sd_version]);

    return 0;
}

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

static int
emmc_issue_command(struct emmc *self, uint32_t cmd, uint32_t arg,
                   int timeout)
{
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
