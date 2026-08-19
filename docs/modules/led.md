# LED Indicator

Owns LED GPIOs and applies commands received on `led_chan`.

## Files and configuration

- `src/modules/led.c`: GPIOs, subscriber, commands, and blink timer.
- `src/modules/led.h`: `int led_init(void)`.
- `include/zbus_common.h`: `board_led_t`, `led_cmd_t`, and `led_chan_msg_t`.
- Devicetree must provide enabled `led0`, `led1`, and `led2` aliases with `gpios`.

## Ownership

| LED | Publisher | Meaning |
|---|---|---|
| `LED_1` | Control Link | Blink: advertising; on: connected; off: disabled/error |
| `LED_2` | Codec Controller | Current presentation indication |
| `LED_3` | Audio Streaming | Blink: scanning; on: PA/BIS synchronized; off otherwise |

Each subsystem publishes only for its assigned LED. Shared ownership requires an explicit
priority or composition policy.

## Commands

- `TURN_ON` / `TURN_OFF`: set the level and stop blinking.
- `TOGGLE`: toggle once.
- `BLINK`: add the LED to the blink mask.

The worker validates each message and updates the GPIO or blink mask. The timer toggles the
mask every `BLINK_FREQ_MS` and stops when the mask is empty.

To add an LED, add its devicetree alias and static table entry, extend `board_led_t`, and
keep `N_LEDS` derived from `ARRAY_SIZE(leds)`.
