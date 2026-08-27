# ADAU1787 Startup

Codec initialization resets and programs the ADAU1787. Live PCM starts later, after BIS
streaming begins; codec programming therefore precedes regular BCLK/LRCK frames.

## Source map

| Responsibility | Source |
|---|---|
| Startup policy | `src/main.c`, `src/application/device_controller.c` |
| Codec ownership | `src/codec/codec_controller.c` |
| Streaming lifecycle | `src/bluetooth/audio_streaming*.c` |
| Audio pipeline | `src/audio/audio_system.c` |
| Codec abstraction | `src/codec/codec_adapter.c`, `src/codec/hw_codec.c` |
| Driver and SigmaStudio adapter | `src/drivers/adau1787.*`, `src/drivers/SigmaStudioFW.h` |
| Generated DSP image | `src/SigmaStudioFiles/` |
| Application pins | `boards/tiresias_dk_nrf5340_cpuapp.overlay` |
| Base node/binding | Sibling `boards` repository |

## Sequence

1. Device Controller requests Codec Controller initialization and Audio Streaming scan.
2. Audio Streaming prepares the audio datapath and idle I2S peripheral.
3. Codec Controller calls `hw_codec_init()` / `adau1787_init()`.
4. The driver asserts `!PD`, configures MP3-MP6 low, releases `!PD`, and waits 100 ms.
5. The generated SigmaDSP sequence runs, including its 35 ms delay.
6. The generated FastDSP sequence stops and starts FastDSP.
7. Codec Parameters loads the generated defaults into Codec Values and checks flash. A valid
   non-default flash image is restored to codec parameter memory; absent or invalid storage is
   initialized from the defaults already loaded into the codec.
8. Codec Controller enters `LOCAL_ONLY` and mirrors Source Select to flash and RAM if needed.
9. On `LE_AUDIO_EVT_STREAMING`, Audio Streaming calls `audio_system_start()` and starts
   double-buffered I2S.

## Hardware reference

| Signal | Configuration |
|---|---|
| I2C | `i2c1`, address `0x2B`, SDA P1.2, SCL P1.3, Fast Plus |
| `!PD` | P1.0, active low |
| MP3 / MP4 / MP5 / MP6 | P1.14 / P1.15 / P1.12 / P1.13; inactive low |
| I2S MCK | P1.1 |
| I2S SCK / LRCK | P0.6 / P0.7 |
| I2S SDOUT / SDIN | P0.4 / P0.5 |

Bus, address, pins, and bitrate come from devicetree. Generated `DEVICE_ADDR_* = 0x50`
values are ignored by the adapter; transfers use the devicetree address.

Because `!PD` is `GPIO_ACTIVE_LOW`, `GPIO_OUTPUT_ACTIVE` drives it physically low.
`gpio_pin_set_dt(..., 0)` releases it high.

## nRF audio interface

`audio_i2s_init()` starts 12.288 MHz HFCLKAUDIO, applies `i2s0` pinctrl, enables the I2S
interrupt, and initializes I2S0 idle. The default headset configuration is 48 kHz, 16-bit,
stereo, with the nRF5340 as master. `nrfx_i2s_start()` is called only when the streaming
pipeline starts.

## Generated programming

The current Sigma export performs 208 writes and one delay request. Major steps are:

- stop SigmaDSP and power down the analog block;
- configure power, clocks, routing, serial ports, and multipurpose pins;
- write 845 program bytes at `0x5000` and 72 parameter bytes at `0x2000`;
- power the analog block, set DAC volume, and start SigmaDSP.

The delay payload `{0x00, 0x23}` is decoded big-endian as 35 ms. FastDSP then receives
`0x00` followed by `0x01` at `FDSP_RUN` (`0xC061`). Exact generated values are not stable;
the current export is authoritative.

Codec Parameters reads its startup defaults directly from the generated parameter
image. This avoids maintaining a second hand-written default table. Replacing the SigmaStudio
export therefore updates both the codec's initial image and the values used to decide whether
the persistent image must be restored.

## I2C and failures

Writes use:

```text
[sub-address MSB] [sub-address LSB] [payload...]
```

Reads use the two-byte sub-address followed by a repeated-start read. The codec
auto-increments its internal address for bursts.

Generated macros discard individual returns, so `adau1787_write()` latches the first
nonzero error in `adau_init_error`; later success does not clear it. The sequence continues
and initialization fails at the final check. The latch is not reset on a later init call.
Current setup and final programming failures pass through `ERR_CHK_MSG` and call `k_oops()`.

## Stream stop and diagnostics

`audio_system_stop()` stops the pipeline but does not reset or power down the codec. A new
stream restarts I2S without reloading the DSP image. Full programming normally runs once
per boot.

`adau1787_log_status_2()` can decode power, serial-port, ASRC, undervoltage, and PLL status,
but startup does not currently poll these bits or use them as readiness gates.

Troubleshoot in this order:

1. Enabled devicetree node, address, and GPIO assignments.
2. Physical `!PD` low-to-high transition and MP3-MP6 low levels.
3. I2C controller, pins, and bitrate.
4. The 100 ms reset-release wait and generated 35 ms delay.
5. Latched I2C write failures.
6. BCLK/LRCK appearing only after the streaming event.

Replace `src/SigmaStudioFiles/` as one export and follow the
[pristine-build workflow](../development/workflow.md). Put wiring in devicetree, driver
timing/error behavior in `adau1787.c`, adapter behavior in `SigmaStudioFW.h`, and lifecycle
ordering in Codec Controller and Codec Parameters.
