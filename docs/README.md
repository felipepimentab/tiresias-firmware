# Documentation

Quick references are grouped by scope.

## Architecture

- [System context](architecture/system-context.md): product and PoC boundaries.
- [Control plane](architecture/control-plane.md): subsystem ownership and states.
- [Control Link](architecture/control-link.md): BLE management protocol design.
- [Threads and contexts](architecture/threads-and-contexts.md): execution and priority rules.
- [Zbus](architecture/zbus.md): channel ownership and delivery contracts.
- [Hardware](architecture/hardware.md): board-level blocks and interfaces.
- [Audio messages](architecture/audio-application.md): current message ownership.

## Development

- [Workflow](development/workflow.md): SigmaStudio and generated-file rules.

## Modules

- [ADAU1787 startup](modules/adau1787-startup.md)
- [Bluetooth Management](modules/bluetooth-management.md)
- [Button Input](modules/button.md)
- [LED Indicator](modules/led.md)

## Writing guidelines

- Put system boundaries and cross-subsystem decisions in `architecture/`.
- Put repository-wide workflows in `development/`.
- Put source contracts and extension notes in `modules/`.
- Keep documents brief, actionable, and linked from this index.
