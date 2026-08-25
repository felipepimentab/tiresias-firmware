/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Flash persistence adapter for DSP parameter snapshots.
 *
 * This module is the exclusive owner of the Zephyr Settings integration and
 * the versioned binary record used to retain DSP parameter state. It registers
 * the settings handler, serializes and deserializes complete snapshots, and
 * verifies record framing, contract compatibility, and CRC integrity.
 *
 * The adapter deliberately has no parameter lifecycle policy. It does not
 * choose initial values, interpret individual values, access the codec,
 * increment revisions, or decide when a load or save should occur. Those
 * responsibilities belong to dsp_parameter_controller.
 */

#ifndef DSP_PARAMETER_SETTINGS_H
#define DSP_PARAMETER_SETTINGS_H

#include "dsp_parameter_catalog.h"

#include <stdint.h>

/**
 * @brief Complete persistent state exchanged with the parameter controller.
 *
 * Values are stored by ascending parameter ID and then ascending word index. A
 * snapshot is an atomic persistence unit: callers load or replace every DSP
 * parameter word and the associated revision together.
 */
struct dsp_parameter_settings_snapshot {
  /** Revision associated with the committed parameter values. */
  uint32_t revision;

  /** Every raw integer or signed Q5.23 word in fixed catalog order. */
  int32_t values[DSP_PARAMETER_WORD_COUNT];
};

/**
 * @brief Register the module's Zephyr Settings handler.
 *
 * This must run before the application performs a global settings load. It is
 * idempotent and does not read flash by itself.
 *
 * @retval 0 The handler is registered or was already registered.
 * @return A negative errno-style value from Zephyr Settings registration.
 */
int dsp_parameter_settings_init(void);

/**
 * @brief Load and decode the persistent parameter snapshot from flash.
 *
 * A successful load verifies the record magic, storage version, encoded size,
 * fixed DSP contract CRC, value count, reserved fields, and record CRC before
 * copying data to @p snapshot. Parameter values are treated as opaque signed
 * words.
 *
 * This operation is intended for controller initialization and is not
 * reentrant. The output snapshot is modified only on success.
 *
 * @param[out] snapshot Destination for the decoded persistent state.
 *
 * @retval 0 A valid snapshot was loaded.
 * @retval -EINVAL @p snapshot is NULL or the stored record is incompatible or
 * malformed.
 * @retval -ENOENT No parameter record exists.
 * @retval -EIO The settings backend returned an incomplete record.
 * @return Another negative errno-style value from the Zephyr Settings backend.
 */
int dsp_parameter_settings_load(struct dsp_parameter_settings_snapshot* snapshot);

/**
 * @brief Encode and save a complete parameter snapshot to flash.
 *
 * The record is stored under the module-owned settings key and includes its
 * framing metadata and CRC. This function does not validate parameter values or
 * revision policy; the controller must provide a coherent snapshot.
 *
 * @param[in] snapshot Complete state to persist.
 *
 * @retval 0 The settings backend accepted the complete record.
 * @retval -EINVAL @p snapshot is NULL.
 * @return Another negative errno-style value from the Zephyr Settings backend.
 */
int dsp_parameter_settings_save(const struct dsp_parameter_settings_snapshot* snapshot);

#endif /* DSP_PARAMETER_SETTINGS_H */
