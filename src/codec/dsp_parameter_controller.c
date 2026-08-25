/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_controller.h"

#include "codec_adapter.h"
#include "dsp_parameter_catalog.h"
#include "dsp_parameter_settings.h"
#include "param_defaults.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(dsp_parameter_controller, CONFIG_LOG_DEFAULT_LEVEL);

#define PARAMETER_BYTE_COUNT(word_count) ((word_count) * CODEC_PARAMETER_WORD_SIZE)

static K_MUTEX_DEFINE(parameter_mutex);
static uint8_t param_adc_select[PARAMETER_BYTE_COUNT(1U)];
static uint8_t param_source_select[PARAMETER_BYTE_COUNT(1U)];
static uint8_t param_band_1_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_2_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_3_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_4_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_5_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_6_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_7_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_band_8_compressor_lut[PARAMETER_BYTE_COUNT(34U)];
static uint8_t param_phase_comp_gain_1[PARAMETER_BYTE_COUNT(1U)];
static uint8_t param_phase_comp_gain_2[PARAMETER_BYTE_COUNT(1U)];
static uint8_t param_phase_comp_gain_3[PARAMETER_BYTE_COUNT(1U)];
static uint8_t param_output_headroom_gain[PARAMETER_BYTE_COUNT(1U)];
static uint8_t param_soft_clip_lut[PARAMETER_BYTE_COUNT(45U)];

static uint8_t* const parameter_data[DSP_PARAMETER_COUNT + 1U] = {
  [DSP_PARAMETER_ID_ADC_SELECT] = param_adc_select,
  [DSP_PARAMETER_ID_SOURCE_SELECT] = param_source_select,
  [DSP_PARAMETER_ID_BAND_1_COMPRESSOR_LUT] = param_band_1_compressor_lut,
  [DSP_PARAMETER_ID_BAND_2_COMPRESSOR_LUT] = param_band_2_compressor_lut,
  [DSP_PARAMETER_ID_BAND_3_COMPRESSOR_LUT] = param_band_3_compressor_lut,
  [DSP_PARAMETER_ID_BAND_4_COMPRESSOR_LUT] = param_band_4_compressor_lut,
  [DSP_PARAMETER_ID_BAND_5_COMPRESSOR_LUT] = param_band_5_compressor_lut,
  [DSP_PARAMETER_ID_BAND_6_COMPRESSOR_LUT] = param_band_6_compressor_lut,
  [DSP_PARAMETER_ID_BAND_7_COMPRESSOR_LUT] = param_band_7_compressor_lut,
  [DSP_PARAMETER_ID_BAND_8_COMPRESSOR_LUT] = param_band_8_compressor_lut,
  [DSP_PARAMETER_ID_PHASE_COMP_GAIN_1] = param_phase_comp_gain_1,
  [DSP_PARAMETER_ID_PHASE_COMP_GAIN_2] = param_phase_comp_gain_2,
  [DSP_PARAMETER_ID_PHASE_COMP_GAIN_3] = param_phase_comp_gain_3,
  [DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN] = param_output_headroom_gain,
  [DSP_PARAMETER_ID_SOFT_CLIP_LUT] = param_soft_clip_lut,
};

static atomic_t current_revision;
static atomic_t parameters_initialized;

BUILD_ASSERT(ARRAY_SIZE(parameter_data) == DSP_PARAMETER_COUNT + 1U,
    "Parameter byte arrays must be indexed by every parameter ID");
BUILD_ASSERT(sizeof(param_adc_select) + sizeof(param_source_select) + sizeof(param_band_1_compressor_lut)
            + sizeof(param_band_2_compressor_lut) + sizeof(param_band_3_compressor_lut)
            + sizeof(param_band_4_compressor_lut) + sizeof(param_band_5_compressor_lut)
            + sizeof(param_band_6_compressor_lut) + sizeof(param_band_7_compressor_lut)
            + sizeof(param_band_8_compressor_lut) + sizeof(param_phase_comp_gain_1) + sizeof(param_phase_comp_gain_2)
            + sizeof(param_phase_comp_gain_3) + sizeof(param_output_headroom_gain) + sizeof(param_soft_clip_lut)
        == DSP_PARAMETER_BYTE_COUNT,
    "Parameter byte arrays must cover the complete catalog");

