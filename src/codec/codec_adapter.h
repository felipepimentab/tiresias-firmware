/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef CODEC_ADAPTER_H
#define CODEC_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Read bytes from live codec parameter memory.
 *
 * @return 0 on success, otherwise a negative errno-style value.
 */
int codec_param_read(uint16_t start_addr, uint8_t* data, size_t len);

/**
 * @brief Write bytes to live codec parameter memory.
 *
 * Values of up to five complete parameter words use SigmaDSP safeload.
 * Larger values are written directly while the SigmaDSP is stopped.
 *
 * @param start_addr Four-byte-aligned external parameter RAM address.
 * @param data Complete parameter words in external control-port byte order.
 * @param len Nonzero number of bytes; must be a multiple of four and fit in
 * parameter RAM.
 *
 * @return 0 on success, otherwise a negative errno-style value.
 */
int codec_param_write(uint16_t start_addr, const uint8_t* data, size_t len);

/**
 * @brief Initialize the ADAU1787 through its driver.
 *
 * @return 0 on success, otherwise a negative errno-style value.
 */
int codec_adapter_init(void);

/**
 * @brief Select the local microphone and DSP listening path.
 *
 * @return 0 if successful, error otherwise
 */
int codec_adapter_select_local(void);

/**
 * @brief Select the I2S listening path.
 *
 * @return 0 if successful, error otherwise
 */
int codec_adapter_select_i2s(void);

#endif /* CODEC_ADAPTER_H */
