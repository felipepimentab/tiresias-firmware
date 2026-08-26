/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Mutable parameter state for the fixed codec contract.
 *
 * This module owns one mutable byte buffer for every contract parameter. A
 * fixed pointer table maps stable parameter IDs to their buffers. The module
 * validates public IDs, access flags, and byte ranges, and serializes
 * initialization, reads, and writes with one mutex.
 *
 * Initialization zero-fills the parameter buffers and overlays independently
 * persisted values through codec_settings. A successful change saves the
 * complete owning parameter before updating RAM. The boot-local revision
 * advances once for each successfully persisted change and does not advance for
 * failed or no-op writes.
 *
 * This proof-of-concept module performs no codec hardware access. Contract
 * metadata belongs to codec_contract and Zephyr Settings mechanics belong to
 * codec_settings.
 */

#ifndef CODEC_PARAMETERS_H
#define CODEC_PARAMETERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the runtime parameter buffers from flash.
 *
 * The function initializes the Zephyr Settings backend, zero-fills every
 * parameter buffer, and loads each parameter from its independent settings key.
 * A missing key leaves that parameter zero-filled. The revision is reset to
 * zero for the current boot.
 *
 * Successful repeated calls have no effect. A failed call leaves the module
 * unavailable; a later call retries initialization from a newly zero-filled
 * set of buffers.
 *
 * @retval 0 Initialization completed or had already completed.
 * @return A negative errno-style value from settings initialization or loading.
 */
int codec_parameters_init(void);

/**
 * @brief Read an opaque byte range from a parameter buffer.
 *
 * Reads never access flash or codec hardware. The bytes and revision are copied
 * while the module mutex is held, so they describe the same committed RAM
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
 * @retval -EAGAIN The module is not initialized.
 */
int codec_parameters_get(uint8_t id, uint8_t byte_offset, uint8_t* data, size_t size, uint32_t* revision);

/**
 * @brief Persist an opaque byte range and update its parameter buffer.
 *
 * The requested bytes are merged into a temporary copy of the complete owning
 * parameter. That complete value is saved to flash before the RAM buffer and
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
 * @retval -EAGAIN The module is not initialized.
 * @return Another negative errno-style value from flash persistence.
 */
int codec_parameters_set(uint8_t id, uint8_t byte_offset, const uint8_t* data, size_t size, uint32_t* revision);

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
