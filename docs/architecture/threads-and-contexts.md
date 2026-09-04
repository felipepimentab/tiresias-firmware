# Threads and Execution Contexts

A subsystem is an ownership boundary; a thread is a scheduling mechanism. Use a dedicated
context for blocking I/O, resource serialization, distinct latency, or continuous data
processing.

## Control-plane contexts

| Context | Responsibility |
|---|---|
| Main thread | Device Controller subscriber loop and system policy |
| Codec Controller thread | ADAU1787 lifecycle, mode changes, and serialized I2C |
| Control Link thread | BLE control policy, authorization, request routing, and responses |
| Audio Streaming thread | Scan, PA/BASE/BIG/BIS lifecycle, and recovery |
| Button worker | Publish queued ISR events |
| LED worker/timer | Apply indication commands and blink LEDs |

`main()` publishes `START` after board initialization and enters the Device Controller
loop. Control Link and Audio Streaming may independently call the mutex-protected shared
Bluetooth initializer; correctness must not depend on which runs first.

Control Link records outstanding asynchronous work instead of blocking. Codec requests use
ordered, bounded request/result queues; only Codec Controller accesses the ADAU1787.

## Data-plane contexts

```text
Bluetooth ISO callback
    -> ISO receive FIFO
    -> audio datapath thread (LC3 and PCM)
    -> I2S ISR/DMA buffers
    -> ADAU1787 DSP and output
```

Optional transmit processing uses I2S buffers, a FIFO, and a dedicated encoder thread.
High-rate sensor or telemetry data also needs a queue or fixed-buffer path, not control
state channels.

## Context rules

| Context | Do | Defer |
|---|---|---|
| GPIO/I2S ISR | Capture state, exchange fixed buffers, enqueue | I2C, allocation, policy, long logging |
| Bluetooth callback | Validate minimum framing, copy/retain data, enqueue | Codec access, parsing, waits, policy |
| Subsystem thread | State transitions, validation, owned blocking operations | Continuous audio processing |
| Data-plane thread | FIFO waits, LC3, PCM production/consumption | Device policy and unrelated peripherals |
| System workqueue | Short cleanup, retry, or timer work | Long blocking or steady high-rate work |

Work submitted without a private queue runs on Zephyr's shared system workqueue. Keep it
short so unrelated kernel and application work is not delayed.

## Priority and sizing

Relative urgency:

1. Hardware and Bluetooth controller deadlines.
2. Audio data-plane workers.
3. Audio Streaming lifecycle.
4. Codec Controller and Control Link bounded work.
5. Device policy, UI, telemetry, shell, and diagnostics.

In Zephyr, lower numeric values mean higher preemptive priority. Measure stack high-water
marks and queue bursts during startup, reconnection, recovery, and heavy logging. Size for
the measured worst case plus margin; sleeping threads consume stack RAM but negligible CPU.

## Current status

The main, Codec Controller, Control Link, and Audio Streaming contexts are implemented for
the local-audio and BIS-reception path. Legacy `controller`, `audio_control`, and
`bluetooth` sources remain uncompiled reference code. Audio datapath, encoder, button, and
LED workers remain separate.
