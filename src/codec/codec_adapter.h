/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef CODEC_ADAPTER_H
#define CODEC_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#define CODEC_PARAMETER_WORD_SIZE 4U

int codec_param_read(uint16_t start_addr, uint8_t* data, size_t len);
int codec_param_write(uint16_t start_addr, const uint8_t* data, size_t len);

#endif /* CODEC_ADAPTER_H */
