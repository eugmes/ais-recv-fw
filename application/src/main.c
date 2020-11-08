/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <kernel.h>
#include <usb/usb_device.h>
#include <stdio.h>
#include <console/tty.h>

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

struct ais_state {
	const struct device *dev;
	struct hdlc_data hdlc;
	uint8_t channel_index;
};

#define NMEA_MAX_LENGTH 82
#define AIVDM_OVERHEAD (strlen("!AIVDM,0,0,0,A,,0,*00\r\n"))
// Note: this is the limit for multipart messages, it is one greater for single
// part.
#define MAX_SENTENCE_CHARS (NMEA_MAX_LENGTH - AIVDM_OVERHEAD)

static char nmea_buffer[NMEA_MAX_LENGTH + 1];
static uint8_t multipart_counter;

static struct tty_serial tty;

static char get_ascii6(const uint8_t *buf, uint16_t bit_offset)
{
	uint16_t byte_idx = bit_offset / 8;
	uint16_t bit_idx = bit_offset % 8;
	uint8_t n;

	if (bit_idx <= 2) {
		n = (buf[byte_idx] >> (2 - bit_idx)) & 0x3f;
	} else {
		n = ((buf[byte_idx] << (bit_idx - 2)) & 0x3f)
		  | (buf[byte_idx + 1] >> (10 - bit_idx));
	}

	__ASSERT_NO_MSG(n < 64);

	return (n >= 40) ? n + 56 : n + 48;
}

static void hdlc_callback(const struct hdlc_data *hdlc, const uint8_t *buf, size_t len)
{
	const struct ais_state *ais = CONTAINER_OF(hdlc, struct ais_state, hdlc);
	uint16_t num_chars = ceiling_fraction(len * 8, 6);

	bool multipart = num_chars > MAX_SENTENCE_CHARS + 1;
	uint8_t num_parts = multipart ?
		ceiling_fraction(num_chars, MAX_SENTENCE_CHARS) : 1;
	LOG_DBG("len: %zu, chars: %u, parts: %u", len, num_chars, num_parts);

	char channel = 'A' + ais->channel_index;
	uint16_t bit_offset = 0;
	uint16_t remaining_chars = num_chars;

	for (uint8_t part = 1; part <= num_parts; part++) {
		char *p = nmea_buffer;

		/* Put header. */
		if (multipart) {
			p += sprintf(p, "!AIVDM,%u,%u,%u,%c,", num_parts, part,
				     multipart_counter, channel);
		} else {
			p += sprintf(p, "!AIVDM,1,1,,%c,", channel);
		}

		/* Put the data. */
		uint16_t part_chars = remaining_chars;
		if (multipart && remaining_chars > MAX_SENTENCE_CHARS) {
			part_chars = MAX_SENTENCE_CHARS;
		}

		LOG_DBG("part: %u, chars: %u", part, part_chars);

		for (int i = 0; i < part_chars; i++) {
			*p++ = get_ascii6(buf, bit_offset);
			bit_offset += 6;
		}

		remaining_chars -= part_chars;

		/* Add pad info */
		uint8_t pad = 0;

		if (part == num_parts) {
			__ASSERT_NO_MSG(num_chars * 6 >= len * 8);
			pad = num_chars * 6 - len * 8;
		}

		p += sprintf(p, ",%u", pad);

		/* Calculate the checksum. */
		uint8_t sum = 0;

		for (const char *q = nmea_buffer + 1; q < p; q++) {
			sum ^= *q;
		}

		/* Add the checksum and the line end. */
		p += sprintf(p, "*%02X\r\n", sum);

		__ASSERT(PART_OF_ARRAY(nmea_buffer, p), "nmea_buffer overlow");

		tty_write(&tty, nmea_buffer, p - nmea_buffer);
	}

	if (multipart) {
		multipart_counter++;
		if (multipart_counter > 9) {
			multipart_counter = 0;
		}
	}
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

static struct ais_state ais_states[AIS_NUM_CHANNELS];

static void init_radios(void)
{
	/* Reset devices first because of shared SDN. */
	for (int i = 0; i < ARRAY_SIZE(ais_configs); i++) {
#ifndef CONFIG_APP_SIMULATE
		const char *dev_name = ais_configs[i].dev_name;
		const struct device *dev = device_get_binding(dev_name);
		__ASSERT_NO_MSG(dev != NULL);
		si4362_reset(dev);
		ais_states[i].channel_index = i;
		ais_states[i].dev = dev;
#endif
	}

	for (int i = 0; i < ARRAY_SIZE(ais_configs); i++) {
		hdlc_init(&ais_states[i].hdlc, hdlc_callback);
#ifndef CONFIG_APP_SIMULATE
		const struct ais_config *cfg = &ais_configs[i];
		const struct device *dev = ais_states[i].dev;
		si4362_send_init_commands(dev, radio_patch);
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
		k_oops();
		return;
	}

	const struct device *dev = device_get_binding("CDC_ACM_0");
	if (!dev) {
		k_oops();
		return;
	}

	tty_init(&tty, dev);

	init_radios();

#ifdef CONFIG_APP_SIMULATE
	start_simulation_thread();
#endif

	for (;;) {
		uint8_t msg;
		k_msgq_get(&ais_msgq, &msg, K_FOREVER);
		int bit = msg & 1;
		int idx = msg >> 1;

		hdlc_input(&ais_states[idx].hdlc, bit);
	}
}
