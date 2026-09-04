/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_adapter.h"
#include "adau1787.h"
#include "sigma_exports.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(codec_adapter, CONFIG_LOG_DEFAULT_LEVEL);

#define ADC_SOURCE_SWITCH_ADDRESS MOD_ADCSELECT_MONOSWSLEW_ADDR
#define LISTENING_MODE_SWITCH_ADDRESS MOD_SOURCESELECT_STEREOSWSLEW_ADDR
#define LISTENING_MODE_I2S 0U
#define LISTENING_MODE_LOCAL 1U
#define SAFELOAD_MAX_WORDS 5U

int codec_param_read(uint16_t start_addr, uint8_t* data, size_t len)
{
  size_t available_bytes;
  int ret;

  if (data == NULL || len == 0U) {
    return -EINVAL;
  }
  if (!IS_PARAM_ADDR(start_addr)) {
    return -EINVAL;
  }

  available_bytes = (size_t)ADAU1787_PARAM_RAM_END - start_addr + 1U;
  if (len > available_bytes) {
    return -EINVAL;
  }
  if (((start_addr - ADAU1787_PARAM_RAM_BASE) % ADAU1787_PARAM_RAM_WIDTH_BYTES) != 0U
      || (len % ADAU1787_PARAM_RAM_WIDTH_BYTES) != 0U) {
    return -EINVAL;
  }

  ret = adau1787_read(start_addr, data, len);
  if (ret != 0) {
    LOG_ERR("Failed to read %zu parameter bytes at 0x%04X: %d", len, start_addr, ret);
  }

  return ret;
}

static int verify_param_write(uint16_t start_addr, const uint8_t* expected, size_t len)
{
  uint8_t actual[SAFELOAD_MAX_WORDS * ADAU1787_PARAM_RAM_WIDTH_BYTES];
  size_t offset = 0U;
  int ret;

  while (offset < len) {
    size_t chunk_len = len - offset;
    uint16_t chunk_addr = start_addr + (uint16_t)offset;

    if (chunk_len > sizeof(actual)) {
      chunk_len = sizeof(actual);
    }

    ret = codec_param_read(chunk_addr, actual, chunk_len);
    if (ret != 0) {
      return ret;
    }

    for (size_t index = 0U; index < chunk_len; index++) {
      if (actual[index] != expected[offset + index]) {
        LOG_ERR("Parameter verification failed at 0x%04X: wrote 0x%02X, read 0x%02X", chunk_addr + (uint16_t)index,
            expected[offset + index], actual[index]);
        return -EIO;
      }
    }

    offset += chunk_len;
  }

  return 0;
}

int codec_param_write(uint16_t start_addr, const uint8_t* data, size_t len)
{
  size_t available_bytes;
  reg_word_t sdsp_run;
  int restart_ret;
  int ret;

  if (data == NULL || len == 0U) {
    return -EINVAL;
  }
  if (!IS_PARAM_ADDR(start_addr)) {
    return -EINVAL;
  }

  available_bytes = (size_t)ADAU1787_PARAM_RAM_END - start_addr + 1U;
  if (len > available_bytes) {
    return -EINVAL;
  }
  if (((start_addr - ADAU1787_PARAM_RAM_BASE) % ADAU1787_PARAM_RAM_WIDTH_BYTES) != 0U
      || (len % ADAU1787_PARAM_RAM_WIDTH_BYTES) != 0U) {
    return -EINVAL;
  }

  if (len <= SAFELOAD_MAX_WORDS * ADAU1787_PARAM_RAM_WIDTH_BYTES) {
    /* TODO: Make the ADAU1787 write APIs accept const data buffers and remove these casts. */
    ret = adau1787_safeload_write(start_addr, (uint8_t*)data, len / ADAU1787_PARAM_RAM_WIDTH_BYTES);
    if (ret != 0) {
      return ret;
    }

    return verify_param_write(start_addr, data, len);
  }

  sdsp_run = 0U;
  ret = adau1787_write_register(REG_SDSP_CTRL2_IC_1_Sigma_ADDR, &sdsp_run);
  if (ret != 0) {
    LOG_ERR("Failed to stop SigmaDSP before parameter write: %d", ret);
    return ret;
  }

  ret = adau1787_write(start_addr, (uint8_t*)data, len);
  if (ret != 0) {
    LOG_ERR("Failed to write %zu parameter bytes at 0x%04X: %d", len, start_addr, ret);
  } else {
    ret = verify_param_write(start_addr, data, len);
  }

  sdsp_run = R110_SDSP_RUN_IC_1_Sigma;
  restart_ret = adau1787_write_register(REG_SDSP_CTRL2_IC_1_Sigma_ADDR, &sdsp_run);
  if (restart_ret != 0) {
    LOG_ERR("Failed to restart SigmaDSP after parameter write: %d", restart_ret);
    if (ret == 0) {
      ret = restart_ret;
    }
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

int codec_adapter_init(void)
{
  return adau1787_init();
}

int codec_adapter_select_local(void)
{
  return select_listening_mode(LISTENING_MODE_LOCAL);
}

int codec_adapter_select_i2s(void)
{
  return select_listening_mode(LISTENING_MODE_I2S);
}
