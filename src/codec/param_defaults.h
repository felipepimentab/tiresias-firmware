/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Immutable views of catalog defaults in the SigmaStudio parameter image.
 */

#ifndef PARAM_DEFAULTS_H
#define PARAM_DEFAULTS_H

#include "dsp_parameter_catalog.h"

#include <stdint.h>

/**
 * @brief Parameter-default byte arrays indexed directly by parameter ID.
 *
 * Index zero is unused. Each remaining entry points into the generated
 * `Param_Data_IC_1_Sigma` byte array at the first word of the corresponding
 * catalog parameter. The byte length is the catalog entry's word count times
 * the four-byte codec parameter-word size.
 */
extern const uint8_t* const dsp_parameter_defaults[DSP_PARAMETER_COUNT + 1U];

#endif /* PARAM_DEFAULTS_H */
