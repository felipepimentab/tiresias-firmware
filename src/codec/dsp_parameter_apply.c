/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_apply.h"

#include <zephyr/sys/util.h>

int dsp_parameter_apply(const struct dsp_parameter_descriptor* parameter, int32_t value)
{
  /* The MVP persists and reports values without touching unavailable DSP hardware. */
  ARG_UNUSED(parameter);
  ARG_UNUSED(value);

  return 0;
}
