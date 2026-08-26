/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_settings.h"

#include <stdio.h>
#include <zephyr/settings/settings.h>

#define SETTINGS_SUBTREE "tiresias/parameters"
#define SETTINGS_KEY_SIZE sizeof(SETTINGS_SUBTREE "/255")

static void parameter_key(uint8_t id, char key[SETTINGS_KEY_SIZE])
{
  (void)snprintf(key, SETTINGS_KEY_SIZE, SETTINGS_SUBTREE "/%u", id);
}

int dsp_parameter_settings_init(void)
{
  return settings_subsys_init();
}

int dsp_parameter_settings_load(uint8_t id, uint8_t* data, size_t size)
{
  char key[SETTINGS_KEY_SIZE];
  ssize_t bytes_read;

  parameter_key(id, key);
  bytes_read = settings_load_one(key, data, size);
  if (bytes_read < 0) {
    return (int)bytes_read;
  }

  /* TODO: Reject malformed stored lengths when persistence recovery is added. */
  return 0;
}

int dsp_parameter_settings_save(uint8_t id, const uint8_t* data, size_t size)
{
  char key[SETTINGS_KEY_SIZE];

  parameter_key(id, key);
  return settings_save_one(key, data, size);
}
