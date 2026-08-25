/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Flash persistence adapter for independent DSP parameters.
 *
 * Each parameter is stored under its own Zephyr Settings key as opaque bytes
 * in ADAU1787 control-port order. This module does not create a combined image,
 * interpret values, access the codec, or manage the runtime revision.
 */

#ifndef DSP_PARAMETER_SETTINGS_H
#define DSP_PARAMETER_SETTINGS_H

#include "dsp_parameter_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Register the module's Zephyr Settings handler.
 *
 * This must run before the application performs a global settings load. It is
 * idempotent and does not read flash by itself.
 *
 * @retval 0 The handler is registered or was already registered.
 * @return A negative errno-style value from Zephyr Settings registration.
 */
int dsp_parameter_settings_init(void);

/**
 * @brief Load stored parameters directly into their RAM byte arrays.
 *
 * Each existing `tiresias/parameters/<id>` entry is copied directly to the
 * array indexed by that parameter ID. Missing or malformed entries leave their
 * destination arrays unchanged. When @p save_missing_parameters is true, those
 * unchanged defaults are saved under their own independent keys. This operation
 * is intended for controller initialization and is not reentrant.
 *
 * @param[in,out] parameter_data Parameter arrays indexed by stable parameter ID.
 * @param[in] save_missing_parameters Whether to create settings for parameters
 * that were not loaded. Bluetooth-only builds pass false so their zero-filled
 * placeholders cannot replace SigmaStudio defaults after a firmware change.
 *
 * @retval 0 All readable settings entries were loaded.
 * @retval -EINVAL @p parameter_data is NULL.
 * @retval -EIO A settings entry could not be read completely.
 * @return Another negative errno-style value from Zephyr Settings.
 */
int dsp_parameter_settings_load(uint8_t* const parameter_data[DSP_PARAMETER_COUNT + 1U], bool save_missing_parameters);

/**
 * @brief Save one complete RAM parameter to its independent flash key.
 *
 * @param[in] id Stable parameter ID.
 * @param[in] data Complete parameter bytes in DSP control-port order.
 * @param[in] size Size of @p data; must match the catalog byte count.
 *
 * @retval 0 The settings backend accepted the parameter.
 * @retval -ENOENT @p id is not part of the fixed catalog.
 * @retval -EINVAL @p data is NULL or @p size does not match the catalog.
 * @return Another negative errno-style value from Zephyr Settings.
 */
int dsp_parameter_settings_save(uint8_t id, const uint8_t* data, size_t size);

#endif /* DSP_PARAMETER_SETTINGS_H */
