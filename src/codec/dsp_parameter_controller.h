/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief RAM and flash controller for the fixed DSP parameter contract.
 *
 * The controller owns the mutable state of every catalog parameter. Values are
 * concatenated in contract order in one @ref DSP_PARAMETER_BYTE_COUNT-byte RAM
 * image. The module validates public IDs, access flags, and byte ranges, and
 * serializes initialization, reads, and writes with one mutex.
 *
 * Initialization zero-fills the RAM image and overlays independently persisted
 * values through dsp_parameter_settings. A successful change saves the complete
 * owning parameter before updating RAM. The boot-local revision advances once
 * for each successfully persisted change and does not advance for failed or
 * no-op writes.
 *
 * This proof-of-concept module performs no codec hardware access. Contract
 * metadata belongs to dsp_parameter_catalog and Zephyr Settings mechanics belong
 * to dsp_parameter_settings.
 */

#ifndef DSP_PARAMETER_CONTROLLER_H
#define DSP_PARAMETER_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the runtime parameter image from flash.
 *
 * The function initializes the Zephyr Settings backend, zero-fills the packed
 * RAM image, and loads each parameter from its independent settings key. A
 * missing key leaves that parameter zero-filled. The revision is reset to zero
 * for the current boot.
 *
 * Successful repeated calls have no effect. A failed call leaves the controller
 * unavailable; a later call retries initialization from a newly zero-filled
 * image.
 *
 * @retval 0 Initialization completed or had already completed.
 * @return A negative errno-style value from settings initialization or loading.
 */
int dsp_parameter_controller_init(void);

/**
 * @brief Read an opaque byte range from the RAM image.
 *
 * Reads never access flash or codec hardware. The bytes and revision are copied
 * while the controller mutex is held, so they describe the same committed RAM
 * state.
 *
 * @param id Stable parameter ID from the fixed contract.
 * @param byte_offset Zero-based offset within the parameter.
 * @param[out] data Destination for @p size opaque bytes.
 * @param size Number of bytes to read; must be nonzero and remain within the
 * parameter.
 * @param[out] revision Destination for the current boot-local revision.
 *
 * @retval 0 The bytes and current revision were returned.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EINVAL An output pointer is NULL.
 * @retval -ERANGE The requested range is invalid.
 * @retval -EAGAIN The controller is not initialized.
 */
int dsp_parameter_controller_get(uint8_t id, uint8_t byte_offset, uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Persist an opaque byte range and update the RAM image.
 *
 * The requested bytes are merged into a temporary copy of the complete owning
 * parameter. That complete value is saved to flash before the RAM image and
 * revision are updated. A persistence failure therefore leaves RAM and the
 * revision unchanged.
 *
 * Writing bytes already present in RAM is a successful no-op: flash is not
 * written, the revision does not advance, and @p revision receives the current
 * value.
 *
 * @param id Stable parameter ID from the fixed contract.
 * @param byte_offset Zero-based offset within the parameter.
 * @param data Source containing @p size opaque bytes.
 * @param size Number of bytes to write; must be nonzero and remain within the
 * parameter.
 * @param[out] revision Destination for the committed boot-local revision.
 *
 * @retval 0 The change was persisted and committed, or the requested bytes were
 * already current.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EACCES The parameter is read-only.
 * @retval -EINVAL An input or output pointer is NULL.
 * @retval -ERANGE The requested range is invalid.
 * @retval -EAGAIN The controller is not initialized.
 * @return Another negative errno-style value from flash persistence.
 */
int dsp_parameter_controller_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Return the boot-local revision of the committed RAM image.
 *
 * The value is zero before and immediately after initialization. It increments
 * after each successful write that changes a parameter.
 *
 * @return Current boot-local revision.
 */
uint32_t dsp_parameter_controller_revision(void);

/**
 * @brief Report whether initialization from flash completed successfully.
 *
 * This status describes only RAM and flash initialization. Codec hardware is
 * outside the proof-of-concept parameter path.
 *
 * @retval true The RAM image is available for parameter requests.
 * @retval false Initialization has not completed successfully.
 */
bool dsp_parameter_controller_loaded(void);

#endif /* DSP_PARAMETER_CONTROLLER_H */
