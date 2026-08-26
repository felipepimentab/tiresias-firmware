/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Zephyr Settings adapter for individual DSP parameters.
 *
 * The adapter maps a stable parameter ID to the key
 * `tiresias/parameters/<id>` and delegates storage to Zephyr Settings. Each key
 * contains one complete opaque parameter value. The module does not know the
 * DSP catalog, own RAM values, interpret bytes, manage revisions, or access
 * codec hardware.
 *
 * The parameter controller is the only intended caller and is responsible for
 * supplying a valid ID, buffer, and catalog byte count. The current
 * proof-of-concept intentionally defers catalog validation and stored-length
 * recovery to keep this adapter small.
 */

#ifndef DSP_PARAMETER_SETTINGS_H
#define DSP_PARAMETER_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the Zephyr Settings subsystem and configured backend.
 *
 * Zephyr treats subsystem initialization as idempotent, so this function may be
 * called when another subsystem has already initialized Settings.
 *
 * @retval 0 The Settings subsystem is ready.
 * @return A negative errno-style value from Zephyr Settings initialization.
 */
int dsp_parameter_settings_init(void);

/**
 * @brief Load one parameter value from flash.
 *
 * When the key does not exist, Zephyr returns zero bytes and @p data remains
 * unchanged. The proof-of-concept currently treats every nonnegative load result
 * as success; stored-length validation is deferred in the implementation.
 *
 * @param id Stable parameter ID used to form the settings key.
 * @param[out] data Destination buffer for the stored opaque bytes.
 * @param size Maximum number of bytes to copy into @p data.
 *
 * @retval 0 The stored value was read or the key does not exist.
 * @return A negative errno-style value from Zephyr Settings.
 */
int dsp_parameter_settings_load(uint8_t id, uint8_t* data, size_t size);

/**
 * @brief Save one complete parameter value to flash.
 *
 * @param id Stable parameter ID used to form the settings key.
 * @param data Complete opaque parameter value.
 * @param size Number of bytes to save.
 *
 * @retval 0 The value was accepted by the Settings backend.
 * @return A negative errno-style value from Zephyr Settings.
 */
int dsp_parameter_settings_save(uint8_t id, const uint8_t* data, size_t size);

#endif /* DSP_PARAMETER_SETTINGS_H */
