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
 * definitions to private SigmaStudio DSP addresses.
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
 * requests. The firmware uses an ID to index @ref dsp_parameter_addresses,
 * while the workstation resolves it to a display name.
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
 * names, prescription constraints, and DSP addresses.
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
 * @brief Ordered public parameter contract.
 *
 * Entries are ordered by ID, so a validated nonzero ID maps to array index
 * `id - 1`.
 *
 * The entry order and contents participate in
 * @ref DSP_PARAMETER_CONTRACT_CRC32. Any contract change must be mirrored in
 * `tiresias_workstation.domain.dsp_contract` and reflected in the contract
 * CRC.
 */
extern const struct dsp_parameter dsp_parameter_contract[DSP_PARAMETER_COUNT];

/**
 * @brief Firmware-only DSP addresses indexed directly by parameter ID.
 *
 * Index zero is unused because public parameter IDs start at one. Each other
 * entry is the private byte address of that parameter's first DSP word. DSP
 * addresses are never transmitted over BLE.
 */
extern const uint16_t dsp_parameter_addresses[DSP_PARAMETER_COUNT + 1U];

#endif /* DSP_PARAMETER_CATALOG_H */
