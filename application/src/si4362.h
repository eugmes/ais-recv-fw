#ifndef APPLICATION_SRC_SI4362_H_
#define APPLICATION_SRC_SI4362_H_

#include <kernel.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

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

	const struct device *sdn_dev;
	const struct device *irq_dev;
	const struct device *cts_dev;
	const struct device *rx_clock_dev;
	const struct device *rx_data_dev;
};

int si4362_shutdown(const struct device *dev, bool shutdown);

int si4362_get_cts(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif
