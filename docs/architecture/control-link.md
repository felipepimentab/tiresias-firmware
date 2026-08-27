# Control Link and Remote Management

## Status

Implemented MVP: shared Bluetooth initialization, connectable advertising, one peripheral
ACL, Device Information Service, the custom Tiresias service, fixed-contract parameter
access, complete parameter-image persistence, advertising restart, and LED 1 indication.

## Service boundaries

| Service | Responsibility |
|---|---|
| Tiresias Control Link | Product-specific commands, status, events, and fixed DSP parameters |
| Device Information Service | Standard static identity; implemented |
| BASS | Standard Broadcast Assistant procedures |
| MCUmgr/SMP | Firmware update; do not duplicate in the custom service |
| VCS/HAS/PACS/CSIS and others | Adopt when their standard semantics match a feature |
| Battery Service | Future hardware only; current board cannot measure battery level |

Use one custom service with focused characteristics. Add another vendor service only for a
materially different lifecycle, authorization boundary, or sustained data rate.

## Ownership

Control Link owns the remote session, negotiation, authorization, wire-protocol parsing,
transaction IDs, response delivery, remote status aggregation, contract identification, and
disconnect cancellation.

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
`0001` as listed below; `0003` is reserved after removal of the protocol-v1 catalog:

| Characteristic | Properties | Content |
|---|---|---|
| Protocol Information (`0002`) | Read | Protocol version, capabilities, limits, contract CRC, boot ID, revision |
| Status (`0004`) | Read, Notify | Control state, persistence state, and last operation |
| Request (`0005`) | Write | One correlated byte-offset operation |
| Response (`0006`) | Indicate | Correlated opaque-byte result |

Wire values have explicit widths and byte order; never expose native C structures. Major
versions break compatibility, minor versions add compatible fields, and capabilities gate
optional operations. A client reads Protocol Information before modifying state.

All integers are little-endian. Wire layouts are fixed and encoded field by field:

| Value | Size | Layout |
|---|---:|---|
| Protocol Information | 24 | `u8 major, u8 minor, u16 length, u32 capabilities, u16 max_request, u16 max_response, u32 contract_crc, u32 boot_id, u32 revision` |
| Request | 12 | `u8 opcode, u8 flags, u32 transaction_id, u8 parameter_id, u8 byte_offset, u8 data[4]` |
| Response | 16 | `u8 opcode, u8 result, u32 transaction_id, u8 parameter_id, u8 byte_offset, u8 data[4], u32 revision` |
| Status | 16 | `u8 state, u8 flags, u8 last_result, u8 reserved, u32 revision, u32 last_transaction_id, u8 last_parameter_id, u8 last_byte_offset, u16 reserved` |

Protocol constants and result values are public in `tiresias_service.h`. Transaction ID zero,
nonzero flags, and nonzero GET data are invalid. CCC, readiness, malformed-length, and busy
failures are rejected at ATT admission; every accepted request completes with one indication
using the same nonzero transaction ID while the session remains connected. Protocol v4 uses
the single DSP contract fingerprint CRC32 `0x22045c5c`.

## Fixed DSP contract

Firmware and workstation compile the same MVP contract. Its public fingerprint is the CRC32
of the ordered four-byte entries `parameter_id, block_id, byte_count, flags`. Membership
implies readable opaque bytes; the writable property is an opt-in flag. Names and GUI grouping
live in the workstation contract, while DSP addresses remain private to firmware. Neither side
assigns numerical meaning, byte order, ranges, or units to parameter contents.

The fixed parameters are ADC Select, Source Select, eight 136-byte compressor LUTs, three
phase-compensation gains, Output Headroom Gain, and the 180-byte Soft Clip LUT. Selectors and
gains are writable byte arrays. LUTs are readable and remain read-only until an atomic
multi-chunk write protocol exists.

The client identifies a chunk with `(parameter_id, byte_offset)`. It reads four opaque bytes at
each offset from zero through `byte_count - 1` to assemble a parameter. Each chunk receives its
own transaction ID and correlated indication; revision must remain stable across the assembled read.

Normal requests use stable IDs. Control Link validates framing, readiness, size, and queue
capacity. Codec Parameters treats payloads as bytes and performs only access and
structural bounds checks required for safe access.

MVP constraints:

- one outstanding parameter operation;
- up to four opaque parameter bytes per correlated `GET_PARAMETER` or `SET_PARAMETER` request;
- the workstation exposes writes for the six fixed writable byte arrays; all 1,292 catalog bytes
  are stored in separate fixed-size parameter buffers owned by Codec Values;
- each parameter is persisted independently as raw bytes under its own stable-ID Zephyr
  Settings key; a successful SET saves only the complete parameter that owns the changed bytes;
- Codec Parameters coordinates loading and saving, while Codec Settings owns the Zephyr Settings
  keys and Codec Values owns the RAM buffers;
- startup zero-fills the parameter buffers and overlays independently stored parameters.
  Missing entries remain zero-filled;
- the protocol revision is boot-local and is not persisted in flash;
- parameter contents have no firmware or workstation interpretation;
- no BLE raw RAM/register access, batch atomicity, or whole-profile replacement.

Codec parameter I/O is outside the proof-of-concept parameter path. Reads use the parameter
buffers and writes update flash and RAM without attempting codec access. Future hardware access
will go through Codec Adapter, the common boundary directly above the ADAU1787 driver. The
service advertises deferred DSP application until that adapter is implemented and validated.

A device-provided dynamic catalog and a generator based on the SigmaStudio `.params` export
are post-MVP improvements. Compatibility negotiation for a dynamic catalog can be designed if
that direction is pursued; DSP addresses must remain off the BLE interface.

Future bulk transfers use a session ID, transaction ID, contract fingerprint, declared parameter set,
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
Link or Audio Streaming may call first. Codec Parameters initializes the idempotent
Zephyr Settings backend and loads individual DSP settings directly. The current build assigns
advertising set 0 only to Control Link. New advertising clients require an allocator/composer
and updated controller limits.

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
It still validates structural framing, array bounds, sizes, queue capacity, and transaction IDs
on-device, but treats parameter bytes supplied by the workstation as opaque. Production authorization,
semantic validation, roles, audit records, and raw-memory maintenance access remain future work.

Audio deadlines take priority over management throughput. Keep buffers fixed, queues
bounded, notifications rate-limited, and concurrency at one parameter operation until
stack, queue, controller-memory, and underrun measurements justify expansion.

## Delivery order

1. Implemented: fixed contract, Status, indexed RAM reads, complete-image persistence, and the
   three-location synchronization lifecycle.
2. Next: Codec Adapter parameter-write implementation and hardware validation.
3. Optional dynamic contract generation, bulk writes, and whole-profile persistence.
4. BASS/Scan Delegator integration and simultaneous control/broadcast testing.
5. Production roles, audit-safe records, stress tests, and recovery hardening.

References: [BASS 1.0.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/BASS_v1.0.1/out/en/index-en.html),
[BAP](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/16212-BAP-html5/out/en/index-en.html),
[Zephyr LE Audio](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/audio/bluetooth-le-audio-arch.html).
