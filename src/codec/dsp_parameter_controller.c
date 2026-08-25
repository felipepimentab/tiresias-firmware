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

static K_MUTEX_DEFINE(parameter_mutex);
static int32_t parameter_values[DSP_PARAMETER_WORD_COUNT];
static struct dsp_parameter_settings_snapshot working_snapshot;
static atomic_t current_revision;
static atomic_t parameters_initialized;

static const struct dsp_parameter* parameter_definition(uint8_t id)
{
  const struct dsp_parameter* parameter;

  if (id == 0U || id > DSP_PARAMETER_COUNT) {
    return NULL;
  }

  parameter = &dsp_parameter_contract[id - 1U];
  return parameter->id == id ? parameter : NULL;
}

static size_t parameter_word_offset(const struct dsp_parameter* parameter, uint8_t word_index)
{
  size_t offset = word_index;

  for (size_t index = 0; index < parameter->id - 1U; index++) {
    offset += dsp_parameter_contract[index].word_count;
  }

  return offset;
}

#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
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
#endif

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
  size_t value_index = 0U;

  for (size_t parameter_index = 0; parameter_index < DSP_PARAMETER_COUNT; parameter_index++) {
    const struct dsp_parameter* parameter = &dsp_parameter_contract[parameter_index];
    const uint8_t* defaults = dsp_parameter_defaults[parameter->id];

    if (defaults == NULL) {
      return -EINVAL;
    }

    for (uint8_t word_index = 0U; word_index < parameter->word_count; word_index++) {
      if (value_index >= ARRAY_SIZE(parameter_values)) {
        return -EOVERFLOW;
      }

      parameter_values[value_index++]
          = decode_parameter_word(parameter, &defaults[word_index * CODEC_PARAMETER_WORD_SIZE]);
    }
  }

  if (value_index != ARRAY_SIZE(parameter_values)) {
    return -EINVAL;
  }

  atomic_clear(&current_revision);
  return 0;
#endif
}

static int write_codec_word(const struct dsp_parameter* parameter, uint8_t word_index, int32_t value)
{
  uint8_t word[CODEC_PARAMETER_WORD_SIZE];
  int ret;

  encode_parameter_word(parameter, value, word);
  ret = codec_param_write(dsp_parameter_addresses[parameter->id] + word_index * sizeof(word), word, sizeof(word));

  return ret == 0 ? 0 : -EREMOTE;
}

static int write_all_codec_values(void)
{
  size_t value_index = 0U;
  int ret;

  for (size_t parameter_index = 0; parameter_index < DSP_PARAMETER_COUNT; parameter_index++) {
    const struct dsp_parameter* parameter = &dsp_parameter_contract[parameter_index];

    for (uint8_t word_index = 0U; word_index < parameter->word_count; word_index++) {
      ret = write_codec_word(parameter, word_index, parameter_values[value_index++]);
      if (ret != 0) {
        LOG_ERR("Failed to restore DSP parameter %u word %u", parameter->id, word_index);
        return ret;
      }
    }
  }

  return 0;
}

static int persist_value(size_t value_index, int32_t value, uint32_t* revision)
{
  uint32_t pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  int ret;

  memcpy(working_snapshot.values, parameter_values, sizeof(parameter_values));
  working_snapshot.values[value_index] = value;
  working_snapshot.revision = pending_revision;

  ret = dsp_parameter_settings_save(&working_snapshot);
  if (ret != 0) {
    return ret;
  }

  parameter_values[value_index] = value;
  atomic_set(&current_revision, (atomic_val_t)pending_revision);
  *revision = pending_revision;
  return 0;
}

int dsp_parameter_controller_init(void)
{
  bool stored_values_differ;
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

  ret = dsp_parameter_settings_load(&working_snapshot);
  if (ret == -ENOENT || ret == -EINVAL) {
    memcpy(working_snapshot.values, parameter_values, sizeof(parameter_values));
    working_snapshot.revision = 0U;
    ret = dsp_parameter_settings_save(&working_snapshot);
    if (ret == 0) {
      atomic_set(&parameters_initialized, 1);
    }
    goto out;
  }
  if (ret != 0) {
    goto out;
  }

  stored_values_differ = memcmp(parameter_values, working_snapshot.values, sizeof(parameter_values)) != 0;
  memcpy(parameter_values, working_snapshot.values, sizeof(parameter_values));
  atomic_set(&current_revision, (atomic_val_t)working_snapshot.revision);

  if (stored_values_differ) {
    ret = write_all_codec_values();
    if (ret != 0) {
      goto out;
    }
  }

  atomic_set(&parameters_initialized, 1);

out:
  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int dsp_parameter_controller_get(uint8_t id, uint8_t word_index, int32_t* value, uint32_t* revision)
{
  const struct dsp_parameter* parameter = parameter_definition(id);
  size_t value_index;

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

  value_index = parameter_word_offset(parameter, word_index);
  k_mutex_lock(&parameter_mutex, K_FOREVER);
  *value = parameter_values[value_index];
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);

  return 0;
}

int dsp_parameter_controller_set(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision)
{
  const struct dsp_parameter* parameter = parameter_definition(id);
  size_t value_index;
  int32_t previous_value;
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

  value_index = parameter_word_offset(parameter, word_index);
  k_mutex_lock(&parameter_mutex, K_FOREVER);

  previous_value = parameter_values[value_index];
  if (previous_value == value) {
    *revision = (uint32_t)atomic_get(&current_revision);
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  ret = write_codec_word(parameter, word_index, value);
  if (ret != 0) {
    LOG_ERR("Failed to write DSP parameter %u word %u", id, word_index);
    k_mutex_unlock(&parameter_mutex);
    return ret;
  }

  ret = persist_value(value_index, value, revision);
  if (ret != 0) {
    rollback_ret = write_codec_word(parameter, word_index, previous_value);
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
  size_t value_index;
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

  value_index = parameter_word_offset(parameter, word_index);
  k_mutex_lock(&parameter_mutex, K_FOREVER);

  if (parameter_values[value_index] == value) {
    *revision = (uint32_t)atomic_get(&current_revision);
    k_mutex_unlock(&parameter_mutex);
    return 0;
  }

  ret = persist_value(value_index, value, revision);
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
