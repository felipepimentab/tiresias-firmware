/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Runtime lifecycle controller for the fixed DSP parameter contract.
 *
 * The controller keeps the fixed catalog's complete parameter image synchronized
 * in per-parameter RAM arrays and the nRF5340's internal flash. RAM and flash
 * retain the opaque bytes unchanged. The controller owns lifecycle ordering and
 * the boot-local, monotonically increasing parameter revision; storage mechanics
 * remain private to dsp_parameter_settings. Hardware synchronization is deferred
 * while BLE communication and flash persistence are validated.
 *
 * Calls that access or mutate parameter state are serialized internally. The
 * module must be initialized before serving parameter requests.
 */

#ifndef DSP_PARAMETER_CONTROLLER_H
#define DSP_PARAMETER_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the runtime parameter state.
 *
 * The controller starts with zero-filled opaque defaults, then loads each
 * independently stored parameter directly into its matching byte array. Missing
 * settings leave that parameter zero-filled. This stage performs no hardware I/O.
 *
 * A failed settings restore leaves the controller unavailable. Repeated
 * successful calls are harmless.
 *
 * @retval 0 The RAM state was initialized and stored parameters were loaded.
 * @return A negative errno-style value when flash cannot be loaded or initialized.
 */
int dsp_parameter_controller_init(void);

/**
 * @brief Read an opaque byte range from a catalog parameter.
 *
 * The controller resolves @p id through the catalog and copies bytes from its
 * synchronized parameter array without interpreting them. Reads never perform
 * codec or flash I/O.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] byte_offset Zero-based byte offset within the parameter.
 * @param[out] data Destination for the opaque bytes.
 * @param[in] size Number of bytes to copy.
 * @param[out] revision Destination for the current parameter revision. It is
 * written together with the value while the RAM mirror is locked.
 *
 * @retval 0 The value and revision were read from the RAM mirror.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p data or @p revision is NULL.
 * @retval -ERANGE The requested byte range is empty or outside the parameter.
 * @retval -EAGAIN Startup synchronization has not completed.
 */
int dsp_parameter_controller_get(uint8_t id, uint8_t byte_offset, uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Persist remotely supplied opaque bytes and update the RAM mirror.
 *
 * This is the GATT/external-update path. The controller saves the owning
 * parameter's complete byte array to its independent flash key, then updates
 * RAM. A persistence failure leaves RAM unchanged. This stage performs no
 * hardware I/O.
 *
 * The bytes have no numerical meaning to firmware. Only structural ID and byte
 * bounds required for safe array access are validated.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] byte_offset Zero-based byte offset within the parameter.
 * @param[in] data New opaque bytes.
 * @param[in] size Number of bytes to apply.
 * @param[out] revision Destination for the new revision on success.
 *
 * @retval 0 Flash and RAM contain the new bytes.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EACCES The catalog does not permit writes to the parameter.
 * @retval -EINVAL @p data or @p revision is NULL.
 * @retval -ERANGE The requested byte range is empty or outside the parameter.
 * @retval -EAGAIN Startup synchronization has not completed.
 * @return Another negative errno-style value when persistence fails.
 */
int dsp_parameter_controller_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Persist an internally supplied update and mirror it in RAM.
 *
 * This compatibility entry point updates the owning RAM parameter and persists
 * only that parameter. It performs no hardware I/O. It remains available for
 * existing internal callers until codec updates are routed through one owner.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] byte_offset Zero-based byte offset within the parameter.
 * @param[in] data Opaque bytes to persist and mirror.
 * @param[in] size Number of bytes to mirror.
 * @param[out] revision Destination for the committed revision.
 *
 * @retval 0 Flash and RAM now mirror the codec value.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p data or @p revision is NULL.
 * @retval -ERANGE The requested byte range is empty or outside the parameter.
 * @retval -EAGAIN The controller has not completed startup synchronization.
 * @return Another negative errno-style value when persistence fails.
 */
int dsp_parameter_controller_mirror_codec_update(
    uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Get the revision of the committed runtime parameter state.
 *
 * The revision starts at zero on each boot and increments once for every
 * successful remote or internal update that changes a value. Failed and no-op
 * updates do not advance it.
 *
 * @return Current parameter revision.
 */
uint32_t dsp_parameter_controller_revision(void);

/**
 * @brief Report whether parameter initialization completed successfully.
 *
 * @retval true The RAM state was initialized from zero defaults and flash.
 * @retval false Initialization has not completed successfully.
 */
bool dsp_parameter_controller_loaded(void);

#endif /* DSP_PARAMETER_CONTROLLER_H */
