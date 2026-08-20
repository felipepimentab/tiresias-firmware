/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dsp_parameter_catalog.h"

#include "adau_1787_IC_1_SIGMA_PARAM.h"

#include <errno.h>
#include <zephyr/sys/util.h>

#define Q5_23_ONE 0x00800000
#define Q5_23_FOUR 0x02000000
#define Q5_23_STEP_1_256 0x00008000
#define PARAMETER_ACCESS                                                                                               \
  (DSP_PARAMETER_FLAG_READABLE | DSP_PARAMETER_FLAG_WRITABLE | DSP_PARAMETER_FLAG_PERSISTENT                           \
      | DSP_PARAMETER_FLAG_LIVE_SAFE)

static const struct dsp_parameter_descriptor parameters[] = {
  {
      .id = 1,
      .flags = PARAMETER_ACCESS,
      .encoding = DSP_PARAMETER_ENCODING_Q5_23,
      .dsp_address = MOD_PHASE_COMP_GAIN1_GAIN1940ALGNS1_ADDR,
      .word_count = 1,
      .unit = DSP_PARAMETER_UNIT_LINEAR,
      .minimum = 0,
      .maximum = Q5_23_FOUR,
      .default_value = Q5_23_ONE,
      .step = Q5_23_STEP_1_256,
      .name = "gain_1",
  },
  {
      .id = 2,
      .flags = PARAMETER_ACCESS,
      .encoding = DSP_PARAMETER_ENCODING_Q5_23,
      .dsp_address = MOD_PHASE_COMP_GAIN2_GAIN1940ALGNS2_ADDR,
      .word_count = 1,
      .unit = DSP_PARAMETER_UNIT_LINEAR,
      .minimum = 0,
      .maximum = Q5_23_FOUR,
      .default_value = Q5_23_ONE,
      .step = Q5_23_STEP_1_256,
      .name = "gain_2",
  },
  {
      .id = 3,
      .flags = PARAMETER_ACCESS,
      .encoding = DSP_PARAMETER_ENCODING_Q5_23,
      .dsp_address = MOD_PHASE_COMP_GAIN3_GAIN1940ALGNS3_ADDR,
      .word_count = 1,
      .unit = DSP_PARAMETER_UNIT_LINEAR,
      .minimum = 0,
      .maximum = Q5_23_FOUR,
      .default_value = Q5_23_ONE,
      .step = Q5_23_STEP_1_256,
      .name = "gain_3",
  },
  {
      .id = 4,
      .flags = PARAMETER_ACCESS,
      .encoding = DSP_PARAMETER_ENCODING_Q5_23,
      .dsp_address = MOD_OUTPUTHEADROOM_GAIN1940ALGNS4_ADDR,
      .word_count = 1,
      .unit = DSP_PARAMETER_UNIT_LINEAR,
      .minimum = 0,
      .maximum = Q5_23_FOUR,
      .default_value = Q5_23_ONE,
      .step = Q5_23_STEP_1_256,
      .name = "headroom",
  },
};

BUILD_ASSERT(ARRAY_SIZE(parameters) == DSP_PARAMETER_CATALOG_COUNT, "Catalog count must match the wire contract");
BUILD_ASSERT(MOD_PHASE_COMP_GAIN1_COUNT == 1 && MOD_PHASE_COMP_GAIN2_COUNT == 1 && MOD_PHASE_COMP_GAIN3_COUNT == 1
        && MOD_OUTPUTHEADROOM_COUNT == 1,
    "MVP parameters must be single DSP words");
BUILD_ASSERT(MOD_PHASE_COMP_GAIN1_GAIN1940ALGNS1_ADDR % 4 == 0 && MOD_PHASE_COMP_GAIN2_GAIN1940ALGNS2_ADDR % 4 == 0
        && MOD_PHASE_COMP_GAIN3_GAIN1940ALGNS3_ADDR % 4 == 0 && MOD_OUTPUTHEADROOM_GAIN1940ALGNS4_ADDR % 4 == 0,
    "MVP DSP addresses must be word aligned");

const struct dsp_parameter_descriptor* dsp_parameter_catalog(void)
{
  return parameters;
}

size_t dsp_parameter_catalog_count(void)
{
  return ARRAY_SIZE(parameters);
}

const struct dsp_parameter_descriptor* dsp_parameter_find(uint16_t id)
{
  for (size_t index = 0; index < ARRAY_SIZE(parameters); index++) {
    if (parameters[index].id == id) {
      return &parameters[index];
    }
  }

  return NULL;
}

int dsp_parameter_validate(const struct dsp_parameter_descriptor* parameter, int32_t value)
{
  if (parameter == NULL) {
    return -ENOENT;
  }

  if ((parameter->flags & DSP_PARAMETER_FLAG_WRITABLE) == 0U) {
    return -EACCES;
  }

  if (value < parameter->minimum || value > parameter->maximum) {
    return -ERANGE;
  }

  if (parameter->step > 0 && (value - parameter->minimum) % parameter->step != 0) {
    return -ERANGE;
  }

  return 0;
}
