/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sigma_exports.h"

#include "adau1787.h"
#include "adau_1787_IC_1_FAST.h"
#include "adau_1787_IC_1_SIGMA.h"

BUILD_ASSERT(PARAM_ADDR_IC_1_Sigma == ADAU1787_PARAM_RAM_BASE, "Param Memory Address must be set to 0x2000.");

const uint16_t sigma_parameter_address = PARAM_ADDR_IC_1_Sigma;
const size_t sigma_parameter_size = sizeof(Param_Data_IC_1_Sigma);
const uint16_t sigma_program_address = PROGRAM_ADDR_IC_1_Sigma;
const size_t sigma_program_size = sizeof(Program_Data_IC_1_Sigma);
