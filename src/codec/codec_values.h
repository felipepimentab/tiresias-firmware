/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Runtime storage for codec parameter values.
 *
 * This module owns one fixed byte buffer for every parameter in the codec
 * contract. It provides storage only; callers remain responsible for contract
 * validation, synchronization, persistence, revisions, and codec access.
 */

#ifndef CODEC_VALUES_H
#define CODEC_VALUES_H

#include <stdint.h>

/**
 * @brief Return the buffer for a codec parameter.
 *
 * @param id Stable parameter ID from the fixed contract.
 *
 * @return Pointer to the mutable parameter buffer, or NULL when @p id is not in
 * the contract.
 */
uint8_t* codec_values_get(uint8_t id);

/**
 * @brief Reset every parameter buffer to zero.
 */
void codec_values_reset(void);

#endif /* CODEC_VALUES_H */
