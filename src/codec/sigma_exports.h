/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SIGMA_EXPORTS_H
#define SIGMA_EXPORTS_H

#include <stddef.h>
#include <stdint.h>

#include "SigmaStudioFW.h"
#include "adau_1787_IC_1_FAST_PARAM.h"
#include "adau_1787_IC_1_FAST_REG.h"
#include "adau_1787_IC_1_SIGMA_PARAM.h"
#include "adau_1787_IC_1_SIGMA_REG.h"

/*
 * The generated PARAM and REG headers contain macros and may be included here.
 * The definition-bearing FAST and SIGMA headers are included only by
 * sigma_exports.c.
 */

extern ADI_REG_TYPE Program_Data_IC_1_Sigma[];
extern ADI_REG_TYPE Param_Data_IC_1_Sigma[];

void default_download_IC_1_Fast(void);
void default_download_IC_1_Sigma(void);

extern const uint16_t sigma_parameter_address;
extern const size_t sigma_parameter_size;
extern const uint16_t sigma_program_address;
extern const size_t sigma_program_size;

#endif /* SIGMA_EXPORTS_H */
