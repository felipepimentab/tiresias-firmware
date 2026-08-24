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
 * choose defaults, validate individual values against the catalog, access the
 * codec, increment revisions, or decide when a load or save should occur. Those
 * responsibilities belong to dsp_parameter_controller.
 */

#ifndef DSP_PARAMETER_SETTINGS_H
#define DSP_PARAMETER_SETTINGS_H

#include "dsp_parameter_catalog.h"

#include <stdint.h>

/**
 * @brief Complete persistent state exchanged with the parameter controller.
 *
 * Values are stored in the controller's fixed persistent-parameter order. A
 * snapshot is an atomic persistence unit: callers load or replace all values
 * and the associated revision together.
 */
struct dsp_parameter_settings_snapshot {
  /** Revision associated with the committed parameter values. */
  uint32_t revision;

  /** Raw integer or signed Q5.23 values in fixed persistent catalog order. */
  int32_t values[DSP_PARAMETER_PERSISTENT_COUNT];
};

/**
 * @brief Load and decode the persistent parameter snapshot from flash.
 *
 * The settings handler is registered lazily on the first call. A successful
 * load verifies the record magic, storage version, encoded size, fixed DSP
 * contract ID, value count, reserved fields, and CRC before copying data to
 * @p snapshot. Semantic validation of individual values is left to the caller.
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
