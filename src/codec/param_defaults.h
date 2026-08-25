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
 * @brief Get a zero-copy view of a catalog parameter's SigmaStudio default.
 *
 * The returned pointer addresses the parameter's first byte in the generated
 * `Param_Data_IC_1_Sigma` array. Its byte length is the matching catalog
 * entry's word count times the four-byte codec parameter-word size.
 *
 * @param id Stable parameter ID from the fixed contract.
 *
 * @return Pointer to the parameter's generated default bytes, or NULL when
 * @p id is outside the catalog.
 */
const uint8_t* dsp_parameter_default(uint8_t id);

#endif /* PARAM_DEFAULTS_H */
