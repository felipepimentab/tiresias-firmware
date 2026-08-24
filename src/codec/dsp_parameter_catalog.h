/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Fixed DSP parameter contract and firmware-only codec bindings.
 *
 * The catalog is the authoritative description of the DSP parameters exposed
 * by the MVP BLE contract. It assigns stable block and parameter IDs, describes
 * the public shape and capabilities of each parameter, and binds those public
 * definitions to private SigmaStudio DSP addresses and scalar constraints.
 *
 * Contract membership implies read access. Parameters are read-only Q5.23
 * values unless their flags explicitly indicate write access or integer
 * representation. Human-readable names are intentionally kept in the matching
 * workstation contract rather than transmitted by the firmware.
 *
 * This module contains immutable metadata only. It does not own runtime values,
 * persistence, BLE request handling, or codec access policy.
 */

#ifndef DSP_PARAMETER_CATALOG_H
#define DSP_PARAMETER_CATALOG_H

#include <stdint.h>

/** CRC-32 fingerprint of the ordered public parameter contract. */
#define DSP_PARAMETER_CONTRACT_CRC32 0xF62C1808U

/** Number of entries in @ref dsp_parameter_contract. */
#define DSP_PARAMETER_COUNT 15U

/** Number of scalar parameter values retained in persistent storage. */
#define DSP_PARAMETER_PERSISTENT_COUNT 6U

/**
 * @brief Public parameter capability and representation flags.
 *
 * Flags occupy one byte in @ref dsp_parameter and may be combined. The absence
 * of @ref DSP_PARAMETER_CONTRACT_FLAG_INTEGER denotes a signed Q5.23 value.
 */
enum dsp_parameter_contract_flag {
  /** The parameter accepts write requests; all parameters remain readable. */
  DSP_PARAMETER_CONTRACT_FLAG_WRITABLE = 1U << 0,

  /** Parameter words are signed integers rather than signed Q5.23 values. */
  DSP_PARAMETER_CONTRACT_FLAG_INTEGER = 1U << 1,
};

/**
 * @brief Stable identifiers for DSP processing blocks.
 *
 * A block ID groups each parameter with its logical processing block. These
 * values are part of the firmware/workstation contract and must not be reused.
 */
enum dsp_block_id {
  /** ADC input selection block. */
  DSP_BLOCK_ID_ADC_SELECT = 1,
  /** Audio source selection block. */
  DSP_BLOCK_ID_SOURCE_SELECT = 2,
  /** Band 1 compressor block. */
  DSP_BLOCK_ID_BAND_1_COMPRESSOR = 3,
  /** Band 2 compressor block. */
  DSP_BLOCK_ID_BAND_2_COMPRESSOR = 4,
  /** Band 3 compressor block. */
  DSP_BLOCK_ID_BAND_3_COMPRESSOR = 5,
  /** Band 4 compressor block. */
  DSP_BLOCK_ID_BAND_4_COMPRESSOR = 6,
  /** Band 5 compressor block. */
  DSP_BLOCK_ID_BAND_5_COMPRESSOR = 7,
  /** Band 6 compressor block. */
  DSP_BLOCK_ID_BAND_6_COMPRESSOR = 8,
  /** Band 7 compressor block. */
  DSP_BLOCK_ID_BAND_7_COMPRESSOR = 9,
  /** Band 8 compressor block. */
  DSP_BLOCK_ID_BAND_8_COMPRESSOR = 10,
  /** First phase-compensation gain block. */
  DSP_BLOCK_ID_PHASE_COMP_GAIN_1 = 11,
  /** Second phase-compensation gain block. */
  DSP_BLOCK_ID_PHASE_COMP_GAIN_2 = 12,
  /** Third phase-compensation gain block. */
  DSP_BLOCK_ID_PHASE_COMP_GAIN_3 = 13,
  /** Output headroom gain block. */
  DSP_BLOCK_ID_OUTPUT_HEADROOM = 14,
  /** Soft-clip transfer-function block. */
  DSP_BLOCK_ID_SOFT_CLIP = 15,
};

/**
 * @brief Stable identifiers for parameters exposed over BLE.
 *
 * IDs are the only parameter identity carried by parameter read and write
 * requests. The firmware resolves an ID to a private DSP address through
 * @ref dsp_parameter_find, while the workstation resolves it to a display name.
 */
