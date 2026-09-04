/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_parameters.h"

#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
#include "codec_adapter.h"
#include "codec_program.h"
#endif
#include "codec_contract.h"
#include "codec_defaults.h"
#include "codec_settings.h"
#include "codec_values.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

static K_MUTEX_DEFINE(parameter_mutex);

static atomic_t current_revision;
static atomic_t parameters_initialized;

static uint8_t* parameter_value(const struct codec_parameter* parameter)
{
  return codec_values_get(parameter->id);
}

#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
static int apply_parameter(const struct codec_parameter* parameter, const uint8_t* data)
{
  sub_addr_t address;
  int ret;

  ret = get_param_address(parameter->id, &address);
  if (ret != 0) {
    return ret;
  }

  return codec_param_write(address, data, parameter->byte_count);
}
#endif

static int persist_parameter(const struct codec_parameter* parameter, const uint8_t* data, uint32_t* revision)
{
  uint32_t pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  uint8_t* stored_parameter = parameter_value(parameter);
  int ret;

  ret = codec_settings_save(parameter->id, data, parameter->byte_count);
  if (ret != 0) {
    return ret;
  }

  memcpy(stored_parameter, data, parameter->byte_count);
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

  atomic_clear(&current_revision);

  for (size_t index = 0U; index < CODEC_PARAMETER_COUNT; index++) {
    const struct codec_parameter* parameter = &codec_contract[index];

    ret = codec_defaults_copy(parameter->id, parameter_value(parameter), parameter->byte_count);
    if (ret != 0) {
      goto out;
    }

    ret = codec_settings_load(parameter->id, parameter_value(parameter), parameter->byte_count);
    if (ret != 0) {
      goto out;
    }

#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
    ret = apply_parameter(parameter, parameter_value(parameter));
    if (ret != 0) {
      goto out;
    }
#endif
  }

  atomic_set(&parameters_initialized, 1);

out:
  k_mutex_unlock(&parameter_mutex);
  return ret;
}

int codec_parameters_get(uint8_t id, uint8_t* data, size_t size, uint32_t* revision)
{
  const struct codec_parameter* parameter = codec_contract_find(id);

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (data == NULL || revision == NULL) {
    return -EINVAL;
  }
  if (size != parameter->byte_count) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  memcpy(data, parameter_value(parameter), size);
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);
  return 0;
}

int codec_parameters_set(uint8_t id, const uint8_t* data, size_t size, uint32_t* revision)
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
  if (size != parameter->byte_count) {
    return -ERANGE;
  }
  if (atomic_get(&parameters_initialized) == 0) {
    return -EAGAIN;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  if (memcmp(parameter_value(parameter), data, size) == 0) {
    *revision = (uint32_t)atomic_get(&current_revision);
    ret = 0;
    goto out;
  }

#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
  ret = apply_parameter(parameter, data);
  if (ret != 0) {
    goto out;
  }
#endif

  ret = persist_parameter(parameter, data, revision);

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
