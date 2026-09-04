# Firmware Control Plane

The control plane is a network of cooperating finite-state machines. Each subsystem owns
one semantic state; the Device Controller coordinates policy without manipulating another
subsystem's resources. ISO, LC3, PCM, and I2S traffic remains in the audio data plane.

## Ownership

| Subsystem | Owns |
|---|---|
| Device Controller | Whole-device lifecycle and startup policy |
| Control Link | Incoming BLE control availability and authorized session |
| Audio Streaming | Broadcast discovery, PA/BIG/BIS synchronization, and recovery |
| Codec Controller | ADAU1787 lifecycle and audible presentation |
| Button Input | Debounced input events |
| LED Indicator | GPIO indication and blink timing |

Bluetooth Management is a shared mechanism, not a product-state subsystem. It owns global
Bluetooth initialization and physical callback fan-out. See
[Bluetooth Management](../modules/bluetooth-management.md).

## State rules

- Authoritative state is private to its owning subsystem.
- The corresponding Zbus channel is a read-only mirror published after accepted changes.
- Only the owner publishes its state.
- Commands request a target behavior; the owner validates and performs the transition.
- A terminal command owns its cleanup sequence. Do not publish several lifecycle commands
  back-to-back on a latest-value channel.
- `ERROR` and `FAULT` represent persistent failures, not rejected commands.

Initial private and mirrored states are: Device Controller `OFF`, Control Link `DISABLED`,
Audio Streaming `DISABLED`, and Codec Controller `OFF`.

## State reference

### Device Controller

| State | Meaning |
|---|---|
| `OFF` | Subsystems and active paths are stopped |
| `INITIALIZING` | Required subsystems are starting |
| `OPERATIONAL` | Required subsystems are ready |
| `LOW_POWER` | Nonessential activity is suspended |
| `FAULT` | A device-level invariant failed |

The PoC implements startup through `OPERATIONAL` and transition to `FAULT`. Do not mirror
codec presentation modes here.

### Control Link

| State | Meaning |
|---|---|
| `DISABLED` | Control interface unavailable |
| `ADVERTISING` | Accepting an incoming ACL |
| `LINKED` | ACL present; Tiresias session not authorized |
| `READY` | Secure, authorized Tiresias session |
| `ERROR` | Persistent local Control Link failure |

The current DIS-only foundation uses `CONNECTED` for the ACL. Add the `LINKED`/`READY`
distinction before accepting custom writes. See [control-link.md](control-link.md).

### Audio Streaming

| State | Meaning |
|---|---|
| `DISABLED` | Receiver unavailable |
| `IDLE` | Initialized, not searching |
| `SCANNING` | Searching for a source |
| `PA_SYNCED` | Periodic advertising synchronized |
| `BIS_SYNCING` | BIG/BIS synchronization in progress |
| `STREAMING` | ISO data available |
| `RECOVERING` | Cleaning up a recoverable failure |
| `ERROR` | Reception requires external recovery |

The PoC starts scanning directly from `DISABLED`, returns to `SCANNING` after recoverable
loss, and implements `STOP`. Control Link and Audio Streaming states are independent.

### Codec Controller

| State | Meaning |
|---|---|
| `OFF` | Codec not initialized or powered down |
| `INITIALIZING` | Reset and programming in progress |
| `LOCAL_ONLY` | Local microphone/DSP path presented |
| `BROADCAST_ONLY` | Received broadcast presented |
| `ERROR` | Initialization or communication failed |

`LOCAL_ONLY` and `BROADCAST_ONLY` are active presentation modes. Leaving Audio Streaming
`STREAMING` forces broadcast presentation back to local. Codec operations stay serialized
inside this subsystem.

## Coordination

Typical broadcast selection:

1. Device Controller requests Audio Streaming `START_SCAN`.
2. Audio Streaming reports `STREAMING` when BIS data is available.
3. Device Controller requests Codec Controller `SELECT_BROADCAST`.
4. Codec Controller reports `BROADCAST_ONLY`.
5. If streaming is lost, Codec Controller selects local audio and reports `LOCAL_ONLY`.

The initial internal model uses state reports for completion. Externally originated
requests require ordered queues, transaction correlation, and explicit terminal results.

## Transport and execution

| Information | Transport |
|---|---|
| Commands, low-rate events, state mirrors | Zbus or bounded subsystem queues |
| Remote parameter requests and bulk management | Ordered queues and fixed buffer pools |
| ISO SDUs and LC3 frames | Dedicated FIFOs |
| PCM and I2S blocks | Fixed buffers and real-time callbacks |

Callbacks capture and enqueue; owning subsystem threads perform policy and blocking I/O.
See [zbus.md](zbus.md) and [threads-and-contexts.md](threads-and-contexts.md).

FOTA uses Zephyr/NCS management infrastructure. Battery monitoring is unavailable on the
current board revision because its signal is connected to a digital-only GPIO.
