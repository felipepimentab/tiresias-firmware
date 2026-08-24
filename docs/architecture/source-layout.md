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
        -> Codec Adapter
            -> ADAU1787 driver
                -> Zephyr I2C and GPIO
```

- The ADAU1787 driver owns devicetree access, GPIO and power/reset sequencing,
  raw register access, device timing, and generic chip mechanisms.
- Codec Adapter is the common hardware-operation boundary above the ADAU1787
  driver. Modules outside the driver layer use it for parameter RAM and future
  codec operations.
- Codec Controller owns lifecycle, command validation, serialized access,
  presentation state, fallback policy, and error handling.
- The DSP parameter catalog owns the fixed contract, private addresses, and value
  constraints. The parameter controller owns the parameter lifecycle: loading
  defaults and stored values, validation, codec synchronization, persistence
  decisions, fallback behavior, and revisions. The DSP parameter settings adapter
  only serializes and retrieves the versioned flash record through Zephyr Settings.

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
