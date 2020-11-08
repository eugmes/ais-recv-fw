#include "radio_configs.h"
#include <autoconf.h>
#ifndef CONFIG_RADIO_IQ_CALIBRATION
#include "radio_config_ch1.h"
#else
#include "radio_config_ch1_cal.h"
#endif

const uint8_t radio_config_ch1[] = RADIO_CONFIGURATION_DATA_ARRAY;
