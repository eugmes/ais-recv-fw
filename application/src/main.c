/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <kernel.h>
#include <usb/usb_device.h>

#include "hdlc.h"
#include "si4362.h"
#include "radio_configs.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

#define AIS_NUM_CHANNELS 2
#define AIS_QUEUE_LENGTH 16

K_MSGQ_DEFINE(ais_msgq, 1, AIS_QUEUE_LENGTH, 1);

#define DEF_AIS_CALLBACK(name, index)						\
static void name(const struct device *dev, int raw_bit)				\
{										\
	__ASSERT_NO_MSG(raw_bit >= 0);						\
	uint8_t msg = (index << 1) | (raw_bit & 1);				\
	int ret = k_msgq_put(&ais_msgq, &msg, K_NO_WAIT);			\
	if (ret != 0) {								\
		LOG_ERR("Failed to put message: %d", ret);			\
	}									\
}

DEF_AIS_CALLBACK(ch1_callback, 0)
DEF_AIS_CALLBACK(ch2_callback, 1)

struct ais_config {
	const char *dev_name;
	const uint8_t *config_data;
	si4362_rx_callback callback;
};

struct ais_data {
	const struct device *dev;
	struct hdlc_data hdlc;
};

static void hdlc_callback(const uint8_t *buf, size_t len)
{
	LOG_INF("hdlc callback");
}

static const struct ais_config ais_configs[AIS_NUM_CHANNELS] = {
	{
		.dev_name = "RADIO_0",
		.config_data = radio_config_ch1,
		.callback = ch1_callback,
	},
	{
		.dev_name = "RADIO_1",
		.config_data = radio_config_ch2,
		.callback = ch2_callback,
	},
};

static struct ais_data ais_datas[AIS_NUM_CHANNELS];

static void init_radios(void)
{
	/* Reset devices first because of shared SDN. */
	for (int i = 0; i < ARRAY_SIZE(ais_configs); i++) {
#ifndef CONFIG_APP_SIMULATE
		const char *dev_name = ais_configs[i].dev_name;
		LOG_INF("Setting up %s", dev_name);
		const struct device *dev = device_get_binding(dev_name);
		__ASSERT_NO_MSG(dev != NULL);
		si4362_reset(dev);
		ais_datas[i].dev = dev;
#endif
	}

	for (int i = 0; i < ARRAY_SIZE(ais_configs); i++) {
		hdlc_init(&ais_datas[i].hdlc, hdlc_callback);
#ifndef CONFIG_APP_SIMULATE
		const struct ais_config *cfg = &ais_configs[i];
		const struct device *dev = ais_datas[i].dev;
		si4362_send_init_commands(dev, cfg->config_data);
		si4362_set_callback(dev, cfg->callback);
		si4362_configure_interrupt(dev, true);
#endif
	}
}

#ifdef CONFIG_APP_SIMULATE

#define SYM_STACK_SIZE 500
#define SYM_PRIORITY 5

K_THREAD_STACK_DEFINE(sym_stack_area, SYM_STACK_SIZE);
static struct k_thread sym_thread_data;

static const uint8_t bitstream[] = {
#include <bitstream.inc>
};

static void sym_thread(void *arg1, void *arg2, void *arg3)
{
	k_sleep(K_SECONDS(10));

	LOG_INF("simulation started");

	for (size_t i = 0; i < ARRAY_SIZE(bitstream); i++) {
		uint8_t byte = bitstream[i];

		for (int bit = 0; bit < 8; bit++) {
			k_usleep(1000000U / 9600U);
			ch1_callback(NULL, (byte & 1) != 0);
			byte >>= 1;
		}
	}

	LOG_INF("simulation ended");
}

static void start_simulation_thread(void)
{
	k_thread_create(&sym_thread_data, sym_stack_area,
		K_THREAD_STACK_SIZEOF(sym_stack_area),
		sym_thread,
		NULL, NULL, NULL,
		SYM_PRIORITY, 0, K_NO_WAIT);
}

#endif

void main(void)
{
	if (usb_enable(NULL)) {
		LOG_ERR("USB initialization failed.");
		return;
	}

	init_radios();

#ifdef CONFIG_APP_SIMULATE
	start_simulation_thread();
#endif

	for (;;) {
		uint8_t msg;
		k_msgq_get(&ais_msgq, &msg, K_FOREVER);
		int bit = msg & 1;
		int idx = msg >> 1;

		hdlc_input(&ais_datas[idx].hdlc, bit);
	}
}
