/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_adapter.h"

#include <errno.h>
#include <zephyr/sys/util.h>

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
