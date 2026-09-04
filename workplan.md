# Tiresias MVP Workplan

## End goal

An engineer can install Tiresias Workstation, discover and connect to a supported Tiresias
DK, select any N1–N7 or S1–S3 profile, apply it with an unambiguous result, and repeat the
workflow without restarting the workstation or board.

This MVP is an engineering and characterization tool, not a clinical fitting system. The
bundled profiles must not be presented as clinically validated for an individual user.

## Scope

### Included

- One trusted workstation and one retained Tiresias DK peer.
- Device discovery, pairing, connection, identity, compatibility, and status.
- Ten fixed profiles: N1–N7 and S1–S3.
- Catalog-based parameter validation.
- Bounded, cancellable, whole-profile transfer with a truthful terminal result.
- Automated protocol/workstation tests and developer-run hardware validation.
- Windows, macOS, and Linux packaging.

### Deferred

- Audiogram entry or import and runtime fitting generation.
- CAMEQ, NAL-NL2, arbitrary DSP editing, and patient data.
- Firmware-update UI, persistent profiles, multiple peers, and clinical roles.
- Auracast Broadcast Assistant UI and BASS integration.

## Sources of truth

- `tiresias-firmware`: GATT/wire protocol, device capabilities, validation, and state.
- `tiresias-workstation`: product workflow, UI, and desktop behavior.
- `tiresias-eval`: source provenance for profile assets only; it is not a runtime or build
  dependency after import.
- Shared protocol constants and golden vectors prevent silent cross-repository drift.

## Current baseline

- Firmware already provides shared Bluetooth initialization, connectable advertising, ACL
  lifecycle, advertising restart, DIS, Codec Controller ownership, ADAU1787 access, and
  safeload writes of up to five parameter words.
- Workstation already provides a PySide6 shell, BLE discovery and connection, a background
  asyncio worker, Bleak transport, connection state, and fake/offscreen tests.
- The custom service, authorization, status, catalog, profile assets, transfer transaction,
  and application UI are not yet connected end to end.

## Working model

Goals are ordered by dependency, but subgoals may proceed in parallel when their contracts
are stable. Each goal should leave a demonstrable vertical result. Revisit later details as
hardware measurements and usability feedback become available.

Whole profiles must use a bounded transaction. Replaying independent parameter writes
cannot provide one authoritative result or safe recovery after interruption.

## Goal 1 — Agree on the MVP contract

### Subgoals

- [ ] **1.1** Confirm the included/deferred scope and whether applied profiles are volatile.
- [ ] **1.2** Define board state after apply, disconnect, cancellation, codec reset, reboot, and
  workstation exit.
- [ ] **1.3** Define device discovery identity, service/characteristic UUIDs, permissions, protocol
  versions, capabilities, layout/catalog identity, Status, and result codes.
- [ ] **1.4** Define framing, byte order, transaction/session IDs, chunking, flow control, integrity,
  timeout, duplicate, cancellation, reconnect, and partial-apply behavior.
- [ ] **1.5** Define the physical-presence pairing window, authorized bond, bond replacement, and
  recovery procedure. Review MCUmgr access under the same threat model.
- [ ] **1.6** Create shared valid and malformed golden vectors and a protocol revision check.

### Complete when

- [ ] **1.7** Firmware and workstation protocol documents agree.
- [ ] **1.8** A worked transaction covers success, cancellation, disconnect, timeout, incompatible
  layout, bad integrity, insufficient capacity, and codec failure.

## Goal 2 — Establish an authorized, read-only device session

### Firmware subgoals

- [ ] **2.1** Stabilize Control Link event filtering, advertising restart, connection ownership,
  bounded delivery, and one-peer/resource assertions.
- [ ] **2.2** Add the custom service and advertise its UUID.
- [ ] **2.3** Implement `DISABLED`, `ADVERTISING`, `LINKED`, `READY`, and `ERROR`, including pairing,
  encryption, authorization, reconnect, and bond recovery.
- [ ] **2.4** Expose Protocol Information and a coherent, rate-limited Status snapshot without
  taking ownership from other subsystems.

### Workstation subgoals

- [ ] **2.5** Keep Qt presentation, application coordination, domain models, protocol adapter, and
  Bleak transport separate.
- [ ] **2.6** Discover the service, connect, authorize, read DIS/Protocol Information/Status, and
  show compatibility and session state.
