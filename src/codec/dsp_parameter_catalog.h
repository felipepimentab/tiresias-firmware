/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DSP_PARAMETER_CATALOG_H
#define DSP_PARAMETER_CATALOG_H

#include <stdint.h>

#define DSP_PARAMETER_CONTRACT_ID 0x54525001U
#define DSP_PARAMETER_CONTRACT_VERSION 1U
#define DSP_PARAMETER_CONTRACT_CRC32 0xF62C1808U
#define DSP_BLOCK_CONTRACT_COUNT 15U
#define DSP_PARAMETER_CONTRACT_COUNT 15U
#define DSP_PARAMETER_PERSISTENT_COUNT 6U

/*
 * Fixed MVP parameter contract. Contract membership implies that a parameter
 * is readable. Parameters are read-only and Q5.23 unless the corresponding
 * capability or type flag is set. Device-provided dynamic catalogs are a
 * post-MVP extension.
 *
 * Keep IDs and metadata synchronized with
 * tiresias_workstation.domain.dsp_contract.
 */
enum dsp_parameter_contract_flag {
  DSP_PARAMETER_CONTRACT_FLAG_WRITABLE = 1U << 0,
  DSP_PARAMETER_CONTRACT_FLAG_INTEGER = 1U << 1,
};

enum dsp_block_id {
  DSP_BLOCK_ID_ADC_SELECT = 1,
  DSP_BLOCK_ID_SOURCE_SELECT = 2,
  DSP_BLOCK_ID_BAND_1_COMPRESSOR = 3,
  DSP_BLOCK_ID_BAND_2_COMPRESSOR = 4,
  DSP_BLOCK_ID_BAND_3_COMPRESSOR = 5,
  DSP_BLOCK_ID_BAND_4_COMPRESSOR = 6,
  DSP_BLOCK_ID_BAND_5_COMPRESSOR = 7,
  DSP_BLOCK_ID_BAND_6_COMPRESSOR = 8,
  DSP_BLOCK_ID_BAND_7_COMPRESSOR = 9,
  DSP_BLOCK_ID_BAND_8_COMPRESSOR = 10,
  DSP_BLOCK_ID_PHASE_COMP_GAIN_1 = 11,
  DSP_BLOCK_ID_PHASE_COMP_GAIN_2 = 12,
  DSP_BLOCK_ID_PHASE_COMP_GAIN_3 = 13,
  DSP_BLOCK_ID_OUTPUT_HEADROOM = 14,
  DSP_BLOCK_ID_SOFT_CLIP = 15,
};

enum dsp_parameter_id {
  DSP_PARAMETER_ID_ADC_SELECT = 1,
  DSP_PARAMETER_ID_SOURCE_SELECT = 2,
  DSP_PARAMETER_ID_BAND_1_COMPRESSOR_LUT = 3,
  DSP_PARAMETER_ID_BAND_2_COMPRESSOR_LUT = 4,
  DSP_PARAMETER_ID_BAND_3_COMPRESSOR_LUT = 5,
  DSP_PARAMETER_ID_BAND_4_COMPRESSOR_LUT = 6,
  DSP_PARAMETER_ID_BAND_5_COMPRESSOR_LUT = 7,
  DSP_PARAMETER_ID_BAND_6_COMPRESSOR_LUT = 8,
  DSP_PARAMETER_ID_BAND_7_COMPRESSOR_LUT = 9,
  DSP_PARAMETER_ID_BAND_8_COMPRESSOR_LUT = 10,
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_1 = 11,
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_2 = 12,
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_3 = 13,
  DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN = 14,
  DSP_PARAMETER_ID_SOFT_CLIP_LUT = 15,
};

struct dsp_parameter {
  uint8_t id;
  uint8_t block_id;
  uint8_t word_count;
  uint8_t flags;
};

struct dsp_parameter_descriptor {
  const struct dsp_parameter* definition;
  uint16_t dsp_address;
  int32_t minimum;
  int32_t maximum;
  int32_t default_value;
  int32_t step;
};

extern const struct dsp_parameter dsp_parameter_contract[DSP_PARAMETER_CONTRACT_COUNT];

const struct dsp_parameter_descriptor* dsp_parameter_find(uint8_t id);
int dsp_parameter_validate(const struct dsp_parameter_descriptor* parameter, uint8_t word_index, int32_t value);

#endif /* DSP_PARAMETER_CATALOG_H */
