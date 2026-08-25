/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_settings.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#define SETTINGS_SUBTREE "tiresias/parameters"
#define SETTINGS_KEY_SIZE sizeof(SETTINGS_SUBTREE "/255")
LOG_MODULE_REGISTER(dsp_parameter_settings, CONFIG_LOG_DEFAULT_LEVEL);

static uint8_t* const* load_targets;
static uint32_t loaded_parameter_mask;
static int load_result;
static bool settings_registered;

BUILD_ASSERT(DSP_PARAMETER_COUNT <= 32U, "Loaded parameter mask must contain every parameter ID");

static int parameter_id_from_name(const char* name, uint8_t* id)
{
  char* end;
  unsigned long parsed_id;

  if (name == NULL || name[0] == '\0') {
    return -ENOENT;
  }

  errno = 0;
  parsed_id = strtoul(name, &end, 10);
  if (errno != 0 || end == name || *end != '\0' || parsed_id == 0U || parsed_id > DSP_PARAMETER_COUNT) {
    return -ENOENT;
  }

  *id = (uint8_t)parsed_id;
  return dsp_parameter_definition(*id) == NULL ? -ENOENT : 0;
}

static int parameter_settings_set(const char* name, size_t len, settings_read_cb read_cb, void* cb_arg)
{
  const struct dsp_parameter* parameter;
  ssize_t bytes_read;
  size_t expected_size;
  uint8_t id;

  if (parameter_id_from_name(name, &id) != 0 || load_targets == NULL) {
    return 0;
  }

  parameter = dsp_parameter_definition(id);
  expected_size = parameter->byte_count;
  if (load_targets[id] == NULL || len != expected_size) {
    LOG_WRN("Ignoring DSP parameter %u with length %zu; expected %zu", id, len, expected_size);
    return 0;
  }

  bytes_read = read_cb(cb_arg, load_targets[id], expected_size);
  if (bytes_read < 0) {
    if (load_result == 0) {
      load_result = (int)bytes_read;
    }
    return 0;
  }
  if (bytes_read != expected_size) {
    LOG_WRN("Incomplete DSP parameter %u: read %zd of %zu bytes", id, bytes_read, expected_size);
    if (load_result == 0) {
      load_result = -EIO;
    }
  } else {
    loaded_parameter_mask |= BIT(id - 1U);
  }

  return 0;
}

static struct settings_handler parameter_settings = {
  .name = SETTINGS_SUBTREE,
  .h_set = parameter_settings_set,
};

int dsp_parameter_settings_init(void)
{
  int ret;

  if (!settings_registered) {
    ret = settings_register(&parameter_settings);
    if (ret != 0) {
      return ret;
    }
    settings_registered = true;
  }

  return 0;
}

int dsp_parameter_settings_load(uint8_t* const parameter_data[DSP_PARAMETER_COUNT + 1U], bool save_missing_parameters)
{
  int ret;

  if (parameter_data == NULL) {
    return -EINVAL;
  }

  ret = dsp_parameter_settings_init();
  if (ret != 0) {
    return ret;
  }

  load_result = 0;
  loaded_parameter_mask = 0U;
  load_targets = parameter_data;
  ret = settings_load_subtree(parameter_settings.name);
  load_targets = NULL;
  if (ret != 0) {
    return ret;
  }
  if (load_result != 0) {
    return load_result;
  }
  if (!save_missing_parameters) {
    return 0;
  }

  for (size_t parameter_index = 0U; parameter_index < DSP_PARAMETER_COUNT; parameter_index++) {
    const struct dsp_parameter* parameter = &dsp_parameter_contract[parameter_index];

    if ((loaded_parameter_mask & BIT(parameter->id - 1U)) != 0U) {
      continue;
    }

    ret = dsp_parameter_settings_save(parameter->id, parameter_data[parameter->id], parameter->byte_count);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

int dsp_parameter_settings_save(uint8_t id, const uint8_t* data, size_t size)
{
  const struct dsp_parameter* parameter = dsp_parameter_definition(id);
  char key[SETTINGS_KEY_SIZE];
  int key_length;

  if (parameter == NULL) {
    return -ENOENT;
  }
  if (data == NULL || size != parameter->byte_count) {
    return -EINVAL;
  }

  key_length = snprintf(key, sizeof(key), SETTINGS_SUBTREE "/%u", id);
  if (key_length < 0 || (size_t)key_length >= sizeof(key)) {
    return -EINVAL;
  }

  return settings_save_one(key, data, size);
}