- [ ] **2.7** Add actionable errors and structured, redacted diagnostics.
- [ ] **2.8** Cover scan, connection races, link loss, cancellation, and shutdown with fake transport
  and offscreen UI tests.

### Complete when

- [ ] **2.9** Shared vectors pass in C and Python.
- [ ] **2.10** A developer verifies discovery, pairing, reconnect, identity, status, notification
  recovery, and bond removal on a DK.

## Goal 3 — Prove cataloged parameter access

### Shared subgoals

- [ ] **3.1** Define one manifest tied to the production SigmaStudio layout: stable IDs, addresses,
  sizes, encodings, bounds, units, access flags, reserved ranges, and layout identity.
- [ ] **3.2** Generate the firmware lookup table and immutable wire catalog from that manifest.
- [ ] **3.3** Reject duplicate IDs, overlap, reserved ranges, bad alignment, unsupported encoding,
  invalid bounds, and oversized output during generation.

### Firmware subgoals

- [ ] **3.4** Expose the complete catalog with offset reads and integrity metadata.
- [ ] **3.5** Add bounded Request, Response, and Event paths with transaction correlation.
- [ ] **3.6** Implement one outstanding `GET_PARAMETER` / live-safe `SET_PARAMETER` operation through
  Codec Controller, with cancellation and independent device-side validation.

### Workstation subgoals

- [ ] **3.7** Read and validate the catalog for the current boot/session.
- [ ] **3.8** Add correlated request handling and a minimal developer-facing read/write proof.
- [ ] **3.9** Keep raw-address access out of the public API and UI.

### Complete when

- [ ] **3.10** Shared vectors and fake adapters cover success, rejection, timeout, cancellation,
  disconnect, and codec errors.
- [ ] **3.11** A developer reads, writes, and reads back one safe parameter with audio active and no
  observed interruption.

## Goal 4 — Produce the ten runtime profile assets

### Subgoals

- [ ] **4.1** Pin the validated N1–N7 and S1–S3 source revision and production DSP layout.
- [ ] **4.2** Define a deterministic asset format with stable profile/parameter IDs, display metadata,
  compatible layout, payload length, integrity value, and provenance.
- [ ] **4.3** Build an importer that resolves upstream data to stable catalog IDs and rejects missing,
  duplicate, unexpected, unsupported, or incompatible values.
- [ ] **4.4** Generate all ten assets and an aggregate manifest reproducibly.
- [ ] **4.5** Load assets through a platform-neutral `PrescriptionCatalog`; validate compatibility
  before selection or application.
- [ ] **4.6** Verify packaged artifacts contain every asset and need no source checkout.

### Complete when

- [ ] **4.7** All ten assets pass schema, integrity, catalog coverage, and layout checks.
- [ ] **4.8** Every shipped asset is traceable to its source, generator revision, and DSP layout.

## Goal 5 — Apply a whole profile safely in firmware

### Transfer subgoals

- [ ] **5.1** Support one active transfer session with fixed storage, bounded queues, explicit flow
  control, progress, `BEGIN`, chunks, `COMMIT`, `ABORT`, and status.
- [ ] **5.2** Validate authorization, versions, layout/catalog identity, declared parameter set,
  size, integrity, codec state, and capacity before apply.
- [ ] **5.3** Reject invalid offsets, gaps, conflicting duplicates, stale sessions, excess data, and
  integrity mismatch deterministically.

### Apply subgoals

- [ ] **5.4** Stage and preflight the entire profile in device-owned storage before the first codec
  write.
- [ ] **5.5** Serialize validation and application through Codec Controller.
- [ ] **5.6** Define and implement the safe mute/quiesce, apply, verification, restore, and revision
  update sequence.
- [ ] **5.7** Provide rollback to the previous image or a known baseline. If partial application is
  possible, report it explicitly with a documented recovery path.
- [ ] **5.8** Release all resources after completion, abort, disconnect, timeout, authorization loss,
  codec reset, or shutdown.
- [ ] **5.9** Keep management work below audio deadlines and measure stack, memory, queue, latency,
  and underrun behavior.

### Complete when

- [ ] **5.10** Unit/fake-codec tests cover parsing, boundaries, flow control, preflight, rollback,
  interruption, malformed input, and resource exhaustion.
- [ ] **5.11** A developer applies, verifies, interrupts, recovers, and reapplies one full profile on
  hardware without silent success.

