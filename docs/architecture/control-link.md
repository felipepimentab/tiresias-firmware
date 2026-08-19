# Control Link and Remote Management

## Status

Implemented foundation: shared Bluetooth initialization, connectable advertising, one
peripheral ACL, Device Information Service, advertising restart, and LED 1 indication.

Planned: custom Tiresias service, authorized sessions, status, parameter catalog, codec
parameter access, and BASS integration. UUIDs and byte-level encoding are not yet defined.

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
| `LINKED` | ACL present; custom session not authorized |
| `READY` | Secure, authorized, negotiated session |
| `ERROR` | Persistent local Control Link failure |

- Enter `ADVERTISING` only after the indexed advertising-start event.
- A peripheral ACL moves to `LINKED`; authorization and negotiation move to `READY`.
- Disconnect cancels peer-owned work and requests advertising restart.
- Malformed, unauthorized, busy, or rejected requests return protocol errors; they do not
  enter `ERROR`.
- The current DIS-only foundation uses `CONNECTED`; split it before custom writes.

## Custom service

| Characteristic | Properties | Content |
|---|---|---|
| Protocol Information | Read | Versions, capabilities, limits, layout ID, boot/session ID |
| Parameter Catalog | Read with offsets | Stable IDs, codec addresses, formats, bounds, units, flags |
| Status | Read, Notify | Coherent subsystem and broadcast summary |
| Request | Write | Framed command or transaction control |
| Response | Indicate | Correlated terminal result and structured error |
| Event | Notify | Sequenced unsolicited events |
| Transfer Data | Future write/notify | Credited chunks for dumps, batches, and profiles |

Wire values have explicit widths and byte order; never expose native C structures. Major
versions break compatibility, minor versions add compatible fields, and capabilities gate
optional operations. A client reads Protocol Information before modifying state.

An ATT write response means the request entered a bounded queue, not that it completed.
The Response indication uses the same transaction ID. Event sequence gaps require a Status
reread. Status contains low-rate state and fault summaries; never include audio data,
unbounded logs, credentials, or Broadcast Codes.

## Parameter catalog

The build-time catalog is the complete V1 allowlist. Each entry includes:

- stable ID and current ADAU1787 byte address;
- word count, encoding, scale, and byte order;
- bounds, optional step or enum choices, and units;
- readable, writable, volatile, and live-update-safe flags.

Generate the remote catalog and firmware lookup table from the same source associated with
the SigmaStudio export. The firmware must not parse a host-provided map. Build failure is
required if the immutable V1 catalog cannot fit the supported characteristic-value limit;
never truncate it.

Normal requests use stable IDs. Control Link validates framing, authorization, layout ID,
size, and queue capacity. Codec Controller independently resolves the entry and validates
state, access, type, range, address, alignment, and safety before I2C access.

V1 constraints:

- one outstanding parameter operation;
- one parameter per correlated `GET_PARAMETER` or `SET_PARAMETER` request;
- only cataloged, live-update-safe writes;
- volatile changes applied through safeload;
- no raw RAM/register access, persistence, batch atomicity, or whole-profile replacement.

The current safeload path handles at most five four-byte words per group. Larger or unsafe
entries are read-only until a maintenance path is designed. Persistent profiles require a
separate versioned, integrity-checked, power-loss-safe store and explicit commit.

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
Link or Audio Streaming may call first. The current build assigns advertising set 0 only
to Control Link. New advertising clients require an allocator/composer and updated
controller limits.

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

V1 authorizes one bonded research workstation on an encrypted connection opened through a
physical-presence pairing window. Still validate all IDs, encodings, bounds, permissions,
sizes, rates, queues, and timeouts on-device. Cancel work on disconnect or authorization
loss, and never log secrets. Wearer, clinician, developer roles, and raw memory access are
future authorization classes.

Audio deadlines take priority over management throughput. Keep buffers fixed, queues
bounded, notifications rate-limited, and concurrency at one parameter operation until
stack, queue, controller-memory, and underrun measurements justify expansion.

## Delivery order

1. Custom read-only service, authorized session, catalog, and Status.
2. Versioned Request/Response/Event framing and volatile single-parameter access.
3. Optional bulk transfer, maintenance apply, and persistent profiles.
4. BASS/Scan Delegator integration and simultaneous control/broadcast testing.
5. Production roles, audit-safe records, stress tests, and recovery hardening.

References: [BASS 1.0.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/BASS_v1.0.1/out/en/index-en.html),
[BAP](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/16212-BAP-html5/out/en/index-en.html),
[Zephyr LE Audio](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/audio/bluetooth-le-audio-arch.html).
