# Zbus Channels

Zbus carries low-rate commands, events, and state mirrors. ISO, LC3, PCM, and I2S data use
dedicated data-plane transports.

## Rules

- A command requests behavior; the receiving subsystem validates it and owns all effects.
- Authoritative state is private. A state channel is only its latest-value mirror.
- Only the owning subsystem publishes its state, after an accepted transition.
- Private and mirrored initial values must match.
- Use a queued transport for bursts, ordered events, external requests, or correlated
  results. Do not publish commands back-to-back on a latest-value channel.
- Publication failure leaves private state authoritative and is handled as a reporting
  failure.
- A rejected command does not imply `ERROR` or `FAULT`.

The initial PoC uses state changes as lifecycle completion reports. Correlation, deadlines,
retries, and stale-result detection can be added without changing state ownership.

## Public control-plane channels

| Channel | Publisher | Subscriber or observer | Contract |
|---|---|---|---|
| `button_chan` | Button Input | Device Controller | Debounced press events |
| `led_chan` | Control Link (`LED_1`), Codec Controller (`LED_2`), Audio Streaming (`LED_3`) | LED Indicator | `TURN_ON`, `TURN_OFF`, `TOGGLE`, `BLINK` |
| Device Controller command | Main startup; future normalized Control Link requests | Device Controller | Whole-device lifecycle; `START` is implemented |
| Device Controller state | Device Controller | Snapshot readers | `OFF`, `INITIALIZING`, `OPERATIONAL`, `LOW_POWER`, `FAULT` |
| Codec Controller command | Device Controller | Codec Controller | `INITIALIZE`, `SELECT_LOCAL`, `SELECT_BROADCAST`; power/reset reserved |
| Codec Controller state | Codec Controller | Device Controller; snapshot readers | `OFF`, `INITIALIZING`, `LOCAL_ONLY`, `BROADCAST_ONLY`, `ERROR` |
| Control Link command | Device Controller | Control Link | `ENABLE_CONTROL`; disable/reset reserved |
| Control Link state | Control Link | Device Controller or snapshot readers | Current foundation: `DISABLED`, `ADVERTISING`, `CONNECTED`, `ERROR`; target splits connected into `LINKED` and `READY` |
| Audio Streaming command | Device Controller | Audio Streaming | `START_SCAN` and `STOP`; other lifecycle commands reserved |
| Audio Streaming state | Audio Streaming | Device Controller and Codec Controller | Discovery and PA/BIS state; drives broadcast availability |

Result-event channels for Codec Controller, Control Link, and Audio Streaming are reserved
for outcomes that do not change durable state; no PoC subscriber currently consumes them.

## Remote requests

Externally originated GATT requests require bounded ordered queues because they may overlap,
retry, and require exact errors.

1. The callback copies an admitted request to the Control Link inbound queue.
2. Control Link routes device policy to Device Controller or parameters to Codec Controller.
3. Results retain the remote transaction ID and explicit terminal status.
4. An outbound queue isolates subsystem work from GATT backpressure.

V1 single-parameter values may use bounded messages. Bulk transfers require fixed buffers
with explicit ownership. Never pass stack or callback-owned pointers across contexts. See
[control-link.md](control-link.md).

## Bluetooth event fan-out

Bluetooth Management alone publishes `bt_mgmt_chan`. Control Link and Audio Streaming each
use a Zbus message subscriber, so each receives its own ordered copy.

| Consumer | Accepted events |
|---|---|
| Control Link | Advertising result for set 0 and lifecycle for its peripheral connection index |
| Audio Streaming | PA and broadcast-reception events |

Switch on the event type before reading event-specific fields. ACL pointers are borrowed;
retain scalar indexes for correlation or take an explicit Bluetooth reference. A zero
return from `bt_mgmt_adv_start()` confirms queue admission only; publish `ADVERTISING`
after `BT_MGMT_EXT_ADV_STARTED`.
