/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_controller.h"

#include "dsp_parameter_apply.h"
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
#define STORE_VERSION 1U
#define STORE_HEADER_SIZE 20U
#define STORE_CRC_SIZE 4U
#define STORE_SIZE (STORE_HEADER_SIZE + DSP_PARAMETER_CATALOG_COUNT * sizeof(int32_t) + STORE_CRC_SIZE)

LOG_MODULE_REGISTER(dsp_parameter_controller, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(STORE_SIZE == 40U, "Persistent DSP parameter record changed unexpectedly");

static K_MUTEX_DEFINE(parameter_mutex);
static int32_t values[DSP_PARAMETER_CATALOG_COUNT];
static atomic_t current_revision;
static bool initialized;
static atomic_t settings_loaded;

static void load_defaults(void)
{
  const struct dsp_parameter_descriptor* catalog = dsp_parameter_catalog();

  for (size_t index = 0; index < dsp_parameter_catalog_count(); index++) {
    values[index] = catalog[index].default_value;
  }

  atomic_clear(&current_revision);
}

static void encode_store(
    uint8_t blob[STORE_SIZE], const int32_t stored_values[DSP_PARAMETER_CATALOG_COUNT], uint32_t revision)
{
  sys_put_le32(STORE_MAGIC, &blob[0]);
  sys_put_le16(STORE_VERSION, &blob[4]);
  sys_put_le16(STORE_SIZE, &blob[6]);
  sys_put_le32(DSP_PARAMETER_LAYOUT_ID, &blob[8]);
  sys_put_le32(revision, &blob[12]);
  sys_put_le16(DSP_PARAMETER_CATALOG_COUNT, &blob[16]);
  sys_put_le16(0U, &blob[18]);

  for (size_t index = 0; index < DSP_PARAMETER_CATALOG_COUNT; index++) {
    sys_put_le32((uint32_t)stored_values[index], &blob[STORE_HEADER_SIZE + index * sizeof(int32_t)]);
  }

  sys_put_le32(crc32_ieee(blob, STORE_SIZE - STORE_CRC_SIZE), &blob[STORE_SIZE - STORE_CRC_SIZE]);
}

static int decode_store(const uint8_t blob[STORE_SIZE])
{
  const struct dsp_parameter_descriptor* catalog = dsp_parameter_catalog();
  uint32_t expected_crc = sys_get_le32(&blob[STORE_SIZE - STORE_CRC_SIZE]);

  if (sys_get_le32(&blob[0]) != STORE_MAGIC || sys_get_le16(&blob[4]) != STORE_VERSION
      || sys_get_le16(&blob[6]) != STORE_SIZE || sys_get_le32(&blob[8]) != DSP_PARAMETER_LAYOUT_ID
      || sys_get_le16(&blob[16]) != DSP_PARAMETER_CATALOG_COUNT
      || crc32_ieee(blob, STORE_SIZE - STORE_CRC_SIZE) != expected_crc) {
    return -EINVAL;
  }

  for (size_t index = 0; index < DSP_PARAMETER_CATALOG_COUNT; index++) {
    int32_t value = (int32_t)sys_get_le32(&blob[STORE_HEADER_SIZE + index * sizeof(int32_t)]);
    int ret = dsp_parameter_validate(&catalog[index], value);

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

  /*
   * Load this module's state explicitly so readiness does not depend on which
   * Bluetooth subsystem happens to perform the process-wide settings load
   * first. An empty subtree is a valid first boot and commits the defaults.
   */
  ret = settings_load_subtree(parameter_settings.name);
  if (ret != 0) {
    return ret;
  }

  initialized = true;
  return 0;
}

int dsp_parameter_controller_get(uint16_t id, int32_t* value, uint32_t* revision)
{
  const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(id);

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (value == NULL || revision == NULL) {
    return -EINVAL;
  }

  k_mutex_lock(&parameter_mutex, K_FOREVER);
  *value = values[parameter - dsp_parameter_catalog()];
  *revision = (uint32_t)atomic_get(&current_revision);
  k_mutex_unlock(&parameter_mutex);

  return 0;
}

int dsp_parameter_controller_set(uint16_t id, int32_t value, uint32_t* revision)
{
  const struct dsp_parameter_descriptor* parameter = dsp_parameter_find(id);
  int32_t pending_values[DSP_PARAMETER_CATALOG_COUNT];
  uint8_t blob[STORE_SIZE];
  size_t index;
  uint32_t pending_revision;
  int ret;

  if (revision == NULL) {
    return -EINVAL;
  }

  ret = dsp_parameter_validate(parameter, value);
  if (ret != 0) {
    return ret;
  }

  index = parameter - dsp_parameter_catalog();
  k_mutex_lock(&parameter_mutex, K_FOREVER);
  memcpy(pending_values, values, sizeof(pending_values));
  pending_values[index] = value;
  pending_revision = (uint32_t)atomic_get(&current_revision) + 1U;
  encode_store(blob, pending_values, pending_revision);

  ret = settings_save_one(SETTINGS_KEY, blob, sizeof(blob));
  if (ret == 0) {
    memcpy(values, pending_values, sizeof(values));
    atomic_set(&current_revision, (atomic_val_t)pending_revision);
    *revision = pending_revision;
  }
  k_mutex_unlock(&parameter_mutex);

  if (ret != 0) {
    LOG_ERR("Failed to persist DSP parameter %u: %d", id, ret);
    return ret;
  }

  ret = dsp_parameter_apply(parameter, value);
  if (ret != 0) {
    LOG_ERR("Failed to apply persisted DSP parameter %u: %d", id, ret);
  }

  return ret;
}

uint32_t dsp_parameter_controller_revision(void)
{
  return (uint32_t)atomic_get(&current_revision);
}

bool dsp_parameter_controller_loaded(void)
{
  return atomic_get(&settings_loaded) != 0;
}
