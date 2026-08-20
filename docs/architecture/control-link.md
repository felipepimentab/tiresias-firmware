# Control Link and Remote Management

## Status

Implemented MVP: shared Bluetooth initialization, connectable advertising, one peripheral
ACL, Device Information Service, the custom Tiresias service, cataloged parameter access,
internal-flash persistence, advertising restart, and LED 1 indication. DSP writes are an
explicit no-op boundary until hardware validation is available.

## Service boundaries

| Service | Responsibility |
|---|---|
| Tiresias Control Link | Product-specific commands, status, events, and cataloged parameters |
| Device Information Service | Standard static identity; implemented |
| BASS | Standard Broadcast Assistant procedures |
| MCUmgr/SMP | Firmware update; do not duplicate in the custom service |
| VCS/HAS/PACS/CSIS and others | Adopt when their standard semantics match a feature |
| Battery Service | Future hardware only; current board cannot measure battery level |

Use one custom service with focused characteristics. Add another vendor service only for a
materially different lifecycle, authorization boundary, or sustained data rate.

## Ownership

Control Link owns the remote session, negotiation, authorization, wire-protocol parsing,
transaction IDs, response delivery, remote status aggregation, parameter-catalog exposure,
and disconnect cancellation.

It does not own Bluetooth stack setup, ADAU1787 access, device/audio policy, broadcast
synchronization, BASS state, firmware update, or audio buffers. These belong to Bluetooth
Management, Codec Controller, Device Controller, Audio Streaming, MCUmgr, and the audio
data plane.

Control and broadcast reception are independent. `READY` + `STREAMING`, a control session
without a broadcast, and broadcast reception without a control peer are all valid. V1
supports one incoming control/Broadcast Assistant ACL.

## State model

| State | Meaning |
|---|---|
| `DISABLED` | Connectable control unavailable |
| `ADVERTISING` | Accepting an ACL |
| `LINKED` | ACL present; session admission is in progress |
| `READY` | Tiresias requests are admitted |
| `ERROR` | Persistent local Control Link failure |

- Enter `ADVERTISING` only after the indexed advertising-start event.
- A peripheral ACL moves through `LINKED` to `READY`. The trusted-workstation MVP has no
  authorization gate.
- Disconnect cancels peer-owned work and requests advertising restart.
- Malformed, unauthorized, busy, or rejected requests return protocol errors; they do not
  enter `ERROR`.

## Custom service

The service UUID is `7b9a0001-6e4f-4b2d-a9c8-4f2e6f5d1000`. Characteristic UUIDs replace
`0001` with `0002` through `0006` in table order:

| Characteristic | Properties | Content |
|---|---|---|
| Protocol Information (`0002`) | Read | Versions, capabilities, limits, layout ID, boot ID, revision |
| Parameter Catalog (`0003`) | Read with offsets | Stable IDs, codec addresses, formats, bounds, units, flags |
| Status (`0004`) | Read, Notify | Control state, persistence state, and last operation |
| Request (`0005`) | Write | One correlated parameter operation |
| Response (`0006`) | Indicate | Correlated terminal result |

Wire values have explicit widths and byte order; never expose native C structures. Major
versions break compatibility, minor versions add compatible fields, and capabilities gate
optional operations. A client reads Protocol Information before modifying state.

All integers are little-endian. Wire layouts are fixed and encoded field by field:

| Value | Size | Layout |
|---|---:|---|
| Protocol Information | 32 | `u8 major, u8 minor, u16 length, u32 capabilities, u16 max_request, u16 max_response, u16 entry_size, u16 entry_count, u32 layout_id, u32 catalog_crc, u32 boot_id, u32 revision` |
| Catalog header | 16 | `u8 version, u8 entry_size, u16 count, u16 total_length, u16 reserved, u32 layout_id, u32 entries_crc` |
| Catalog entry | 32 | `u16 id, u8 flags, u8 encoding, u16 address, u8 words, u8 unit, i32 min, i32 max, i32 default, i32 step, char name[8]` |
| Request | 12 | `u8 opcode, u8 flags, u32 transaction_id, u16 parameter_id, i32 value` |
| Response | 16 | `u8 opcode, u8 result, u32 transaction_id, u16 parameter_id, i32 value, u32 revision` |
| Status | 16 | `u8 state, u8 flags, u8 last_result, u8 reserved, u32 revision, u32 last_transaction_id, u16 last_parameter_id, u16 reserved` |

Protocol constants and result values are public in `tiresias_service.h`. Transaction ID zero,
nonzero flags, and nonzero GET values are invalid. CCC, readiness, malformed-length, and busy
failures are rejected at ATT admission; every accepted request completes with one indication
using the same nonzero transaction ID while the session remains connected. The current catalog
golden CRC32 is `0xdfac5b27`.

