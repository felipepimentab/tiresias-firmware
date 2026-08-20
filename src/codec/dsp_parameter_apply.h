/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DSP_PARAMETER_APPLY_H
#define DSP_PARAMETER_APPLY_H

#include "dsp_parameter_catalog.h"

int dsp_parameter_apply(const struct dsp_parameter_descriptor* parameter, int32_t value);

#endif /* DSP_PARAMETER_APPLY_H */
