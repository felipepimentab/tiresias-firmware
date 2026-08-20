# Source Layout

The source tree currently mixes subsystem-oriented Tiresias code with the
technical-layer layout inherited from the nRF5340 Audio application. Preserve
working inherited code, but organize new and substantially changed code around
clear ownership boundaries.

## Codec boundary

Use the following dependency direction:

```text
Device Controller
    -> Codec Controller
        -> hardware-codec adapter / DSP profile
            -> ADAU1787 driver
                -> Zephyr I2C and GPIO
```

- The ADAU1787 driver owns devicetree access, GPIO and power/reset sequencing,
  raw register access, device timing, and generic chip mechanisms.
- The hardware-codec adapter owns SigmaStudio image loading, exported parameter
  addresses, and semantic operations such as local/I2S selection and mute.
- Codec Controller owns lifecycle, command validation, serialized access,
  presentation state, fallback policy, and error handling.
- The DSP parameter catalog owns the allowlist and value constraints; the parameter
  controller owns settings serialization, persistence, and revisions; the apply adapter is
  the only boundary that may eventually write cataloged values to DSP hardware.

Move Codec Controller and `hw_codec` out of `src/audio` and `src/modules` into a
dedicated `src/codec_controller` directory, or `src/codec` if that name is
explicitly reserved for the hardware-codec domain. Keep `adau1787.*` under
`src/drivers` and generated exports under `src/SigmaStudioFiles`.

## Other directories

- Keep shared Bluetooth mechanisms under `src/bluetooth`. If the subsystem
  files grow, group them into nested `control_link/` and `audio_streaming/`
  directories without duplicating shared Bluetooth management or LE Audio code.
- Keep `src/audio` focused on the PCM, LC3, I2S, and audio data plane.
- Gradually replace the inherited `modules` and `utils` catch-alls: move audio
  mechanisms to `audio`, board and UICR support to `platform` or `board`, storage
  features to `storage`, and UI peripherals to an appropriate owned component.

Perform these moves incrementally when the affected code is already changing.
Directory count is not a design goal; explicit ownership and dependency direction
are more important than a large structure-only refactor.
