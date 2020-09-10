#define DT_DRV_COMPAT silabs_si4362

#include <errno.h>
#include <device.h>
#include <init.h>
#include <drivers/spi.h>

#include "si4362.h"

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(si4362);

static int si4362_init(struct device *dev)
{
	const struct si4362_config *const config = dev->config;
	struct si4362_drv_data *const drv_data =
		(struct si4362_drv_data *)dev->data;

	drv_data->spi = device_get_binding((char *)config->spi_dev_name);
	if (!drv_data->spi) {
		LOG_ERR("Unable to get SPI device");
		return -ENODEV;
	}

	if (config->cs_dev) {
		/* handle SPI CS thru GPIO if it is the case */
		drv_data->cs_ctrl.gpio_dev = device_get_binding(config->cs_dev);
		if (!drv_data->cs_ctrl.gpio_dev) {
			LOG_ERR("Unable to get GPIO SPI CS device");
			return -ENODEV;
		}
		drv_data->cs_ctrl.gpio_pin = config->cs_pin;
		drv_data->cs_ctrl.gpio_dt_flags = config->cs_flags;
		drv_data->cs_ctrl.delay = 0;

		drv_data->spi_cfg.cs = &drv_data->cs_ctrl;

		LOG_INF("CS configured on %s:%u",
			config->cs_dev, config->cs_pin);
	}

	drv_data->spi_cfg.frequency = config->freq;
	// FIXME
	drv_data->spi_cfg.operation = (SPI_OP_MODE_MASTER | SPI_MODE_CPOL |
				       SPI_MODE_CPHA | SPI_WORD_SET(8) |
				       SPI_LINES_SINGLE);
	drv_data->spi_cfg.slave = config->slave;

	return 0;
}

#define SI4362_INIT(inst)						\
	static struct si4362_config si4362_##inst##_config = {		\
		.spi_dev_name = DT_INST_BUS_LABEL(inst),		\
		.slave = DT_INST_REG_ADDR(inst),			\
		.freq = DT_INST_PROP(inst, spi_max_frequency),		\
									\
		IF_ENABLED(DT_INST_SPI_DEV_HAS_CS_GPIOS(inst),		\
			(.cs_dev =					\
			 DT_INST_SPI_DEV_CS_GPIOS_LABEL(inst),))	\
		IF_ENABLED(DT_INST_SPI_DEV_HAS_CS_GPIOS(inst),		\
			(.cs_pin =					\
			 DT_INST_SPI_DEV_CS_GPIOS_PIN(inst),))		\
		IF_ENABLED(DT_INST_SPI_DEV_HAS_CS_GPIOS(inst),		\
			(.cs_flags =					\
			 DT_INST_SPI_DEV_CS_GPIOS_FLAGS(inst),))	\
	};								\
									\
	static struct si4362_drv_data si4362_##inst##_drvdata = {	\
	};								\
									\
	DEVICE_AND_API_INIT(si4362_##inst, DT_INST_LABEL(inst),		\
			    si4362_init, &si4362_##inst##_drvdata,	\
			    &si4362_##inst##_config,			\
			    POST_KERNEL,				\
			    CONFIG_SI4362_INIT_PRIORITY,		\
			    (const void *)1);

DT_INST_FOREACH_STATUS_OKAY(SI4362_INIT)
