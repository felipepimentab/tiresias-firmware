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
 * across three locations: per-parameter byte arrays in RAM, the nRF5340's
 * internal flash, and the ADAU1787's parameter memory. RAM and flash retain
 * words byte-for-byte in ADAU1787 control-port order. The controller owns
 * lifecycle ordering and the boot-local, monotonically increasing parameter
 * revision; storage mechanics remain private to dsp_parameter_settings and
 * codec I/O remains private to codec_adapter.
 *
 * Calls that access or mutate parameter state are serialized internally. The
 * module must be initialized before serving parameter requests.
 */

#ifndef DSP_PARAMETER_CONTROLLER_H
#define DSP_PARAMETER_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the runtime parameter state.
 *
 * Call this after the codec driver has loaded the generated SigmaStudio image.
 * The controller first copies the catalog parameters' SigmaStudio defaults into
 * RAM, then loads each independently stored parameter directly into its matching
 * byte array. Missing settings leave that parameter at its default. The complete
 * resulting RAM state is written to the codec before initialization completes.
 *
 * A failed restore leaves the controller unavailable rather than exposing an
 * image that is known not to be synchronized. Repeated successful calls are
 * harmless.
 *
 * @retval 0 RAM, flash, and codec parameter memory are synchronized.
 * @return A negative errno-style value when defaults cannot be read, flash
 * cannot be loaded or initialized, or codec restoration fails.
 */
int dsp_parameter_controller_init(void);

/**
 * @brief Read one word of a catalog parameter.
 *
 * The controller resolves @p id through the catalog, reads the raw word from
 * its synchronized parameter byte array, and converts it to the signed value
 * representation used by the Control Link API. Reads never perform codec or
 * flash I/O.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] word_index Zero-based word offset within the parameter.
 * @param[out] value Destination for the integer or signed Q5.23 word.
 * @param[out] revision Destination for the current parameter revision. It is
 * written together with the value while the RAM mirror is locked.
 *
 * @retval 0 The value and revision were read from the RAM mirror.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p value or @p revision is NULL.
 * @retval -ERANGE @p word_index is outside the parameter.
 * @retval -EAGAIN Startup synchronization has not completed.
 */
int dsp_parameter_controller_get(uint8_t id, uint8_t word_index, int32_t* value, uint32_t* revision);

/**
 * @brief Apply a remotely supplied parameter value to all three mirrors.
 *
 * This is the GATT/external-update path. The controller first writes the new
 * word to the codec, updates its owning RAM parameter, and saves that parameter's
 * byte array to its independent flash key. A codec failure leaves flash and RAM
 * unchanged. If persistence fails after the codec write, the controller restores
 * the RAM word and attempts to restore the codec's previous value.
 *
 * The PoC trusts the workstation to provide correctly encoded values and does
 * not perform firmware-side range, step, or prescription validation. Only
 * structural ID and word bounds required for safe array and DSP access remain.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] word_index Zero-based word offset within the parameter.
 * @param[in] value New integer or signed Q5.23 word.
 * @param[out] revision Destination for the new revision on success.
 *
 * @retval 0 Codec, flash, and RAM contain the new value.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p revision is NULL.
 * @retval -ERANGE The word index is outside the parameter.
 * @retval -EAGAIN Startup synchronization has not completed.
 * @retval -EREMOTE Codec access failed.
 * @return Another negative errno-style value when persistence fails.
 */
int dsp_parameter_controller_set(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision);

/**
 * @brief Mirror a parameter value already applied to the codec internally.
 *
 * Internal routines call this after successfully writing the codec. The codec
 * operation remains the caller's responsibility and is expected to use the
 * Codec Adapter. The controller updates the owning RAM parameter and persists
 * only that parameter. It does not write the codec.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] word_index Zero-based word offset within the parameter.
 * @param[in] value Value already present in codec parameter memory.
 * @param[out] revision Destination for the committed revision.
 *
 * @retval 0 Flash and RAM now mirror the codec value.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p revision is NULL.
 * @retval -ERANGE @p word_index is outside the parameter.
 * @retval -EAGAIN The controller has not completed startup synchronization.
 * @return Another negative errno-style value when persistence fails.
 */
int dsp_parameter_controller_mirror_codec_update(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision);

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
