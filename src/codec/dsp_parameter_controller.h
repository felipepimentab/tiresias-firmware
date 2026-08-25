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
 * in per-parameter RAM arrays and the nRF5340's internal flash. Builds with
 * CONFIG_AUDIO_CODEC_ADAU1787 also synchronize the ADAU1787's parameter memory;
 * builds without it compile out all codec access. RAM and flash retain the
 * opaque bytes unchanged. The controller owns lifecycle ordering and the
 * boot-local, monotonically increasing parameter revision; storage mechanics
 * remain private to dsp_parameter_settings and codec I/O remains private to
 * codec_adapter.
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
 * In ADAU1787 builds, call this after the codec driver has loaded the generated
 * SigmaStudio image. The controller copies the catalog parameters' SigmaStudio
 * defaults into RAM. Builds without the codec use zero-filled opaque defaults.
 * It then loads each independently stored parameter directly into its matching
 * byte array. Missing settings leave that parameter at its applicable default.
 * ADAU1787 builds write the complete resulting RAM state to the codec before
 * initialization completes.
 *
 * A failed restore leaves the controller unavailable rather than exposing an
 * image that is known not to be synchronized. Repeated successful calls are
 * harmless.
 *
 * @retval 0 RAM and flash are synchronized, as is codec parameter memory when enabled.
 * @return A negative errno-style value when defaults cannot be read, flash
 * cannot be loaded or initialized, or codec restoration fails.
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
 * @brief Apply remotely supplied opaque bytes to every enabled mirror.
 *
 * This is the GATT/external-update path. ADAU1787 builds first write the new
 * byte range to the codec. The controller then saves the owning parameter's
 * complete byte array to its independent flash key and updates RAM. A codec
 * failure leaves flash and RAM unchanged. If persistence fails after a codec
 * write, the controller attempts to restore the codec's previous bytes. Builds
 * without the codec compile out both codec operations.
 *
 * The bytes have no numerical meaning to firmware. Only structural ID and byte
 * bounds required for safe array and DSP access are validated.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] byte_offset Zero-based byte offset within the parameter.
 * @param[in] data New opaque bytes.
 * @param[in] size Number of bytes to apply.
 * @param[out] revision Destination for the new revision on success.
 *
 * @retval 0 Every enabled mirror contains the new bytes.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EACCES The catalog does not permit writes to the parameter.
 * @retval -EINVAL @p data or @p revision is NULL.
 * @retval -ERANGE The requested byte range is empty or outside the parameter.
 * @retval -EAGAIN Startup synchronization has not completed.
 * @retval -EREMOTE Codec access failed.
 * @return Another negative errno-style value when persistence fails.
 */
int dsp_parameter_controller_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Mirror opaque parameter bytes already applied to the codec internally.
 *
 * Internal routines call this after successfully writing the codec. The codec
 * operation remains the caller's responsibility and is expected to use the
 * Codec Adapter. The controller updates the owning RAM parameter and persists
 * only that parameter. It does not write the codec.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] byte_offset Zero-based byte offset within the parameter.
 * @param[in] data Opaque bytes already present in codec parameter memory.
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
 * @retval true RAM, flash, and codec parameter memory are synchronized.
 * @retval false Initialization has not completed successfully.
 */
bool dsp_parameter_controller_loaded(void);

#endif /* DSP_PARAMETER_CONTROLLER_H */
