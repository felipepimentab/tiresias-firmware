/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Private mapping between codec parameter IDs and DSP addresses.
 */

#ifndef CODEC_PROGRAM_H
#define CODEC_PROGRAM_H

#include "adau1787.h"

#include <stdint.h>

/**
 * @brief Resolve a codec parameter ID to its SigmaStudio start address.
 *
 * @param id Stable parameter ID from the codec contract.
 * @param[out] address Resolved byte address in ADAU1787 parameter memory.
 *
 * @retval 0 The address was resolved.
 * @retval -ENOENT The parameter ID is not in the codec contract.
 * @retval -EINVAL @p address is NULL.
 */
int get_param_address(uint8_t id, sub_addr_t* address);

#endif /* CODEC_PROGRAM_H */
