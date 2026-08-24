/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Runtime lifecycle controller for the fixed DSP parameter contract.
 *
 * The controller coordinates codec access, persistent snapshots, cached scalar
 * values, and the monotonically increasing parameter revision. It decides when
 * parameters are loaded, read, written, committed, or rolled back; storage
 * mechanics remain private to dsp_parameter_settings and codec I/O remains
 * private to codec_adapter.
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
 * The controller starts with zero-initialized cached values and then asks the
 * settings adapter for a persistent snapshot. A missing, incompatible, or
 * malformed snapshot leaves that initial state active. Other settings backend
 * failures abort initialization. Repeated successful calls are harmless.
 *
 * Initialization restores the controller's cached scalar state and revision;
 * it does not proactively write the complete snapshot to the codec.
 *
 * @retval 0 The controller is ready, using stored or zero-initialized values.
 * @return A negative errno-style settings error when initialization fails.
 */
int dsp_parameter_controller_init(void);

/**
 * @brief Read one word of a catalog parameter.
 *
 * The controller resolves @p id through the catalog and normally reads the word
 * from the codec through codec_adapter. While codec parameter access is
 * unsupported, persistent single-word parameters fall back to their cached
 * values. Multiword parameters have no cached fallback.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] word_index Zero-based word offset within the parameter.
 * @param[out] value Destination for the integer or signed Q5.23 word.
 * @param[out] revision Destination for the current parameter revision. It is
 * written after a valid request reaches the serialized read operation, even if
 * codec access ultimately fails.
 *
 * @retval 0 The value was read from the codec or persistent scalar cache.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p value or @p revision is NULL.
 * @retval -ERANGE @p word_index is outside the parameter.
 * @retval -EREMOTE Codec access failed or no cached fallback exists.
 */
int dsp_parameter_controller_get(uint8_t id, uint8_t word_index, int32_t* value, uint32_t* revision);

/**
 * @brief Apply and persist a new scalar parameter value.
 *
 * A successful operation is transactional from the controller's perspective:
 * it writes the codec when codec access is available, stores the complete
 * pending snapshot, then advances the cached state and revision. If persistence
 * fails after a codec write, the controller attempts to restore the previous
 * codec value. When codec access reports -ENOTSUP, the persistent state is still
 * committed so the lifecycle can be exercised before hardware parameter
 * operations are implemented.
 *
 * The PoC trusts the workstation to provide correctly encoded values and does
 * not perform firmware-side range, step, or prescription validation. Only
 * structural ID and word bounds required for safe array and DSP access remain.
 *
 * @param[in] id Stable parameter ID from the fixed contract.
 * @param[in] word_index Zero-based word offset within the parameter.
 * @param[in] value New integer or signed Q5.23 scalar value.
 * @param[out] revision Destination for the new revision on success.
 *
 * @retval 0 The value was persisted and the runtime state was committed.
 * @retval -ENOENT @p id is not part of the catalog.
 * @retval -EINVAL @p revision is NULL.
 * @retval -EACCES The parameter is not one of the persisted PoC scalars.
 * @retval -ERANGE The word index is outside the parameter.
 * @retval -EREMOTE Codec access failed.
 * @return Another negative errno-style value when persistence fails.
 */
int dsp_parameter_controller_set(uint8_t id, uint8_t word_index, int32_t value, uint32_t* revision);

/**
 * @brief Get the revision of the committed runtime parameter state.
 *
 * The revision is restored from persistent storage during initialization and
 * increments once for every successful set operation. Failed operations do not
 * advance it.
 *
 * @return Current parameter revision.
 */
uint32_t dsp_parameter_controller_revision(void);

/**
 * @brief Report whether parameter initialization completed successfully.
 *
 * @retval true Initial state or a valid stored snapshot has been loaded.
 * @retval false Initialization has not completed successfully.
 */
bool dsp_parameter_controller_loaded(void);

#endif /* DSP_PARAMETER_CONTROLLER_H */
