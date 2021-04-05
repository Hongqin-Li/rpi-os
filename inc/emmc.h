#include "sdhost.h"

struct tscr // SD configuration register
{
	uint32_t	scr[2];
	uint32_t	sd_bus_widths;
	int	sd_version;
};

struct emmc {
    uint64_t ull_offset;
    struct bcm2835_host host;

	// was: struct emmc_block_dev
	uint32_t device_id[4];

	uint32_t card_supports_sdhc;
	uint32_t card_supports_hs;
	uint32_t card_supports_18v;
	uint32_t card_ocr;
	uint32_t card_rca;
	uint32_t last_error;

    struct tscr *scr;

	int failed_voltage_switch;

	uint32_t last_cmd_reg;
	uint32_t last_cmd;
	uint32_t last_cmd_success;
	uint32_t last_r0;
	uint32_t last_r1;
	uint32_t last_r2;
	uint32_t last_r3;

	void *buf;
	int blocks_to_transfer;
	size_t block_size;

	// static const char *sd_versions[];
	// static const uint32_t sd_commands[];
	// static const uint32_t sd_acommands[];
};

int emmc_init(struct emmc *self);
size_t emmc_read(struct emmc *self, void *buf, size_t cnt);
size_t emmc_write(struct emmc *self, void *buf, size_t cnt);
uint64_t emmc_seek(struct emmc *self, uint64_t off);

