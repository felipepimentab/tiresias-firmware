/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_parameters.h"

#include "codec_contract.h"
#include "codec_settings.h"
#include "codec_values.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

static K_MUTEX_DEFINE(parameter_mutex);

static atomic_t current_revision;
static atomic_t parameters_initialized;

static uint8_t* parameter_value(const struct codec_parameter* parameter, uint8_t byte_offset)
{
  return &codec_values_get(parameter->id)[byte_offset];
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

  codec_values_reset();
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
