/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_datapath.h"

#include <errno.h>
#include <zephyr/sys/util.h>

static uint32_t presentation_delay_us;

int audio_datapath_tone_play(uint16_t freq, uint16_t dur_ms, float amplitude)
{
  ARG_UNUSED(freq);
  ARG_UNUSED(dur_ms);
  ARG_UNUSED(amplitude);

  return -ENOTSUP;
}

void audio_datapath_tone_stop(void)
{
}

int audio_datapath_pres_delay_us_set(uint32_t delay_us)
{
  presentation_delay_us = delay_us;

  return 0;
}

void audio_datapath_pres_delay_us_get(uint32_t* delay_us)
{
  if (delay_us != NULL) {
    *delay_us = presentation_delay_us;
  }
}

void audio_datapath_stream_out(
    const uint8_t* buf, size_t size, uint32_t sdu_ref_us, bool bad_frame, uint32_t recv_frame_ts_us)
{
  ARG_UNUSED(buf);
  ARG_UNUSED(size);
  ARG_UNUSED(sdu_ref_us);
  ARG_UNUSED(bad_frame);
  ARG_UNUSED(recv_frame_ts_us);
}

int audio_datapath_start(struct data_fifo* fifo_rx)
{
  ARG_UNUSED(fifo_rx);

  return 0;
}

int audio_datapath_stop(void)
{
  return 0;
}

int audio_datapath_init(void)
{
  return 0;
}
