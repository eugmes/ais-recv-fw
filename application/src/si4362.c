#define DT_DRV_COMPAT silabs_si4362

#include <errno.h>
#include <device.h>
#include <init.h>
#include <drivers/spi.h>

#include "si4362.h"

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(si4362);

int si4362_shutdown(const struct device *dev, bool shutdown)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	if (!drv_data->sdn_dev) {
		return -EINVAL;
	}

	return gpio_pin_set_raw(drv_data->sdn_dev, config->sdn.pin,
		shutdown ? 0 : 1);
}

int si4362_get_cts(const struct device *dev)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	if (!drv_data->cts_dev) {
		return -EINVAL;
	}

	return gpio_pin_get(drv_data->cts_dev, config->cts.pin);
}

#define CONFIGURE_PIN(name)							\
({if (config->name.dev) {							\
	drv_data->name##_dev = device_get_binding(config->name.dev);		\
	if (!drv_data->name##_dev) {						\
		LOG_ERR("Unable to get GPI device for " #name);			\
		return -ENODEV;							\
	}									\
	int ret = gpio_pin_configure(drv_data->name##_dev, config->name.pin,	\
		config->name.flags);						\
	if (ret < 0) {								\
		return ret;							\
	}									\
	LOG_INF(#name " configured on %s:%u",					\
		config->name.dev, config->name.pin);				\
}})

static int si4362_init(const struct device *dev)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	drv_data->spi = device_get_binding(config->spi_dev_name);
	if (!drv_data->spi) {
		LOG_ERR("Unable to get SPI device");
		return -ENODEV;
	}

	if (config->cs.dev) {
		/* handle SPI CS thru GPIO if it is the case */
		drv_data->cs_ctrl.gpio_dev = device_get_binding(config->cs.dev);
		if (!drv_data->cs_ctrl.gpio_dev) {
			LOG_ERR("Unable to get GPIO SPI CS device");
			return -ENODEV;
		}
		drv_data->cs_ctrl.gpio_pin = config->cs.pin;
		drv_data->cs_ctrl.gpio_dt_flags = config->cs.flags;
		drv_data->cs_ctrl.delay = 0;

		drv_data->spi_cfg.cs = &drv_data->cs_ctrl;

		LOG_INF("CS configured on %s:%u", config->cs.dev,
			config->cs.pin);
	}

	drv_data->spi_cfg.frequency = config->freq;
	// FIXME
	drv_data->spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_MODE_CPOL |
				      SPI_MODE_CPHA | SPI_WORD_SET(8) |
				      SPI_LINES_SINGLE;
	drv_data->spi_cfg.slave = config->slave;

	CONFIGURE_PIN(sdn);
	CONFIGURE_PIN(irq);
	CONFIGURE_PIN(cts);
	CONFIGURE_PIN(rx_clock);
	CONFIGURE_PIN(rx_data);

	return 0;
}

#define PIN_CONFIG(inst, name)			\
{						\
	.dev = DT_INST_GPIO_LABEL(inst, name),	\
	.pin = DT_INST_GPIO_PIN(inst, name),	\
	.flags = DT_INST_GPIO_FLAGS(inst, name)	\
}


#define SI4362_INIT(inst)							\
	static const struct si4362_config si4362_##inst##_config = {		\
		.spi_dev_name = DT_INST_BUS_LABEL(inst),			\
		.slave = DT_INST_REG_ADDR(inst),				\
		.freq = DT_INST_PROP(inst, spi_max_frequency),			\
										\
		IF_ENABLED(DT_INST_SPI_DEV_HAS_CS_GPIOS(inst),			\
			(.cs = {						\
				DT_INST_SPI_DEV_CS_GPIOS_LABEL(inst),		\
				DT_INST_SPI_DEV_CS_GPIOS_PIN(inst),		\
				DT_INST_SPI_DEV_CS_GPIOS_FLAGS(inst),		\
			},))							\
										\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, sdn_gpios),		\
			(.sdn = PIN_CONFIG(inst, sdn_gpios),))			\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, irq_gpios),		\
			(.irq = PIN_CONFIG(inst, irq_gpios),))			\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, cts_gpios),		\
			(.cts = PIN_CONFIG(inst, cts_gpios),))			\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, rx_clock_gpios),		\
			(.rx_clock = PIN_CONFIG(inst, rx_clock_gpios),))	\
		IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, rx_data_gpios),		\
			(.rx_data = PIN_CONFIG(inst, rx_data_gpios),))		\
	};									\
										\
	static struct si4362_drv_data si4362_##inst##_drvdata = {		\
	};									\
										\
	DEVICE_INIT(si4362_##inst, DT_INST_LABEL(inst),				\
		    si4362_init, &si4362_##inst##_drvdata,			\
		    &si4362_##inst##_config,					\
		    POST_KERNEL,						\
		    CONFIG_SI4362_INIT_PRIORITY);

DT_INST_FOREACH_STATUS_OKAY(SI4362_INIT)
