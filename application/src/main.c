/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <usb/usb_device.h>
#include <drivers/uart.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

void main(void)
{
	if (usb_enable(NULL)) {
		LOG_ERR("USB initialization failed.");
		return;
	}
}
