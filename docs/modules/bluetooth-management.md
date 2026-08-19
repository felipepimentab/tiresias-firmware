# Bluetooth Management

Shared mechanism layer for Zephyr Bluetooth initialization, controller setup, physical
procedures, callbacks, and event fan-out. Product policy and state remain in Control Link
and Audio Streaming.

## Initialization

`bt_mgmt_init()` initializes the host, settings, optional test address/bonds, controller
configuration, connection callbacks, and advertising worker.

- A mutex allows one initialization attempt.
- Concurrent callers wait and receive the cached first result.
- Failure is sticky until reboot to avoid repeating partial global setup.
- `-EALREADY` from `bt_enable()` is accepted; remaining project setup still runs.
- The completion callback records and releases the result; the caller owns error policy.

Control Link and Audio Streaming may call in either order.

## ACL events

Bluetooth Management alone publishes physical ACL events on `bt_mgmt_chan`.

| Field | Use |
|---|---|
| `event` | Selects the valid payload fields |
| `peripheral` | Filters the local ACL role |
| `index` | Long-lived lifecycle correlation |
| `conn` | Borrowed pointer for immediate handling only |

Control Link accepts only its peripheral ACL and restarts advertising only for the retained
index. Retaining `conn` requires `bt_conn_ref()` and later `bt_conn_unref()`.

Generic callbacks publish lifecycle facts. Control Link owns reconnection policy. Physical
recovery from failed connection establishment or directed-advertising timeout remains in
Bluetooth Management.

## Advertising

`bt_mgmt_adv_start()` retains the advertising-data pointers, queues a set index, and
returns. Zero means admitted, not started.

| Event | Meaning |
|---|---|
| `BT_MGMT_EXT_ADV_STARTED` | Indexed set started |
| `BT_MGMT_EXT_ADV_FAILED` | Indexed set failed; includes error |
| `BT_MGMT_EXT_ADV_WITH_PA_READY` | Indexed set and periodic advertising ready |

The current build assigns set 0 to Control Link and uses static advertising data. Audio
Streaming uses observer and periodic-sync procedures and does not mutate this set. A new
advertising client requires an explicit allocator/composer and updated limits.

## Delivery and concurrency

Control Link and Audio Streaming have separate Zbus message subscribers. Each receives an
ordered copy and filters it:

| Consumer | Owns |
|---|---|
| Control Link | Set 0 results and its peripheral ACL lifecycle |
| Audio Streaming | Scan, PA, broadcast code, and LE Audio lifecycle |

Switch on `event` before reading event-specific fields. Both callbacks and the advertising
worker publish without blocking. The message pool is bounded; advertising-result overflow
is logged, while some inherited paths still escalate through `ERR_CHK`. Completion
timeouts and a unified nonfatal overflow policy remain future work.

## Lifecycle summary

Startup: both subsystems request shared initialization; Control Link submits set 0 and
enters `ADVERTISING` only after its started event.

Disconnect: both subscribers receive the indexed event; Audio Streaming ignores it;
Control Link restarts only for its retained peer and returns to `ADVERTISING` after the new
started event.

See [Control Link](../architecture/control-link.md) for the custom service and policy.
