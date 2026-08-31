/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

// #include "adau1787.h"
#include "codec_values.h"
#include "codec_contract.h"

#include <stddef.h>
#include <string.h>
#include <zephyr/sys/util.h>

static uint8_t adc_select[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t source_select[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t band_1_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_2_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_3_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_4_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_5_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_6_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_7_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t band_8_compressor_lut[CODEC_BYTES_FROM_WORDS(34)];
static uint8_t phase_comp_gain_1[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t phase_comp_gain_2[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t phase_comp_gain_3[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t output_headroom_gain[CODEC_BYTES_FROM_WORDS(1)];
static uint8_t soft_clip_lut[CODEC_BYTES_FROM_WORDS(45)];

static uint8_t* const parameter_values[CODEC_PARAMETER_COUNT] = {
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_ADC_SELECT)] = adc_select,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_SOURCE_SELECT)] = source_select,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_1_COMPRESSOR_LUT)] = band_1_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_2_COMPRESSOR_LUT)] = band_2_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_3_COMPRESSOR_LUT)] = band_3_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_4_COMPRESSOR_LUT)] = band_4_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_5_COMPRESSOR_LUT)] = band_5_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_6_COMPRESSOR_LUT)] = band_6_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_7_COMPRESSOR_LUT)] = band_7_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_BAND_8_COMPRESSOR_LUT)] = band_8_compressor_lut,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_PHASE_COMP_GAIN_1)] = phase_comp_gain_1,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_PHASE_COMP_GAIN_2)] = phase_comp_gain_2,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_PHASE_COMP_GAIN_3)] = phase_comp_gain_3,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN)] = output_headroom_gain,
  [CODEC_PARAMETER_INDEX(DSP_PARAMETER_ID_SOFT_CLIP_LUT)] = soft_clip_lut,
};

uint8_t* codec_values_get(uint8_t id)
{
  if (id == DSP_PARAMETER_ID_INVALID || id > ARRAY_SIZE(parameter_values)) {
    return NULL;
  }

  return parameter_values[CODEC_PARAMETER_INDEX(id)];
}

void codec_values_reset(void)
{
  for (size_t index = 0U; index < ARRAY_SIZE(parameter_values); index++) {
    memset(parameter_values[index], 0, codec_contract[index].byte_count);
  }
}

BUILD_ASSERT(ARRAY_SIZE(parameter_values) == CODEC_PARAMETER_COUNT, "Every contract parameter needs RAM storage");
BUILD_ASSERT(sizeof(adc_select) + sizeof(source_select) + sizeof(band_1_compressor_lut) + sizeof(band_2_compressor_lut)
            + sizeof(band_3_compressor_lut) + sizeof(band_4_compressor_lut) + sizeof(band_5_compressor_lut)
            + sizeof(band_6_compressor_lut) + sizeof(band_7_compressor_lut) + sizeof(band_8_compressor_lut)
            + sizeof(phase_comp_gain_1) + sizeof(phase_comp_gain_2) + sizeof(phase_comp_gain_3)
            + sizeof(output_headroom_gain) + sizeof(soft_clip_lut)
        == CODEC_PARAMETER_BYTE_COUNT,
    "Parameter buffers must match the contract byte count");
