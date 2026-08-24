/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DSP_PARAMETER_SETTINGS_H
#define DSP_PARAMETER_SETTINGS_H

#include "dsp_parameter_catalog.h"

#include <stdint.h>

struct dsp_parameter_settings_snapshot {
  uint32_t revision;
  int32_t values[DSP_PARAMETER_PERSISTENT_COUNT];
};

/* Load and save the complete persistent parameter snapshot. */
int dsp_parameter_settings_load(struct dsp_parameter_settings_snapshot* snapshot);
int dsp_parameter_settings_save(const struct dsp_parameter_settings_snapshot* snapshot);

#endif /* DSP_PARAMETER_SETTINGS_H */
