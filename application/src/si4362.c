/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT silabs_si4362

#include <errno.h>
#include <device.h>
#include <init.h>
#include <drivers/spi.h>

#include "si4362.h"

#define LOG_LEVEL CONFIG_SI4362_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(si4362);

#define SI4362_RESET_DELAY K_MSEC(10)
#define SI4362_CTS_TIMEOUT 10000

static void rx_clock_callback_handler(const struct device *port,
	struct gpio_callback *cb, gpio_port_pins_t pins)
{
	struct si4362_drv_data *drv_data =
		CONTAINER_OF(cb, struct si4362_drv_data, rx_clock_cb);

	if (drv_data->rx_callback == NULL) {
		return;
	}

	const struct device *dev = drv_data->dev;
	const struct si4362_config *config = dev->config;

	if (pins & BIT(config->rx_clock.pin)) {
		int data = gpio_pin_get(drv_data->rx_data_dev,
			config->rx_data.pin);
		drv_data->rx_callback(dev, data);
	}
}

int si4362_reset(const struct device *dev)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	if (!drv_data->sdn_dev) {
		return -ENOTSUP;
	}

	k_sleep(SI4362_RESET_DELAY);
	gpio_pin_set(drv_data->sdn_dev, config->sdn.pin, 1);
	k_sleep(SI4362_RESET_DELAY);
	gpio_pin_set(drv_data->sdn_dev, config->sdn.pin, 0);
	k_sleep(SI4362_RESET_DELAY);

	return 0;
}

int si4362_get_cts(const struct device *dev)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	if (!drv_data->cts_dev) {
		return -ENOTSUP;
	}

	return gpio_pin_get(drv_data->cts_dev, config->cts.pin);
}

int si4362_configure_interrupt(const struct device *dev, bool enable)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	if (!drv_data->rx_clock_dev) {
		return -ENOTSUP;
	}

	gpio_flags_t flags = enable ?
		(GPIO_INT_ENABLE | GPIO_INT_EDGE_RISING) :
		GPIO_INT_DISABLE;

	return gpio_pin_interrupt_configure(drv_data->rx_clock_dev,
		config->rx_clock.pin, flags);
}

void si4362_set_callback(const struct device *dev, si4362_rx_callback callback)
{
	struct si4362_drv_data *drv_data = dev->data;
	drv_data->rx_callback = callback;
}

static int send_command(const struct device *dev, size_t len, const void *data)
{
	struct si4362_drv_data *drv_data = dev->data;
	int ret;

	const struct spi_buf tx_buf[] = {
		{
			.len = len,
			.buf = (void *)data,
		},
	};

	const struct spi_buf_set tx = {
		.buffers = tx_buf,
		.count = ARRAY_SIZE(tx_buf),
	};

	ret = spi_write(drv_data->spi, &drv_data->spi_cfg, &tx);
	spi_release(drv_data->spi, &drv_data->spi_cfg);
	return ret;
}

static int get_response(const struct device *dev, size_t len, void *data)
{
	struct si4362_drv_data *drv_data = dev->data;
	int ret;

	uint8_t cmd = 0x44;
	uint8_t cts = 0;

	const struct spi_buf tx_buf[] = {
		{
			.len = 1,
			.buf = &cmd,
		},
		{
			.len = 1,
			.buf = NULL,
		},
	};

	const struct spi_buf rx_buf[] = {
		{
			.len = 1,
			.buf = NULL,
		},
		{
			.len = 1,
			.buf = &cts,
		},
	};

	const struct spi_buf_set tx = {
		.buffers = tx_buf,
		.count = ARRAY_SIZE(tx_buf),
	};

	const struct spi_buf_set rx = {
		.buffers = rx_buf,
		.count = ARRAY_SIZE(rx_buf),
	};

	ret = spi_transceive(drv_data->spi, &drv_data->spi_cfg, &tx, &rx);

	if (ret < 0) {
		goto ret_unlock;
	}

	if (cts != 0xff) {
		ret = -EAGAIN;
		goto ret_unlock;
	}

	/* read the response. */
	ret = 0;
	if (len > 0) {
		const struct spi_buf resp_buf[] = {
			{
				.len = len,
				.buf = data,
			},
		};

		const struct spi_buf_set resp_rx = {
			.buffers = resp_buf,
			.count = ARRAY_SIZE(resp_buf),
		};

		ret = spi_read(drv_data->spi, &drv_data->spi_cfg, &resp_rx);
	}

ret_unlock:
	spi_release(drv_data->spi, &drv_data->spi_cfg);
	return ret;
}

