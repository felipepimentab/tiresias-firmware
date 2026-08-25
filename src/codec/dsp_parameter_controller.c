/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_controller.h"

#include "dsp_parameter_catalog.h"
#include "dsp_parameter_settings.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(dsp_parameter_controller, CONFIG_LOG_DEFAULT_LEVEL);

static K_MUTEX_DEFINE(parameter_mutex);
static uint8_t param_adc_select[4U];
static uint8_t param_source_select[4U];
static uint8_t param_band_1_compressor_lut[136U];
static uint8_t param_band_2_compressor_lut[136U];
static uint8_t param_band_3_compressor_lut[136U];
static uint8_t param_band_4_compressor_lut[136U];
static uint8_t param_band_5_compressor_lut[136U];
static uint8_t param_band_6_compressor_lut[136U];
static uint8_t param_band_7_compressor_lut[136U];
static uint8_t param_band_8_compressor_lut[136U];
static uint8_t param_phase_comp_gain_1[4U];
static uint8_t param_phase_comp_gain_2[4U];
static uint8_t param_phase_comp_gain_3[4U];
static uint8_t param_output_headroom_gain[4U];
static uint8_t param_soft_clip_lut[DSP_PARAMETER_MAX_BYTE_COUNT];

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

static uint8_t* parameter_bytes(const struct dsp_parameter* parameter, uint8_t byte_offset)
{
  return &parameter_data[parameter->id][byte_offset];
}

static bool parameter_range_valid(const struct dsp_parameter* parameter, uint8_t byte_offset, size_t size)
{
  return size > 0U && byte_offset < parameter->byte_count && size <= parameter->byte_count - byte_offset;
}

static int load_parameter_defaults(void)
{
  size_t byte_count = 0U;

  for (size_t parameter_index = 0; parameter_index < DSP_PARAMETER_COUNT; parameter_index++) {
    const struct dsp_parameter* parameter = &dsp_parameter_contract[parameter_index];
    size_t size = parameter->byte_count;

    if (parameter_data[parameter->id] == NULL) {
      return -EINVAL;
    }

    /* TODO: Load generated defaults through Codec Adapter when hardware integration resumes. */
    memset(parameter_data[parameter->id], 0, size);
    byte_count += size;
  }

  if (byte_count != DSP_PARAMETER_BYTE_COUNT) {
    return -EINVAL;
  }

  atomic_clear(&current_revision);
  return 0;
}

static int persist_bytes(
    const struct dsp_parameter* parameter, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision)
{
  uint32_t pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  uint8_t pending_parameter[DSP_PARAMETER_MAX_BYTE_COUNT];
  int ret;

  memcpy(pending_parameter, parameter_data[parameter->id], parameter->byte_count);
  memcpy(&pending_parameter[byte_offset], data, size);

  ret = dsp_parameter_settings_save(parameter->id, pending_parameter, parameter->byte_count);
  if (ret != 0) {
    return ret;
  }

  memcpy(parameter_data[parameter->id], pending_parameter, parameter->byte_count);
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

  ret = load_parameter_defaults();
  if (ret != 0) {
    goto out;
  }

  ret = dsp_parameter_settings_load(parameter_data, false);
  if (ret != 0) {
    goto out;
  }

  /* TODO: Restore the complete parameter state through Codec Adapter when hardware integration resumes. */

  atomic_set(&parameters_initialized, 1);

out:
  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int dsp_parameter_controller_get(uint8_t id, uint8_t byte_offset, uint8_t* data, size_t size, uint32_t* revision)
{
  const struct dsp_parameter* parameter = dsp_parameter_definition(id);

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
  memcpy(data, parameter_bytes(parameter, byte_offset), size);
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);

  return 0;
}

int dsp_parameter_controller_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision)
{
  const struct dsp_parameter* parameter = dsp_parameter_definition(id);
  int ret;

  if (data == NULL || revision == NULL) {
    return -EINVAL;
  }
  if (parameter == NULL) {
    return -ENOENT;
  }
  if ((parameter->flags & DSP_PARAMETER_CONTRACT_FLAG_WRITABLE) == 0U) {
    return -EACCES;
  }
  if (!parameter_range_valid(parameter, byte_offset, size)) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);

  if (memcmp(parameter_bytes(parameter, byte_offset), data, size) == 0) {
    *revision = (uint32_t)atomic_get(&current_revision);
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  /* TODO: Apply the update through Codec Adapter before committing it to flash and RAM. */

  ret = persist_bytes(parameter, byte_offset, data, size, revision);
  if (ret != 0) {
    LOG_ERR("Failed to persist DSP parameter %u bytes %u..%zu: %d", id, byte_offset, byte_offset + size - 1U, ret);
  }

  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int dsp_parameter_controller_mirror_codec_update(
    uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision)
{
  const struct dsp_parameter* parameter = dsp_parameter_definition(id);
  int ret;

  if (data == NULL || revision == NULL) {
    return -EINVAL;
  }
  if (parameter == NULL) {
    return -ENOENT;
  }
  if (!parameter_range_valid(parameter, byte_offset, size)) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  /* TODO: Remove this compatibility path when the controller owns internal codec updates. */
  k_mutex_lock(&parameter_mutex, K_FOREVER);

  if (memcmp(parameter_bytes(parameter, byte_offset), data, size) == 0) {
    *revision = (uint32_t)atomic_get(&current_revision);
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  ret = persist_bytes(parameter, byte_offset, data, size, revision);
  if (ret != 0) {
    LOG_ERR("Failed to mirror DSP parameter %u bytes %u..%zu: %d", id, byte_offset, byte_offset + size - 1U, ret);
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
