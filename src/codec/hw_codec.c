/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "hw_codec.h"

#include "adau1787.h"
#include "adau_1787_IC_1_SIGMA_PARAM.h"

#include <errno.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hw_codec, CONFIG_MODULE_HW_CODEC_LOG_LEVEL);

#define ADC_SOURCE_SWITCH_ADDRESS MOD_ADCSELECT_MONOSWSLEW_ADDR
#define LISTENING_MODE_SWITCH_ADDRESS MOD_SOURCESELECT_STEREOSWSLEW_ADDR
#define LISTENING_MODE_I2S 0U
#define LISTENING_MODE_LOCAL 1U

BUILD_ASSERT(MOD_ADCSELECT_COUNT == 1, "ADC Select block must contain one parameter");
BUILD_ASSERT(IS_PARAM_ADDR(ADC_SOURCE_SWITCH_ADDRESS), "ADC Select parameter must be in parameter RAM");

typedef enum source_adc {
  ADC_0,
  ADC_1,
  ADC_2,
  ADC_3,
} source_adc;

static int set_source_adc(source_adc source)
{
  param_word_t param = {
    0U,
    0U,
    0U,
    (uint8_t)source,
  };
  int ret;

  if ((uint32_t)source > (uint32_t)ADC_3) {
    LOG_ERR("Invalid ADC source: %d", source);
    return -EINVAL;
  }

  ret = adau1787_safeload_write(ADC_SOURCE_SWITCH_ADDRESS, param, MOD_ADCSELECT_COUNT);
  if (ret != 0) {
    LOG_ERR("Failed to select ADC source %d at 0x%04X: %d", source, ADC_SOURCE_SWITCH_ADDRESS, ret);
  }

  return ret;
}

static int set_dac_mute(bool mute)
{
  reg_word_t dac_ctrl2;
  int ret;

  ret = adau1787_read_register(REG_DAC_CTRL2_IC_1_Sigma_ADDR, &dac_ctrl2);
  if (ret != 0) {
    LOG_ERR("Failed to read DAC mute controls: %d", ret);
    return ret;
  }

  if (mute) {
    dac_ctrl2 |= R56_DAC0_MUTE_IC_1_Sigma_MASK | R56_DAC1_MUTE_IC_1_Sigma_MASK;
  } else {
    dac_ctrl2 &= ~(R56_DAC0_MUTE_IC_1_Sigma_MASK | R56_DAC1_MUTE_IC_1_Sigma_MASK);
  }

  ret = adau1787_write_register(REG_DAC_CTRL2_IC_1_Sigma_ADDR, &dac_ctrl2);
  if (ret != 0) {
    LOG_ERR("Failed to %s DAC outputs: %d", mute ? "mute" : "unmute", ret);
  }

  return ret;
}

static int select_listening_mode(uint32_t mode)
{
  param_word_t codec_param = {
    (mode >> 24) & 0xFF,
    (mode >> 16) & 0xFF,
    (mode >> 8) & 0xFF,
    mode & 0xFF,
  };
  int ret;

  ret = adau1787_safeload_write(LISTENING_MODE_SWITCH_ADDRESS, codec_param, 1U);
  if (ret != 0) {
    LOG_ERR("Failed to select listening mode %u at 0x%04X: %d", mode, LISTENING_MODE_SWITCH_ADDRESS, ret);
  }

  return ret;
}

int hw_codec_volume_set(uint8_t set_val)
{
  (void)set_val;

  return 0;
}

int hw_codec_volume_adjust(int8_t adjustment)
{
  (void)adjustment;

  return 0;
}

int hw_codec_volume_decrease(void)
{
  return 0;
}

int hw_codec_volume_increase(void)
{
  return 0;
}

int hw_codec_volume_mute(void)
{
  return set_dac_mute(true);
}

int hw_codec_volume_unmute(void)
{
  return set_dac_mute(false);
}

int hw_codec_default_conf_enable(void)
{
  return 0;
}

int hw_codec_soft_reset(void)
{
  return 0;
}

int hw_codec_init(void)
{
  int ret;

  ret = adau1787_init();
  if (ret != 0) {
    return ret;
  }

  return set_source_adc(ADC_0);
}

int hw_codec_select_local(void)
{
  return select_listening_mode(LISTENING_MODE_LOCAL);
}

int hw_codec_select_i2s(void)
{
  return select_listening_mode(LISTENING_MODE_I2S);
}

void hw_codec_log_status_2(void)
{
  adau1787_log_status_2();
}
