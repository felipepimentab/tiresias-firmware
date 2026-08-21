/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_controller.h"

#include "codec_adapter.h"
#include "dsp_parameter_catalog.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

#define SETTINGS_KEY "tiresias/parameters"
#define STORE_MAGIC 0x54525053U
#define STORE_VERSION 2U
#define STORE_HEADER_SIZE 20U
#define STORE_CRC_SIZE 4U
#define STORE_SIZE (STORE_HEADER_SIZE + DSP_PARAMETER_PERSISTENT_COUNT * sizeof(int32_t) + STORE_CRC_SIZE)

LOG_MODULE_REGISTER(dsp_parameter_controller, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(STORE_SIZE == 48U, "Persistent DSP parameter record changed unexpectedly");

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
static atomic_t settings_loaded;

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

static void encode_store(
    uint8_t blob[STORE_SIZE], const int32_t stored_values[DSP_PARAMETER_PERSISTENT_COUNT], uint32_t revision)
{
  sys_put_le32(STORE_MAGIC, &blob[0]);
  sys_put_le16(STORE_VERSION, &blob[4]);
  sys_put_le16(STORE_SIZE, &blob[6]);
  sys_put_le32(DSP_PARAMETER_CONTRACT_ID, &blob[8]);
  sys_put_le32(revision, &blob[12]);
  sys_put_le16(DSP_PARAMETER_PERSISTENT_COUNT, &blob[16]);
  sys_put_le16(0U, &blob[18]);

  for (size_t index = 0; index < DSP_PARAMETER_PERSISTENT_COUNT; index++) {
    sys_put_le32((uint32_t)stored_values[index], &blob[STORE_HEADER_SIZE + index * sizeof(int32_t)]);
  }

  sys_put_le32(crc32_ieee(blob, STORE_SIZE - STORE_CRC_SIZE), &blob[STORE_SIZE - STORE_CRC_SIZE]);
}

static int decode_store(const uint8_t blob[STORE_SIZE])
{
  uint32_t expected_crc = sys_get_le32(&blob[STORE_SIZE - STORE_CRC_SIZE]);

  if (sys_get_le32(&blob[0]) != STORE_MAGIC || sys_get_le16(&blob[4]) != STORE_VERSION
      || sys_get_le16(&blob[6]) != STORE_SIZE || sys_get_le32(&blob[8]) != DSP_PARAMETER_CONTRACT_ID
      || sys_get_le16(&blob[16]) != DSP_PARAMETER_PERSISTENT_COUNT
      || crc32_ieee(blob, STORE_SIZE - STORE_CRC_SIZE) != expected_crc) {
    return -EINVAL;
  }

  for (size_t index = 0; index < DSP_PARAMETER_PERSISTENT_COUNT; index++) {
    const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(persistent_ids[index]);
    int32_t value = (int32_t)sys_get_le32(&blob[STORE_HEADER_SIZE + index * sizeof(int32_t)]);
    int ret = dsp_parameter_validate(parameter, 0U, value);

    if (ret != 0) {
      return ret;
    }
    values[index] = value;
  }

  atomic_set(&current_revision, (atomic_val_t)sys_get_le32(&blob[12]));
  return 0;
}

static int parameter_settings_set(const char* name, size_t len, settings_read_cb read_cb, void* cb_arg)
{
  uint8_t blob[STORE_SIZE];
  ssize_t bytes_read;
  int ret;

  if (strcmp(name, "parameters") != 0) {
    return -ENOENT;
  }

  if (len != sizeof(blob)) {
    LOG_WRN("Ignoring persistent DSP record with length %zu", len);
    return 0;
  }

  bytes_read = read_cb(cb_arg, blob, sizeof(blob));
  if (bytes_read < 0) {
    return (int)bytes_read;
  }
  if (bytes_read != sizeof(blob)) {
    return -EIO;
  }

  ret = decode_store(blob);
  if (ret != 0) {
    LOG_WRN("Ignoring invalid persistent DSP record: %d", ret);
    load_defaults();
  }

  return 0;
}

static int parameter_settings_commit(void)
{
  atomic_set(&settings_loaded, 1);
  return 0;
}

static struct settings_handler parameter_settings = {
  .name = "tiresias",
  .h_set = parameter_settings_set,
  .h_commit = parameter_settings_commit,
};

int dsp_parameter_controller_init(void)
{
  int ret;

  if (initialized) {
    return 0;
  }

  load_defaults();
  ret = settings_register(&parameter_settings);
  if (ret != 0) {
    return ret;
  }

  ret = settings_load_subtree(parameter_settings.name);
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
  int32_t pending_values[DSP_PARAMETER_PERSISTENT_COUNT];
  uint8_t blob[STORE_SIZE];
  size_t index;
  uint32_t pending_revision;
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

  memcpy(pending_values, values, sizeof(pending_values));
  pending_values[index] = value;
  pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  encode_store(blob, pending_values, pending_revision);

  ret = settings_save_one(SETTINGS_KEY, blob, sizeof(blob));
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

  memcpy(values, pending_values, sizeof(values));
  atomic_set(&current_revision, (atomic_val_t)pending_revision);
  *revision = pending_revision;
  k_mutex_unlock(&parameter_mutex);

  return 0;
}

uint32_t dsp_parameter_controller_revision(void)
{
  return (uint32_t)atomic_get(&current_revision);
}

bool dsp_parameter_controller_loaded(void)
{
  return atomic_get(&settings_loaded) != 0;
}
