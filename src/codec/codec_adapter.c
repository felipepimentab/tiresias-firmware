/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_adapter.h"

#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
#include "adau1787.h"
#endif

#include <errno.h>
#include <zephyr/sys/util.h>

int codec_param_default_read(uint16_t start_addr, uint8_t* data, size_t len)
{
#if defined(CONFIG_AUDIO_CODEC_ADAU1787)
  return adau1787_param_default_read(start_addr, data, len);
#else
  ARG_UNUSED(start_addr);
  ARG_UNUSED(data);
  ARG_UNUSED(len);

  return -ENOTSUP;
#endif
}

int codec_param_read(uint16_t start_addr, uint8_t* data, size_t len)
{
  ARG_UNUSED(start_addr);
  ARG_UNUSED(data);
  ARG_UNUSED(len);

  return -ENOTSUP;
}

int codec_param_write(uint16_t start_addr, const uint8_t* data, size_t len)
{
  ARG_UNUSED(start_addr);
  ARG_UNUSED(data);
  ARG_UNUSED(len);

  return -ENOTSUP;
}