static int transceive(const struct device *dev,
		      size_t tx_len, const void *data_tx,
		      size_t rx_len, void *data_rx)
{
	int ret;
	ret = send_command(dev, tx_len, data_tx);
	if (ret < 0) {
		return ret;
	}

	for (int retries = 0; retries < SI4362_CTS_TIMEOUT; retries++) {
		ret = get_response(dev, rx_len, data_rx);
		if (ret != -EAGAIN) {
			return ret;
		}
	}

	LOG_ERR("CTS timeout exceeded");
	return -EIO;
}

int si4362_send_init_commands(const struct device *dev, const uint8_t *cmds)
{
	const uint8_t *p = cmds;
	int ret = 0;

	for (;;) {
		size_t cmd_len = *p++;
		if (cmd_len == 0) {
			break;
		}

		LOG_DBG("command: 0x%02x (len = %d)", (int)*p, (int)cmd_len);

		ret = transceive(dev, cmd_len, p, 0, NULL);
		if (ret < 0) {
			return ret;
		}

		p += cmd_len;
	}

	return 0;
}

#define CONFIGURE_PIN(name, extra_flags)					\
({if (config->name.dev) {							\
	drv_data->name##_dev = device_get_binding(config->name.dev);		\
	if (!drv_data->name##_dev) {						\
		LOG_ERR("Unable to get GPI device for " #name);			\
		return -ENODEV;							\
	}									\
	int ret = gpio_pin_configure(drv_data->name##_dev, config->name.pin,	\
		config->name.flags | (extra_flags));				\
	if (ret < 0) {								\
		return ret;							\
	}									\
	LOG_DBG(#name " configured on %s:%u",					\
		config->name.dev, config->name.pin);				\
}})

static int si4362_init(const struct device *dev)
{
	const struct si4362_config *config = dev->config;
	struct si4362_drv_data *drv_data = dev->data;

	drv_data->dev = dev;
	drv_data->rx_callback = NULL;

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
		drv_data->cs_ctrl.delay = 1;

		drv_data->spi_cfg.cs = &drv_data->cs_ctrl;

		gpio_pin_configure(drv_data->cs_ctrl.gpio_dev,
			config->cs.pin,
			config->cs.flags | GPIO_OUTPUT | GPIO_OUTPUT_INIT_HIGH);

		LOG_DBG("CS configured on %s:%u", config->cs.dev,
			config->cs.flags);
	}

	drv_data->spi_cfg.frequency = config->freq;
	// FIXME
	drv_data->spi_cfg.operation = SPI_OP_MODE_MASTER |
				      SPI_TRANSFER_MSB |
				      SPI_WORD_SET(8) |
				      SPI_LINES_SINGLE |
				      SPI_HOLD_ON_CS |
				      SPI_LOCK_ON;
	drv_data->spi_cfg.slave = config->slave;


	CONFIGURE_PIN(sdn, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
	CONFIGURE_PIN(irq, GPIO_INPUT);
	CONFIGURE_PIN(cts, GPIO_INPUT);
	CONFIGURE_PIN(rx_clock, GPIO_INPUT);
	CONFIGURE_PIN(rx_data, GPIO_INPUT);

	if (drv_data->rx_clock_dev) {
		gpio_init_callback(&drv_data->rx_clock_cb,
			&rx_clock_callback_handler,
			BIT(config->rx_clock.pin));
		gpio_add_callback(drv_data->rx_clock_dev,
			&drv_data->rx_clock_cb);
	}

	si4362_reset(dev);

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
