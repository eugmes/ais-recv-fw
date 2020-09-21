/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APPLICATION_SRC_SI4362_H_
#define APPLICATION_SRC_SI4362_H_

#include <kernel.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*si4362_rx_callback)(const struct device *dev, int bit);

/* FIXME why is this not a standard type??? */
struct si4362_gpio_pin_config {
	const char *dev;
	gpio_pin_t pin;
	gpio_dt_flags_t flags;
};

struct si4362_config {
	const char *spi_dev_name;
	uint16_t slave;
	uint32_t freq;
	struct si4362_gpio_pin_config cs;
	struct si4362_gpio_pin_config sdn;
	struct si4362_gpio_pin_config irq;
	struct si4362_gpio_pin_config cts;
	struct si4362_gpio_pin_config rx_clock;
	struct si4362_gpio_pin_config rx_data;
};

struct si4362_drv_data {
	/** Master SPI device */
	const struct device *spi;
	struct spi_config spi_cfg;
	struct spi_cs_control cs_ctrl;

	/* Backling to ease handling of GPIO interrupts. */
	const struct device *dev;

	const struct device *sdn_dev;
	const struct device *irq_dev;
	const struct device *cts_dev;
	const struct device *rx_clock_dev;
	const struct device *rx_data_dev;

	struct gpio_callback rx_clock_cb;
	si4362_rx_callback rx_callback;
};

#define SI4362_CMD_NOP       0x00
#define SI4362_CMD_PART_INFO 0x01
#define SI4362_CMD_POWER_UP  0x02
#define SI4362_CMD_FUNC_INFO 0x10

struct si4362_part_info {
	uint8_t chip_rev;
	uint16_t part;
	uint8_t pbuild;
	uint16_t id;
	uint8_t customer;
	uint8_t rom_id;
};

struct si4362_func_info {
	uint8_t rev_ext;
	uint8_t rev_branch;
	uint8_t rev_int;
	uint16_t patch;
	uint8_t func;
};

int si4362_reset(const struct device *dev);
int si4362_get_cts(const struct device *dev);
int si4362_configure_interrupt(const struct device *dev, bool enable);
void si4362_set_callback(const struct device *dev, si4362_rx_callback callback);

int si4362_send_init_commands(const struct device *dev, const uint8_t *cmds);

// int si4362_part_info(const struct device *dev, struct si4362_part_info *info);
// int si4362_func_info(const struct device *dev, struct si4362_func_info *info);

#ifdef __cplusplus
}
#endif

#endif
