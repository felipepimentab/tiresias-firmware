/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Zephyr Settings adapter for individual DSP parameters.
 */

#ifndef DSP_PARAMETER_SETTINGS_H
#define DSP_PARAMETER_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

/** Initialize the Zephyr Settings backend. */
int dsp_parameter_settings_init(void);

/** Load one parameter from its flash key, leaving @p data unchanged when missing. */
int dsp_parameter_settings_load(uint8_t id, uint8_t* data, size_t size);

/** Save one complete parameter to its flash key. */
int dsp_parameter_settings_save(uint8_t id, const uint8_t* data, size_t size);

#endif /* DSP_PARAMETER_SETTINGS_H */
