/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Coordination of runtime codec parameters and persistence.
 *
 * This module validates public IDs, access flags, and complete value sizes, and
 * serializes initialization, reads, and writes with one mutex. Codec Values
 * owns the mutable parameter buffers and Codec Settings owns persistence.
 *
 * Initialization copies SigmaStudio defaults into the parameter buffers and
 * overlays independently persisted values through codec_settings. A successful
 * change saves the complete parameter before updating RAM. The boot-local revision
 * advances once for each successfully persisted change and does not advance for
 * failed or no-op writes.
 *
 * This proof-of-concept module performs no codec hardware access. Contract
 * metadata belongs to codec_contract.
 */

#ifndef CODEC_PARAMETERS_H
#define CODEC_PARAMETERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the runtime parameter buffers from flash.
 *
 * The function initializes the Zephyr Settings backend, copies every parameter's
 * SigmaStudio default into its buffer, and loads each parameter from its
 * independent settings key. A missing key leaves the generated default intact.
 * The revision is reset to zero for the current boot.
 *
 * Successful repeated calls have no effect. A failed call leaves the module
 * unavailable; a later call retries initialization from the generated defaults.
 *
 * @retval 0 Initialization completed or had already completed.
 * @return A negative errno-style value from settings initialization or loading.
 */
int codec_parameters_init(void);

/**
 * @brief Read one complete opaque parameter value.
 *
 * Reads never access flash or codec hardware. The bytes and revision are copied
 * while the module mutex is held, so they describe the same committed RAM
 * state.
 *
 * @param id Stable parameter ID from the fixed contract.
 * @param[out] data Destination for the complete opaque value.
 * @param size Size of @p data; must match the parameter's contract byte count.
 * @param[out] revision Destination for the current boot-local revision.
 *
 * @retval 0 The bytes and current revision were returned.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EINVAL An output pointer is NULL.
 * @retval -ERANGE @p size does not match the parameter's byte count.
 * @retval -EAGAIN The module is not initialized.
 */
int codec_parameters_get(uint8_t id, uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Persist one complete opaque value and update its parameter buffer.
 *
 * The complete value is saved to flash before the RAM buffer and revision are
 * updated. A persistence failure therefore leaves RAM and the revision
 * unchanged.
 *
 * Writing the value already present in RAM is a successful no-op: flash is not
 * written, the revision does not advance, and @p revision receives the current
 * value.
 *
 * @param id Stable parameter ID from the fixed contract.
 * @param data Source containing the complete opaque value.
 * @param size Size of @p data; must match the parameter's contract byte count.
 * @param[out] revision Destination for the committed boot-local revision.
 *
 * @retval 0 The change was persisted and committed, or the value was already
 * current.
 * @retval -ENOENT The parameter ID is unknown.
 * @retval -EACCES The parameter is read-only.
 * @retval -EINVAL An input or output pointer is NULL.
 * @retval -ERANGE @p size does not match the parameter's byte count.
 * @retval -EAGAIN The module is not initialized.
 * @return Another negative errno-style value from flash persistence.
 */
int codec_parameters_set(uint8_t id, const uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Return the boot-local revision of the committed parameter buffers.
 *
 * The value is zero before and immediately after initialization. It increments
 * after each successful write that changes a parameter.
 *
 * @return Current boot-local revision.
 */
uint32_t codec_parameters_revision(void);

/**
 * @brief Report whether initialization from flash completed successfully.
 *
 * This status describes only RAM and flash initialization. Codec hardware is
 * outside the proof-of-concept parameter path.
 *
 * @retval true The parameter buffers are available for requests.
 * @retval false Initialization has not completed successfully.
 */
bool codec_parameters_loaded(void);

#endif /* CODEC_PARAMETERS_H */
