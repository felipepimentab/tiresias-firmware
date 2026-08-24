/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_controller.h"

#include "codec_adapter.h"
#include "dsp_parameter_catalog.h"
#include "dsp_parameter_settings.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(dsp_parameter_controller, CONFIG_LOG_DEFAULT_LEVEL);

static const uint8_t persistent_ids[DSP_PARAMETER_PERSISTENT_COUNT] = {
  DSP_PARAMETER_ID_ADC_SELECT,
  DSP_PARAMETER_ID_SOURCE_SELECT,
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_1,
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_2,
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_3,
  DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN,
};

static K_MUTEX_DEFINE(parameter_mutex);
static int32_t values[DSP_PARAMETER_PERSISTENT_COUNT];
static atomic_t current_revision;
static bool initialized;

static size_t persistent_index(uint8_t id)
{
  for (size_t index = 0; index < ARRAY_SIZE(persistent_ids); index++) {
    if (persistent_ids[index] == id) {
      return index;
    }
  }

  return ARRAY_SIZE(persistent_ids);
}

static int read_parameter_word(const struct dsp_parameter_descriptor* parameter, uint8_t word_index, int32_t* value)
{
  uint8_t word[CODEC_PARAMETER_WORD_SIZE];
  uint32_t raw;
  int ret;

  ret = codec_param_read(parameter->dsp_address + word_index * sizeof(word), word, sizeof(word));
  if (ret != 0) {
    return ret == -ENOTSUP ? ret : -EREMOTE;
  }

  raw = sys_get_be32(word);
  if ((parameter->definition->flags & DSP_PARAMETER_CONTRACT_FLAG_INTEGER) == 0U) {
    raw &= 0x0FFFFFFFU;
    if ((raw & BIT(27)) != 0U) {
      raw |= 0xF0000000U;
    }
  }
  *value = (int32_t)raw;

  return 0;
}

static int write_parameter_word(const struct dsp_parameter_descriptor* parameter, int32_t value)
{
  uint8_t word[CODEC_PARAMETER_WORD_SIZE];
  uint32_t raw = (uint32_t)value;
  int ret;

  if ((parameter->definition->flags & DSP_PARAMETER_CONTRACT_FLAG_INTEGER) == 0U) {
    raw &= 0x0FFFFFFFU;
  }
  sys_put_be32(raw, word);

  ret = codec_param_write(parameter->dsp_address, word, sizeof(word));
  return ret == 0 || ret == -ENOTSUP ? ret : -EREMOTE;
}

static void load_defaults(void)
{
  for (size_t index = 0; index < ARRAY_SIZE(persistent_ids); index++) {
    const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(persistent_ids[index]);

    __ASSERT_NO_MSG(parameter != NULL);
    values[index] = parameter->default_value;
  }

  atomic_clear(&current_revision);
}

static int load_persistent_values(void)
{
  struct dsp_parameter_settings_snapshot snapshot;
  int ret;

  ret = dsp_parameter_settings_load(&snapshot);
  if (ret == -ENOENT || ret == -EINVAL) {
    return 0;
  }
  if (ret != 0) {
    return ret;
  }

  for (size_t index = 0; index < DSP_PARAMETER_PERSISTENT_COUNT; index++) {
    const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(persistent_ids[index]);
    ret = dsp_parameter_validate(parameter, 0U, snapshot.values[index]);

    if (ret != 0) {
      LOG_WRN("Ignoring invalid stored value for DSP parameter %u", persistent_ids[index]);
      return 0;
    }
  }

  memcpy(values, snapshot.values, sizeof(values));
  atomic_set(&current_revision, (atomic_val_t)snapshot.revision);
  return 0;
}

int dsp_parameter_controller_init(void)
{
  int ret;

  if (initialized) {
    return 0;
  }

  load_defaults();
  ret = load_persistent_values();
  if (ret != 0) {
    return ret;
  }

  initialized = true;
  return 0;
}

int dsp_parameter_controller_get(uint8_t id, uint8_t word_index, int32_t* value, uint32_t* revision)
{
  const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(id);
  int ret;

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (value == NULL || revision == NULL) {
    return -EINVAL;
  }
  if (word_index >= parameter->definition->word_count) {
    return -ERANGE;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  ret = read_parameter_word(parameter, word_index, value);
  if (ret == -ENOTSUP) {
    size_t index = persistent_index(id);

    if (word_index == 0U && index < ARRAY_SIZE(values)) {
      *value = values[index];
      ret = 0;
    } else {
      ret = -EREMOTE;
    }
  }
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);

  return ret;
}

int dsp_parameter_controller_set(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision)
{
  const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(id);
  struct dsp_parameter_settings_snapshot pending_snapshot;
  size_t index;
  bool codec_applied;
  int rollback_ret;
  int ret;

  if (revision == NULL) {
    return -EINVAL;
  }

  ret = dsp_parameter_validate(parameter, word_index, value);
  if (ret != 0) {
    return ret;
  }

  index = persistent_index(id);
  if (index >= ARRAY_SIZE(values)) {
    return -EINVAL;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  ret = write_parameter_word(parameter, value);
  codec_applied = ret == 0;
  if (ret != 0 && ret != -ENOTSUP) {
    k_mutex_unlock(&parameter_mutex);
    LOG_ERR("Failed to write codec parameter %u: %d", id, ret);
    return ret;
  }

  memcpy(pending_snapshot.values, values, sizeof(values));
  pending_snapshot.values[index] = value;
  pending_snapshot.revision = (uint32_t)atomic_get(&current_revision) + 1U;

  ret = dsp_parameter_settings_save(&pending_snapshot);
  if (ret != 0) {
    if (codec_applied) {
      rollback_ret = write_parameter_word(parameter, values[index]);
      if (rollback_ret != 0) {
        LOG_ERR("Failed to roll back codec parameter %u: %d", id, rollback_ret);
      }
    }
    k_mutex_unlock(&parameter_mutex);
    LOG_ERR("Failed to persist DSP parameter %u: %d", id, ret);
    return ret;
  }

  memcpy(values, pending_snapshot.values, sizeof(values));
  atomic_set(&current_revision, (atomic_val_t)pending_snapshot.revision);
  *revision = pending_snapshot.revision;
  k_mutex_unlock(&parameter_mutex);

  return 0;
}

uint32_t dsp_parameter_controller_revision(void)
{
  return (uint32_t)atomic_get(&current_revision);
}

bool dsp_parameter_controller_loaded(void)
{
  return initialized;
}
