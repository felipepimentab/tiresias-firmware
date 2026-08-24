/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_settings.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

#define SETTINGS_SUBTREE "tiresias"
#define SETTINGS_NAME "parameters"
#define SETTINGS_KEY SETTINGS_SUBTREE "/" SETTINGS_NAME
#define STORE_MAGIC 0x54525053U
#define STORE_VERSION 2U
#define STORE_HEADER_SIZE 20U
#define STORE_CRC_SIZE 4U
#define STORE_SIZE (STORE_HEADER_SIZE + DSP_PARAMETER_PERSISTENT_COUNT * sizeof(int32_t) + STORE_CRC_SIZE)

LOG_MODULE_REGISTER(dsp_parameter_settings, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(STORE_SIZE == 48U, "Persistent DSP parameter record changed unexpectedly");

/**
 * Persistent record layout, with every multibyte field encoded little-endian:
 *
 * | Offset | Size | Field                                      |
 * |-------:|-----:|--------------------------------------------|
 * |      0 |    4 | Record magic                               |
 * |      4 |    2 | Storage format version                     |
 * |      6 |    2 | Total record size                          |
 * |      8 |    4 | Fixed DSP parameter contract ID            |
 * |     12 |    4 | Parameter revision                         |
 * |     16 |    2 | Persistent value count                     |
 * |     18 |    2 | Reserved; must be zero                     |
 * |     20 |   24 | Six signed 32-bit parameter values         |
 * |     44 |    4 | CRC-32 over every preceding record byte    |
 *
 * The storage format version is independent of the public BLE contract version.
 * The contract ID prevents a structurally valid record from being loaded by a
 * firmware image whose fixed parameter identity has changed.
 */

static struct dsp_parameter_settings_snapshot loaded_snapshot;
static int load_result = -ENOENT;
static bool settings_registered;

static void encode_store(uint8_t blob[STORE_SIZE], const struct dsp_parameter_settings_snapshot* snapshot)
{
  sys_put_le32(STORE_MAGIC, &blob[0]);
  sys_put_le16(STORE_VERSION, &blob[4]);
  sys_put_le16(STORE_SIZE, &blob[6]);
  sys_put_le32(DSP_PARAMETER_CONTRACT_ID, &blob[8]);
  sys_put_le32(snapshot->revision, &blob[12]);
  sys_put_le16(DSP_PARAMETER_PERSISTENT_COUNT, &blob[16]);
  sys_put_le16(0U, &blob[18]);

  for (size_t index = 0; index < DSP_PARAMETER_PERSISTENT_COUNT; index++) {
    sys_put_le32((uint32_t)snapshot->values[index], &blob[STORE_HEADER_SIZE + index * sizeof(int32_t)]);
  }

  sys_put_le32(crc32_ieee(blob, STORE_SIZE - STORE_CRC_SIZE), &blob[STORE_SIZE - STORE_CRC_SIZE]);
}

static int decode_store(const uint8_t blob[STORE_SIZE], struct dsp_parameter_settings_snapshot* snapshot)
{
  uint32_t expected_crc = sys_get_le32(&blob[STORE_SIZE - STORE_CRC_SIZE]);

  if (sys_get_le32(&blob[0]) != STORE_MAGIC || sys_get_le16(&blob[4]) != STORE_VERSION
      || sys_get_le16(&blob[6]) != STORE_SIZE || sys_get_le32(&blob[8]) != DSP_PARAMETER_CONTRACT_ID
      || sys_get_le16(&blob[16]) != DSP_PARAMETER_PERSISTENT_COUNT || sys_get_le16(&blob[18]) != 0U
      || crc32_ieee(blob, STORE_SIZE - STORE_CRC_SIZE) != expected_crc) {
    return -EINVAL;
  }

  snapshot->revision = sys_get_le32(&blob[12]);
  for (size_t index = 0; index < DSP_PARAMETER_PERSISTENT_COUNT; index++) {
    snapshot->values[index] = (int32_t)sys_get_le32(&blob[STORE_HEADER_SIZE + index * sizeof(int32_t)]);
  }

  return 0;
}

static int parameter_settings_set(const char* name, size_t len, settings_read_cb read_cb, void* cb_arg)
{
  uint8_t blob[STORE_SIZE];
  ssize_t bytes_read;

  if (strcmp(name, SETTINGS_NAME) != 0) {
    return -ENOENT;
  }
  if (len != sizeof(blob)) {
    LOG_WRN("Ignoring persistent DSP record with length %zu", len);
    load_result = -EINVAL;
    return 0;
  }

  bytes_read = read_cb(cb_arg, blob, sizeof(blob));
  if (bytes_read < 0) {
    load_result = (int)bytes_read;
    return 0;
  }
  if (bytes_read != sizeof(blob)) {
    load_result = -EIO;
    return 0;
  }

  load_result = decode_store(blob, &loaded_snapshot);
  if (load_result != 0) {
    LOG_WRN("Ignoring invalid persistent DSP record: %d", load_result);
  }

  return 0;
}

static struct settings_handler parameter_settings = {
  .name = SETTINGS_SUBTREE,
  .h_set = parameter_settings_set,
};

int dsp_parameter_settings_load(struct dsp_parameter_settings_snapshot* snapshot)
{
  int ret;

  if (snapshot == NULL) {
    return -EINVAL;
  }

  if (!settings_registered) {
    ret = settings_register(&parameter_settings);
    if (ret != 0) {
      return ret;
    }
    settings_registered = true;
  }

  load_result = -ENOENT;
  ret = settings_load_subtree(parameter_settings.name);
  if (ret != 0) {
    return ret;
  }
  if (load_result != 0) {
    return load_result;
  }

  *snapshot = loaded_snapshot;
  return 0;
}

int dsp_parameter_settings_save(const struct dsp_parameter_settings_snapshot* snapshot)
{
  uint8_t blob[STORE_SIZE];

  if (snapshot == NULL) {
    return -EINVAL;
  }

  encode_store(blob, snapshot);
  return settings_save_one(SETTINGS_KEY, blob, sizeof(blob));
}
