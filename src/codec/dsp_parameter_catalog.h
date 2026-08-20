/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DSP_PARAMETER_CATALOG_H
#define DSP_PARAMETER_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#define DSP_PARAMETER_LAYOUT_ID 0x54525031U
#define DSP_PARAMETER_CATALOG_COUNT 4U

enum dsp_parameter_flags {
  DSP_PARAMETER_FLAG_READABLE = 1U << 0,
  DSP_PARAMETER_FLAG_WRITABLE = 1U << 1,
  DSP_PARAMETER_FLAG_PERSISTENT = 1U << 2,
  DSP_PARAMETER_FLAG_LIVE_SAFE = 1U << 3,
};

enum dsp_parameter_encoding {
  DSP_PARAMETER_ENCODING_Q5_23 = 1,
};

enum dsp_parameter_unit {
  DSP_PARAMETER_UNIT_LINEAR = 1,
};

struct dsp_parameter_descriptor {
  uint16_t id;
  uint8_t flags;
  uint8_t encoding;
  uint16_t dsp_address;
  uint8_t word_count;
  uint8_t unit;
  int32_t minimum;
  int32_t maximum;
  int32_t default_value;
  int32_t step;
  char name[8];
};

const struct dsp_parameter_descriptor* dsp_parameter_catalog(void);
size_t dsp_parameter_catalog_count(void);
const struct dsp_parameter_descriptor* dsp_parameter_find(uint16_t id);
int dsp_parameter_validate(const struct dsp_parameter_descriptor* parameter, int32_t value);

#endif /* DSP_PARAMETER_CATALOG_H */
