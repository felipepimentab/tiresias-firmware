/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_parameters.h"

#include "codec_contract.h"
#include "codec_settings.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

static K_MUTEX_DEFINE(parameter_mutex);

static uint8_t adc_select[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t source_select[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t band_1_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_2_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_3_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_4_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_5_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_6_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_7_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_8_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t phase_comp_gain_1[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t phase_comp_gain_2[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t phase_comp_gain_3[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t output_headroom_gain[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t soft_clip_lut[CODEC_BYTES_FROM_WORDS(45)];

static uint8_t* const parameter_storage[] = {
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_ADC_SELECT)] = adc_select,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_SOURCE_SELECT)] = source_select,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_1_COMPRESSOR_LUT)] = band_1_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_2_COMPRESSOR_LUT)] = band_2_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_3_COMPRESSOR_LUT)] = band_3_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_4_COMPRESSOR_LUT)] = band_4_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_5_COMPRESSOR_LUT)] = band_5_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_6_COMPRESSOR_LUT)] = band_6_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_7_COMPRESSOR_LUT)] = band_7_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_8_COMPRESSOR_LUT)] = band_8_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_PHASE_COMP_GAIN_1)] = phase_comp_gain_1,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_PHASE_COMP_GAIN_2)] = phase_comp_gain_2,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_PHASE_COMP_GAIN_3)] = phase_comp_gain_3,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN)] = output_headroom_gain,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_SOFT_CLIP_LUT)] = soft_clip_lut,
};

static atomic_t current_revision;
static atomic_t parameters_initialized;

static uint8_t* parameter_value(const struct codec_parameter* parameter, uint8_t byte_offset)
{
  return &parameter_storage[CODEC_PARAMETER_INDEX(parameter->id)][byte_offset];
}

static bool parameter_range_valid(const struct codec_parameter* parameter, uint8_t byte_offset, size_t size)
{
  return size > 0U && byte_offset < parameter->byte_count && size <= parameter->byte_count - byte_offset;
}

static int persist_parameter(
    const struct codec_parameter* parameter, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision)
{
  uint8_t pending_parameter[parameter->byte_count];
  uint32_t pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  uint8_t* stored_parameter = parameter_value(parameter, 0U);
  int ret;

  memcpy(pending_parameter, stored_parameter, parameter->byte_count);
  memcpy(&pending_parameter[byte_offset], data, size);

  ret = codec_settings_save(parameter->id, pending_parameter, parameter->byte_count);
  if (ret != 0) {
    return ret;
  }

  memcpy(stored_parameter, pending_parameter, parameter->byte_count);
  atomic_set(&current_revision, (atomic_val_t)pending_revision);
  *revision = pending_revision;
  return 0;
}

int codec_parameters_init(void)
{
  int ret = 0;

  if (atomic_get(&parameters_initialized) != 0) {
    return 0;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  if (atomic_get(&parameters_initialized) != 0) {
    goto out;
  }

  ret = codec_settings_init();
  if (ret != 0) {
    goto out;
  }

  for (size_t index = 0U; index < ARRAY_SIZE(parameter_storage); index++) {
    memset(parameter_storage[index], 0, codec_contract[index].byte_count);
  }
  atomic_clear(&current_revision);

  for (size_t index = 0U; index < CODEC_PARAMETER_COUNT; index++) {
    const struct codec_parameter* parameter = &codec_contract[index];

    ret = codec_settings_load(parameter->id, parameter_value(parameter, 0U), parameter->byte_count);
    if (ret != 0) {
      goto out;
    }
  }

  /* TODO: Apply the restored parameter buffers through Codec Adapter after the proof of concept. */
  atomic_set(&parameters_initialized, 1);

out:
  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int codec_parameters_get(uint8_t id, uint8_t byte_offset, uint8_t* data, size_t size, uint32_t* revision)
{
  const struct codec_parameter* parameter = codec_contract_find(id);

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (data == NULL || revision == NULL) {
    return -EINVAL;
  }
  if (!parameter_range_valid(parameter, byte_offset, size)) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  memcpy(data, parameter_value(parameter, byte_offset), size);
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);
  return 0;
}

int codec_parameters_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision)
{
  const struct codec_parameter* parameter = codec_contract_find(id);
  int ret;

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (data == NULL || revision == NULL) {
    return -EINVAL;
  }
  if ((parameter->flags & CODEC_CONTRACT_FLAG_WRITABLE) == 0U) {
    return -EACCES;
  }
  if (!parameter_range_valid(parameter, byte_offset, size)) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  if (memcmp(parameter_value(parameter, byte_offset), data, size) == 0) {
    *revision = (uint32_t)atomic_get(&current_revision);
    ret = 0;
    goto out;
  }

  /* TODO: Apply the update through Codec Adapter before persistence after the proof of concept. */
  ret = persist_parameter(parameter, byte_offset, data, size, revision);

out:
  k_mutex_unlock(&parameter_mutex);
  return ret;
}

uint32_t codec_parameters_revision(void)
{
  return (uint32_t)atomic_get(&current_revision);
}

bool codec_parameters_loaded(void)
{
  return atomic_get(&parameters_initialized) != 0;
}

BUILD_ASSERT(ARRAY_SIZE(parameter_storage) == CODEC_PARAMETER_COUNT, "Every contract parameter needs RAM storage");
BUILD_ASSERT(sizeof(adc_select) + sizeof(source_select) + sizeof(band_1_compressor_lut) + sizeof(band_2_compressor_lut)
            + sizeof(band_3_compressor_lut) + sizeof(band_4_compressor_lut) + sizeof(band_5_compressor_lut)
            + sizeof(band_6_compressor_lut) + sizeof(band_7_compressor_lut) + sizeof(band_8_compressor_lut)
            + sizeof(phase_comp_gain_1) + sizeof(phase_comp_gain_2) + sizeof(phase_comp_gain_3)
            + sizeof(output_headroom_gain) + sizeof(soft_clip_lut)
        == CODEC_PARAMETER_BYTE_COUNT,
    "Parameter buffers must match the contract byte count");