static const struct dsp_parameter* parameter_definition(uint8_t id)
{
  const struct dsp_parameter* parameter;

  if (id == 0U || id > DSP_PARAMETER_COUNT) {
    return NULL;
  }

  parameter = &dsp_parameter_contract[id - 1U];
  return parameter->id == id ? parameter : NULL;
}

static size_t parameter_byte_count(const struct dsp_parameter* parameter)
{
  return PARAMETER_BYTE_COUNT(parameter->word_count);
}

static uint8_t* parameter_word(const struct dsp_parameter* parameter, uint8_t word_index)
{
  return &parameter_data[parameter->id][PARAMETER_BYTE_COUNT(word_index)];
}

static int32_t decode_parameter_word(
    const struct dsp_parameter* parameter, const uint8_t word[CODEC_PARAMETER_WORD_SIZE])
{
  uint32_t raw = sys_get_be32(word);

  if ((parameter->flags & DSP_PARAMETER_CONTRACT_FLAG_INTEGER) == 0U) {
    raw &= 0x0FFFFFFFU;
    if ((raw & BIT(27)) != 0U) {
      raw |= 0xF0000000U;
    }
  }

  return (int32_t)raw;
}

static void encode_parameter_word(
    const struct dsp_parameter* parameter, int32_t value, uint8_t word[CODEC_PARAMETER_WORD_SIZE])
{
  uint32_t raw = (uint32_t)value;

  if ((parameter->flags & DSP_PARAMETER_CONTRACT_FLAG_INTEGER) == 0U) {
    raw &= 0x0FFFFFFFU;
  }
  sys_put_be32(raw, word);
}

static int load_sigma_defaults(void)
{
#if !defined(CONFIG_AUDIO_CODEC_ADAU1787)
  return -ENOTSUP;
#else
  size_t byte_count = 0U;

  for (size_t parameter_index = 0; parameter_index < DSP_PARAMETER_COUNT; parameter_index++) {
    const struct dsp_parameter* parameter = &dsp_parameter_contract[parameter_index];
    const uint8_t* defaults = dsp_parameter_default(parameter->id);
    size_t size = parameter_byte_count(parameter);

    if (defaults == NULL || parameter_data[parameter->id] == NULL) {
      return -EINVAL;
    }

    memcpy(parameter_data[parameter->id], defaults, size);
    byte_count += size;
  }

  if (byte_count != DSP_PARAMETER_BYTE_COUNT) {
    return -EINVAL;
  }

  atomic_clear(&current_revision);
  return 0;
#endif
}

static int write_codec_word(
    const struct dsp_parameter* parameter, uint8_t word_index, const uint8_t word[CODEC_PARAMETER_WORD_SIZE])
{
  int ret;

  ret = codec_param_write(
      dsp_parameter_addresses[parameter->id] + PARAMETER_BYTE_COUNT(word_index), word, CODEC_PARAMETER_WORD_SIZE);

  return ret == 0 ? 0 : -EREMOTE;
}

static int write_all_codec_values(void)
{
  int ret;

  for (size_t parameter_index = 0; parameter_index < DSP_PARAMETER_COUNT; parameter_index++) {
    const struct dsp_parameter* parameter = &dsp_parameter_contract[parameter_index];

    for (uint8_t word_index = 0U; word_index < parameter->word_count; word_index++) {
      ret = write_codec_word(parameter, word_index, parameter_word(parameter, word_index));
      if (ret != 0) {
        LOG_ERR("Failed to restore DSP parameter %u word %u", parameter->id, word_index);
        return ret;
      }
    }
  }

  return 0;
}

static int persist_word(const struct dsp_parameter* parameter, uint8_t word_index,
    const uint8_t word[CODEC_PARAMETER_WORD_SIZE], uint32_t* revision)
{
  uint32_t pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  uint8_t previous_word[CODEC_PARAMETER_WORD_SIZE];
  uint8_t* current_word = parameter_word(parameter, word_index);
  int ret;

  memcpy(previous_word, current_word, sizeof(previous_word));
  memcpy(current_word, word, CODEC_PARAMETER_WORD_SIZE);

  ret = dsp_parameter_settings_save(parameter->id, parameter_data[parameter->id], parameter_byte_count(parameter));
  if (ret != 0) {
    memcpy(current_word, previous_word, sizeof(previous_word));
    return ret;
  }

  atomic_set(&current_revision, (atomic_val_t)pending_revision);
  *revision = pending_revision;
  return 0;
}

