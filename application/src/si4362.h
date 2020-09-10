#ifndef APPLICATION_SRC_SI4362_H_
#define APPLICATION_SRC_SI4362_H_

#include <kernel.h>

#include <drivers/spi.h>

#ifdef __cplusplus
extern "C" {
#endif

struct si4362_config {
	const char *const spi_dev_name;
	const uint16_t slave;
	const uint32_t freq;
	const char *const cs_dev;
	const uint32_t cs_pin;
	const uint8_t cs_flags;
};

struct si4362_drv_data {
	/** Master SPI device */
	struct device *spi;
	struct spi_config spi_cfg;
	struct spi_cs_control cs_ctrl;
};

#ifdef __cplusplus
}
#endif

#endif