## Parameter catalog

The build-time catalog is the complete V1 allowlist. Each entry includes:

- stable ID and current ADAU1787 byte address;
- word count, encoding, scale, and byte order;
- bounds, optional step or enum choices, and units;
- readable, writable, persistent, and live-update-safe flags.

The remote catalog and lookup table share one immutable descriptor array tied to the current
SigmaStudio export. The firmware never parses a host-provided map; build assertions protect
the fixed catalog count, DSP word size, address alignment, and wire size.

Normal requests use stable IDs. Control Link validates framing, readiness, size, and queue
capacity. The DSP parameter controller independently resolves the entry and validates access,
range, and step before persistence.

MVP constraints:

- one outstanding parameter operation;
- one parameter per correlated `GET_PARAMETER` or `SET_PARAMETER` request;
- only cataloged, live-update-safe writes;
- every successful SET is committed as one versioned, layout-bound, CRC-checked settings
  record before RAM state and revision advance;
- the parameter controller loads its own settings subtree during initialization, so an empty
  first boot commits catalog defaults and readiness is independent of Bluetooth startup order;
- the exact Q5.23 step is `1/256`; values outside the catalog range or off-step are rejected;
- no raw RAM/register access, batch atomicity, or whole-profile replacement.

The current catalog contains three phase-compensation gains and output headroom. The DSP
adapter is intentionally empty, so success currently means the flash commit completed, not
that hardware changed. Before real DSP writes are enabled, SET must be redesigned as a
transaction that defines apply failure, rollback, and flash/DSP power-loss reconciliation.

Future bulk transfers use a session ID, transaction ID, layout ID, declared parameter set,
offsets, total length, integrity value, credits, and fixed owned buffers. Disconnect,
authorization loss, timeout, shutdown, or codec reset cancels the session with documented
partial-application semantics.

## Firmware integration

| Resource or policy | Owner |
|---|---|
| One-time `bt_enable()`, settings, callbacks, controller setup | Bluetooth Management |
| Advertising set creation/execution and physical event publication | Bluetooth Management |
| Set 0 policy, peripheral ACL, reconnection, Control Link state | Control Link |
| Scan, PA/BASE/BIG/BIS lifecycle | Audio Streaming |
| Startup policy | Device Controller |
| ADAU1787 validation and access | Codec Controller |

`bt_mgmt_init()` is mutex-protected and caches the first result for the boot; either Control
Link or Audio Streaming may call first. Application startup registers the DSP settings handler
before either subsystem can initialize Bluetooth and call `settings_load()`. The current build
assigns advertising set 0 only to Control Link. New advertising clients require an
allocator/composer and updated controller limits.

`bt_mgmt_adv_start()` is asynchronous. Control Link tracks the pending index and changes
state only on `BT_MGMT_EXT_ADV_STARTED`; `BT_MGMT_EXT_ADV_FAILED` reports failure. ACL
correlation uses the copied connection index. Borrowed connection pointers require an
explicit `bt_conn_ref()` before retention.

Bluetooth and GATT callbacks perform minimal validation, copy to bounded queues, and
return. Control Link routes semantic work; owning subsystem threads validate and execute
it. No callback performs I2C, waits for completion, or decides device policy.

## BASS and security

The same ACL may carry the custom service and standard BASS. A Broadcast Assistant selects
a source through BASS; Audio Streaming owns synchronization and updates Broadcast Receive
State; Device Controller decides audibility; Codec Controller changes presentation.

The build enables Scan Delegator capability, but the compiled subsystem path does not yet
initialize or route it. Add BASS solicitation, callback routing, and receive-state updates
without vendor source-selection opcodes.

The MVP trusts one workstation and does not require encryption, bonding, or physical presence.
It still validates IDs, encodings, bounds, sizes, queue capacity, and transaction IDs on-device.
Production authorization, roles, audit records, and raw-memory maintenance access remain
future work.

Audio deadlines take priority over management throughput. Keep buffers fixed, queues
bounded, notifications rate-limited, and concurrency at one parameter operation until
stack, queue, controller-memory, and underrun measurements justify expansion.

## Delivery order

1. Implemented: custom service, catalog, Status, correlated access, and parameter persistence.
2. Next: transactional DSP apply and hardware validation.
3. Optional bulk transfer, maintenance apply, and whole-profile persistence.
4. BASS/Scan Delegator integration and simultaneous control/broadcast testing.
5. Production roles, audit-safe records, stress tests, and recovery hardening.

References: [BASS 1.0.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/BASS_v1.0.1/out/en/index-en.html),
[BAP](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/16212-BAP-html5/out/en/index-en.html),
[Zephyr LE Audio](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/audio/bluetooth-le-audio-arch.html).
