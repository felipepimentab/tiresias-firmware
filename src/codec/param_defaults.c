/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "param_defaults.h"

#include "adau1787.h"

/* Defined by the generated SigmaStudio image included by adau1787.c. */
extern uint8_t Param_Data_IC_1_Sigma[];

const uint8_t* dsp_parameter_default(uint8_t id)
{
  if (id == 0U || id > DSP_PARAMETER_COUNT) {
    return NULL;
  }

  return &Param_Data_IC_1_Sigma[dsp_parameter_addresses[id] - ADAU1787_PARAM_RAM_BASE];
}
