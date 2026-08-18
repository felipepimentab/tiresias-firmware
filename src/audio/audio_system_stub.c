/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_system.h"

#include "streamctrl.h"

#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(audio_system, CONFIG_AUDIO_SYSTEM_LOG_LEVEL);

static atomic_t pipeline_running = ATOMIC_INIT(0);

void audio_system_encoder_start(void)
{
}

void audio_system_encoder_stop(void)
{
}

int audio_system_encode_test_tone_set(uint32_t freq)
{
  ARG_UNUSED(freq);

  return -ENOTSUP;
}

int audio_system_encode_test_tone_step(void)
{
  return -ENOTSUP;
}

int audio_system_config_set(uint32_t encoder_sample_rate_hz, uint32_t encoder_bitrate, uint32_t decoder_sample_rate_hz)
{
  ARG_UNUSED(encoder_sample_rate_hz);
  ARG_UNUSED(encoder_bitrate);

  if (decoder_sample_rate_hz != 0U && decoder_sample_rate_hz != 16000U && decoder_sample_rate_hz != 24000U
      && decoder_sample_rate_hz != 48000U) {
    return -EINVAL;
  }

  return 0;
}

int audio_system_decode(void const* const encoded_data, size_t encoded_data_size, bool bad_frame)
{
  ARG_UNUSED(encoded_data);
  ARG_UNUSED(encoded_data_size);
  ARG_UNUSED(bad_frame);

  return 0;
}

void audio_system_start(void)
{
  atomic_set(&pipeline_running, 1);
}

void audio_system_stop(void)
{
  atomic_clear(&pipeline_running);
}

int audio_system_fifo_rx_block_drop(void)
{
  return 0;
}

int audio_system_decoder_num_ch_get(void)
{
  return 1;
}

int audio_system_init(void)
{
  LOG_INF("Audio hardware disabled; ISO audio payloads will be discarded");

  return 0;
}

uint8_t stream_state_get(void)
{
  return atomic_get(&pipeline_running) != 0 ? STATE_STREAMING : STATE_PAUSED;
}
