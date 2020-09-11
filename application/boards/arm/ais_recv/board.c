/*
 * Copyright (c) 2020 Ievgenii Meshcheriakov
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <init.h>

static int ais_recv_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Enable CRS Clock */
	__HAL_RCC_CRS_CLK_ENABLE();

	RCC_CRSInitTypeDef RCC_CRSInitStruct;

	/* Default Synchro Signal division factor (not divided) */
	RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;

	/* Set the SYNCSRC[1:0] bits according to CRS_Source value */
	RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;

	/* HSI48 is synchronized with USB SOF at 1KHz rate */
	RCC_CRSInitStruct.ReloadValue =  RCC_CRS_RELOADVALUE_DEFAULT;
	RCC_CRSInitStruct.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;

	RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;

	/* Set the TRIM[5:0] to the default value*/
	RCC_CRSInitStruct.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;

	/* Start automatic synchronization */
	HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

	return 0;
}

SYS_INIT(ais_recv_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
