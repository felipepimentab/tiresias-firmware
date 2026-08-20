/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_sync_timer.h"

#include <zephyr/kernel.h>

uint32_t audio_sync_timer_capture(void)
{
  return k_ticks_to_us_floor32(k_uptime_ticks());
}

uint32_t audio_sync_timer_capture_get(void)
{
  return audio_sync_timer_capture();
}
