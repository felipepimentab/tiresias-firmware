/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief RAM and flash controller for the fixed DSP parameter contract.
 *
 * The controller owns one packed RAM image, serialized access, and the
 * boot-local parameter revision. It delegates contract metadata to
 * dsp_parameter_catalog and flash access to dsp_parameter_settings. This proof
 * of concept performs no codec hardware access.
 */

#ifndef DSP_PARAMETER_CONTROLLER_H
#define DSP_PARAMETER_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Initialize the zero-filled RAM image and overlay values stored in flash. */
int dsp_parameter_controller_init(void);

/**
 * Read an opaque byte range from the RAM image.
 *
 * @retval 0 The bytes and current revision were returned.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EINVAL An output pointer is NULL.
 * @retval -ERANGE The requested range is invalid.
 * @retval -EAGAIN The controller is not initialized.
 */
int dsp_parameter_controller_get(uint8_t id, uint8_t byte_offset, uint8_t* data, size_t size, uint32_t* revision);

/**
 * Persist an opaque byte range and update the RAM image.
 *
 * @retval 0 Flash, RAM, and the revision were updated.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EACCES The parameter is read-only.
 * @retval -EINVAL An input or output pointer is NULL.
 * @retval -ERANGE The requested range is invalid.
 * @retval -EAGAIN The controller is not initialized.
 * @return Another negative errno-style value from flash persistence.
 */
int dsp_parameter_controller_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision);

/** Return the boot-local revision of the persisted RAM image. */
uint32_t dsp_parameter_controller_revision(void);

/** Return whether initialization from flash completed successfully. */
bool dsp_parameter_controller_loaded(void);

#endif /* DSP_PARAMETER_CONTROLLER_H */
