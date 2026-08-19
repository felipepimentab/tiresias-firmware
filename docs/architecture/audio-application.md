# Audio Message Ownership

| Channel | Producer | Owner or consumer |
|---|---|---|
| `le_audio_chan` | Configured broadcast, source, or unicast implementation | Audio Streaming |
| `bt_mgmt_chan` | Bluetooth Management | Control Link and Audio Streaming |
| `sdu_ref_chan` | LE Audio transmit path | Audio data plane |
| `volume_chan` | Bluetooth rendering/capture adapter | Future Codec Controller or volume owner |
| `cont_media_chan` | Bluetooth Content Control adapter | Future device/media policy owner |
