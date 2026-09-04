# Button Input

Owns button GPIO setup, debounce, and publication of semantic button events.

## Files and configuration

- `src/modules/button_handler.c`: GPIO, debounce, queue, and publisher.
- `src/modules/button_handler.h`: initialization and polling API.
- `include/zbus_common.h`: event and message types.
- Devicetree must define `sw0` with a `gpios` property.

## API

```c
int button_handler_init(void);
int button_pressed(gpio_pin_t button_pin, bool *button_pressed);
```

Use `button_chan` for normal event handling. Use `button_pressed()` only when the current
physical level is required.

## Runtime contract

1. The GPIO ISR rejects events while debounce is active.
2. It maps the pin to a `btn_event_t` and enqueues `btn_chan_msg_t` without blocking.
3. `button_publish_thread` publishes the event on `button_chan`.
4. A timer re-enables input after `CONFIG_BUTTON_DEBOUNCE_MS`.

To add a button, add its event to `btn_event_t`, add its devicetree-backed entry to
`buttons[]`, and keep policy in subscribers rather than the ISR.
