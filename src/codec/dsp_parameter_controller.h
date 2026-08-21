/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DSP_PARAMETER_CONTROLLER_H
#define DSP_PARAMETER_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

int dsp_parameter_controller_init(void);
int dsp_parameter_controller_get(uint8_t id, uint8_t word_index, int32_t* value, uint32_t* revision);
int dsp_parameter_controller_set(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision);
uint32_t dsp_parameter_controller_revision(void);
bool dsp_parameter_controller_loaded(void);

#endif /* DSP_PARAMETER_CONTROLLER_H */
