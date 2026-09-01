/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Access to parameter defaults from the SigmaStudio image.
 */

#ifndef CODEC_DEFAULTS_H
#define CODEC_DEFAULTS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Copy one parameter's SigmaStudio default value.
 *
 * Resolves the parameter ID to its generated DSP address, validates that the
 * requested value fits within the exported SigmaStudio parameter image, and
 * copies it into @p destination.
 *
 * @param id Stable parameter ID from the codec contract.
 * @param[out] destination Buffer that receives the complete default value.
 * @param size Size of @p destination; must match the contract byte count.
 *
 * @retval 0 The default value was copied.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EINVAL A pointer or size is invalid.
 * @retval -ERANGE The generated value lies outside the parameter image.
 */
int codec_defaults_copy(uint8_t id, uint8_t* destination, size_t size);

#endif /* CODEC_DEFAULTS_H */