## Goal 6 — Complete the workstation workflow

### Application subgoals

- [ ] **6.1** Implement one application use case for readiness checks, asset validation, negotiation,
  chunking, flow control, progress, commit, terminal response, cancellation, and cleanup.
- [ ] **6.2** Allow one conflicting operation at a time and make cancellation idempotent.
- [ ] **6.3** On reconnect, reread protocol and device state; never infer completion from cached UI
  state or an ATT acknowledgement.

### UI subgoals

- [ ] **6.4** Provide Devices, Board, DSP Profiles, and Diagnostics views.
- [ ] **6.5** Show all ten profiles with description, provenance, selection, compatibility, and
  board-reported active state.
- [ ] **6.6** Enable Apply only for an authorized, compatible, ready device; confirm the target and
  engineering-use boundary.
- [ ] **6.7** Show phase, confirmed progress, cancellation, terminal result, and actionable recovery.
- [ ] **6.8** Keep the UI responsive and diagnostics correlated and redacted.

### Complete when

- [ ] **6.9** Stateful fake-device tests cover all profiles, repeated switching, every cancellation
  point, reconnect, compatibility failures, transfer failures, rollback, and partial apply.
- [ ] **6.10** Offscreen UI tests cover navigation, accessibility, progress, errors, and clean shutdown.

## Goal 7 — Validate and release the MVP

### Conformance subgoals

- [ ] **7.1** Run byte-for-byte firmware/workstation tests using the shared vectors and compatibility
  matrix.
- [ ] **7.2** Inject lost, delayed, duplicate, reordered, disconnected, rebooted, unauthorized,
  I2C-failed, mismatched-readback, and exhausted-resource cases.
- [ ] **7.3** Prove the workstation reports success only after the firmware's correlated terminal
  result and recovers truthfully after every failure.

### Developer-run validation

- [ ] **7.4** Record board, firmware, workstation, asset manifest, layout, OS, adapter, and negotiated
  MTU for each run.
- [ ] **7.5** Apply every profile once and cycle through all ten repeatedly.
- [ ] **7.6** Interrupt transfers at early, middle, pre-commit, and apply stages.
- [ ] **7.7** Check local/broadcast audio coexistence, transients, mute time, underruns, synchronization,
  connection stability, codec state, and characterization results.
- [ ] **7.8** Validate the complete workflow on Windows, macOS, and Linux.

These hardware, build, flash, and platform checks are performed manually by the developer.

### Packaging and release subgoals

- [ ] **7.9** Produce reproducible, versioned, signed/notarized packages as required, including Qt,
  Bleak, and all profile assets.
- [ ] **7.10** Smoke-test clean installations with no source checkout or development environment.
- [ ] **7.11** Document installation, permissions, pairing, compatibility, profile provenance,
  cancellation, terminal results, recovery, and the non-clinical boundary.
- [ ] **7.12** Archive automated and manual validation evidence and release matching firmware,
  workstation, catalog, and asset versions together.

### MVP complete when

- [ ] **7.13** The end-goal workflow passes on the supported DK and desktop platforms, or explicit
  release limitations are approved.
- [ ] **7.14** No known defect can cause silent success, undetected partial application, unsafe
  parameter acceptance, or unrecoverable pairing/transfer state.

## Post-MVP goals

- Flexible catalog-based DSP inspection, batch snapshots, and characterization views.
- Audiogram import and pluggable fitting engines such as CAMEQ or NAL-NL2.
- Versioned, power-loss-safe profile persistence and expanded authorization roles.
- Standard services such as VCS/HAS where applicable.
- BASS/Scan Delegator integration and Broadcast Assistant UI.
- Simultaneous control, BASS, scanning, PA synchronization, and BIS stress validation.

## Ongoing engineering rules

- Preserve firmware and workstation ownership boundaries.
- Keep callbacks bounded and nonblocking; use fixed storage and ordered queues.
- Preserve public APIs unless a reviewed requirement demands coordinated change.
- Keep protocol, implementation, tests, and concise documentation synchronized.
- Never treat UI state or an ATT acknowledgement as proof of codec/profile completion.
- Never log secrets or expose uncataloged codec memory.
- Keep automated tests independent of a physical BLE adapter; reserve builds, flashing,
  hardware tests, and platform validation for the developer.