enum dsp_parameter_id {
  /** ADC selection scalar. */
  DSP_PARAMETER_ID_ADC_SELECT = 1,
  /** Source selection scalar. */
  DSP_PARAMETER_ID_SOURCE_SELECT = 2,
  /** Band 1 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_1_COMPRESSOR_LUT = 3,
  /** Band 2 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_2_COMPRESSOR_LUT = 4,
  /** Band 3 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_3_COMPRESSOR_LUT = 5,
  /** Band 4 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_4_COMPRESSOR_LUT = 6,
  /** Band 5 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_5_COMPRESSOR_LUT = 7,
  /** Band 6 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_6_COMPRESSOR_LUT = 8,
  /** Band 7 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_7_COMPRESSOR_LUT = 9,
  /** Band 8 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_8_COMPRESSOR_LUT = 10,
  /** First phase-compensation gain scalar. */
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_1 = 11,
  /** Second phase-compensation gain scalar. */
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_2 = 12,
  /** Third phase-compensation gain scalar. */
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_3 = 13,
  /** Output headroom gain scalar. */
  DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN = 14,
  /** Soft-clip lookup table. */
  DSP_PARAMETER_ID_SOFT_CLIP_LUT = 15,
};

/**
 * @brief Compact public definition of one DSP parameter.
 *
 * This four-byte representation is shared with the workstation contract and is
 * used to calculate the fixed contract fingerprint. It deliberately excludes
 * names, constraints, defaults, and DSP addresses.
 */
struct dsp_parameter {
  /** Stable @ref dsp_parameter_id value. */
  uint8_t id;

  /** Stable @ref dsp_block_id value for the owning processing block. */
  uint8_t block_id;

  /** Number of consecutive 32-bit DSP words in the parameter. */
  uint8_t word_count;

  /** Bitwise combination of @ref dsp_parameter_contract_flag values. */
  uint8_t flags;
};

/**
 * @brief Firmware-only binding and scalar constraints for one parameter.
 *
 * The descriptor augments the public contract entry with implementation data
 * that must never be exposed as raw codec access over BLE. Range, default, and
 * step fields use the representation selected by the public definition flags.
 */
struct dsp_parameter_descriptor {
  /** Public contract definition associated with this binding. */
  const struct dsp_parameter* definition;

  /** Private byte address of the parameter's first word in DSP memory. */
  uint16_t dsp_address;

  /** Inclusive minimum accepted for a writable scalar. */
  int32_t minimum;

  /** Inclusive maximum accepted for a writable scalar. */
  int32_t maximum;

  /** Value used when no valid persistent scalar snapshot is available. */
  int32_t default_value;

  /** Required increment from @ref minimum for a writable scalar. */
  int32_t step;
};

/**
 * @brief Ordered public parameter contract.
 *
 * The entry order and contents participate in
 * @ref DSP_PARAMETER_CONTRACT_CRC32. Any contract change must be mirrored in
 * `tiresias_workstation.domain.dsp_contract` and reflected in the contract
 * CRC.
 */
extern const struct dsp_parameter dsp_parameter_contract[DSP_PARAMETER_COUNT];

/**
 * @brief Resolve a public parameter ID to its firmware-only descriptor.
 *
 * @param[in] id Stable parameter ID from @ref dsp_parameter_id.
 *
 * @return Pointer to immutable catalog storage when the ID exists.
 * @retval NULL The ID is not part of the fixed contract.
 */
const struct dsp_parameter_descriptor* dsp_parameter_find(uint8_t id);

/**
 * @brief Validate a requested write against the fixed parameter contract.
 *
 * Validation covers catalog membership, word bounds, write permission, scalar
 * shape, inclusive range, and step alignment. The current MVP permits writes
 * only to single-word parameters at word index zero.
 *
 * @param[in] parameter Descriptor returned by @ref dsp_parameter_find, or NULL.
 * @param[in] word_index Zero-based word offset within the parameter.
 * @param[in] value Integer or signed Q5.23 value, as selected by the flags.
 *
 * @retval 0 The requested value is valid and writable.
 * @retval -ENOENT @p parameter is NULL.
 * @retval -EACCES The parameter or selected word is not writable.
 * @retval -ERANGE The word index, value, or step alignment is invalid.
 */
int dsp_parameter_validate(const struct dsp_parameter_descriptor* parameter, uint8_t word_index, int32_t value);

#endif /* DSP_PARAMETER_CATALOG_H */
