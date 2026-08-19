# System Context

## Intended system

```mermaid
flowchart TB
    audiologist([Audiologist]) --> fitting[Fitting system]
    fitting -->|Clinical baseline and limits| hearing[Tiresias hearing aid]
    fitting -->|Reviewed fitting| phone[User phone]
    phone -->|Bounded personalization and broadcast assistance| hearing
    hearing -->|Status and telemetry| phone
    phone -->|EMA and device evidence| fitting
    source[Auracast source] -->|Broadcast audio| hearing
    environment[Acoustic environment] -->|Local sound| hearing
    user([User]) -->|Physical controls| hearing
    user -->|Goals and ratings| phone
    hearing -->|Presented audio and feedback| user
```

- The audiologist owns the clinical baseline and safe adjustment envelope.
- The user makes reversible adjustments inside that envelope.
- The phone provides control, Broadcast Assistant behavior, updates, and data relay.
- The fitting system stores and presents longitudinal evidence for review.

## Current proof of concept

```mermaid
flowchart TB
    researcher([Researcher / audiologist]) --> workstation[Development workstation]
    workstation -->|Configuration and control| prototype[Tiresias prototype]
    prototype -->|Evidence and diagnostics| workstation
    source[nRF5340 Audio DK] -->|Development LE Audio broadcast| prototype
    environment[Acoustic environment] -->|Local sound| prototype
    participant([Participant]) -->|Physical adjustment| prototype
    participant -->|Ratings and reports| workstation
    prototype -->|Presented audio and feedback| participant
```

The workstation currently combines the future phone and fitting-system roles. The nRF5340
Audio DK is a development broadcaster, not an Auracast implementation.

## System boundary

Inside the Tiresias system:

- Tiresias DK hardware and nRF5340 firmware;
- ADAU1787 codec, DSP configuration, microphones, and output transducer;
- local controls, indicators, and power management.

People, phones, workstations, broadcasters, tools, and the acoustic environment are
external. See [control-link.md](control-link.md) for BLE control and Broadcast Assistant
boundaries.