int dsp_parameter_controller_init(void)
{
  int ret;

  if (atomic_get(&parameters_initialized) != 0) {
    return 0;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  if (atomic_get(&parameters_initialized) != 0) {
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  ret = load_sigma_defaults();
  if (ret != 0) {
    goto out;
  }

  ret = dsp_parameter_settings_load(parameter_data);
  if (ret != 0) {
    goto out;
  }

  ret = write_all_codec_values();
  if (ret != 0) {
    goto out;
  }

  atomic_set(&parameters_initialized, 1);

out:
  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int dsp_parameter_controller_get(uint8_t id, uint8_t word_index, int32_t* value, uint32_t* revision)
{
  const struct dsp_parameter* parameter = parameter_definition(id);

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (value == NULL || revision == NULL) {
    return -EINVAL;
  }
  if (word_index >= parameter->word_count) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  *value = decode_parameter_word(parameter, parameter_word(parameter, word_index));
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);

  return 0;
}

int dsp_parameter_controller_set(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision)
{
  const struct dsp_parameter* parameter = parameter_definition(id);
  uint8_t previous_word[CODEC_PARAMETER_WORD_SIZE];
  uint8_t pending_word[CODEC_PARAMETER_WORD_SIZE];
  int rollback_ret;
  int ret;

  if (revision == NULL) {
    return -EINVAL;
  }
  if (parameter == NULL) {
    return -ENOENT;
  }
  if (word_index >= parameter->word_count) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  encode_parameter_word(parameter, value, pending_word);
  k_mutex_lock(&parameter_mutex, K_FOREVER);

  memcpy(previous_word, parameter_word(parameter, word_index), sizeof(previous_word));
  if (memcmp(previous_word, pending_word, sizeof(previous_word)) == 0) {
    *revision = (uint32_t)atomic_get(&current_revision);
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  ret = write_codec_word(parameter, word_index, pending_word);
  if (ret != 0) {
    LOG_ERR("Failed to write DSP parameter %u word %u", id, word_index);
    k_mutex_unlock(&parameter_mutex);
    return ret;
  }

  ret = persist_word(parameter, word_index, pending_word, revision);
  if (ret != 0) {
    rollback_ret = write_codec_word(parameter, word_index, previous_word);
    if (rollback_ret != 0) {
      LOG_ERR("Failed to roll back DSP parameter %u word %u", id, word_index);
    }
    LOG_ERR("Failed to persist DSP parameter %u word %u: %d", id, word_index, ret);
  }

  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int dsp_parameter_controller_mirror_codec_update(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision)
{
  const struct dsp_parameter* parameter = parameter_definition(id);
  uint8_t pending_word[CODEC_PARAMETER_WORD_SIZE];
  int ret;

  if (revision == NULL) {
    return -EINVAL;
  }
  if (parameter == NULL) {
    return -ENOENT;
  }
  if (word_index >= parameter->word_count) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  encode_parameter_word(parameter, value, pending_word);
  k_mutex_lock(&parameter_mutex, K_FOREVER);

  if (memcmp(parameter_word(parameter, word_index), pending_word, sizeof(pending_word)) == 0) {
    *revision = (uint32_t)atomic_get(&current_revision);
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  ret = persist_word(parameter, word_index, pending_word, revision);
  if (ret != 0) {
    LOG_ERR("Failed to mirror DSP parameter %u word %u: %d", id, word_index, ret);
  }

  k_mutex_unlock(&parameter_mutex);
  return ret;
}

uint32_t dsp_parameter_controller_revision(void)
{
  return (uint32_t)atomic_get(&current_revision);
}

bool dsp_parameter_controller_loaded(void)
{
  return atomic_get(&parameters_initialized) != 0;
}
