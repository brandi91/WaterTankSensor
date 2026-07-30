# Configurable deep-sleep audit

Baseline: `8982303` on branch `test`. Firmware version for this change: `0.9.7`.

## Persistence and behavior

The public setting and form field are `deepSleepEnabled`, defaulting to `true`.
ESP32 NVS limits key names to 15 characters, so the physical key in the existing
`tank` namespace is `deepSleep`. A missing key loads as enabled and factory reset
clears it so the enabled default is restored.

When enabled, the established automatic sequence remains measurement, bounded
network work, persistence, and real deep sleep. When disabled, a completed
automatic cycle starts the existing web service and remains awake. A
`millis()`-safe scheduler performs the next measurement at the configured
interval without delaying the main loop. Manual web or button measurements do
not postpone that periodic baseline.

`POST /sleep` remains an authenticated deliberate override. It uses the normal
timer and fixed GPIO 33 wake sources even when automatic sleep is disabled.
Recovery/configuration-AP mode never triggers the normal awake scheduler.

## Controlled entry and isolation

There is exactly one direct `esp_deep_sleep_start()` call:
`SleepManager::sleepForSeconds()` in `src/sleep_manager.cpp`.
`SleepManager::sleepNow()` is the manual wrapper and the normal-cycle helper
calls `sleepForSeconds()` directly.

The ordered entry is:

1. Validate a nonzero interval and emit the final normal diagnostic.
2. Mark runtime `PreparingSleep`.
3. Stop the web server/configuration AP.
4. Disconnect MQTT and Wi-Fi.
5. drive the ultrasonic trigger low and switch LEDs off.
6. flush bounded persistent logs.
7. clear stale wake sources.
8. configure timer wake and fixed GPIO 33 ext0 wake.
9. flush serial output, mark runtime `Sleeping`, and call deep sleep.

No application action is expected after the call. Logger, sensor measurement,
history insertion, battery-estimator sampling, MQTT connect/loop/publish, Wi-Fi
connect/loop, and NTP synchronization all reject new normal work once sleep
preparation starts. Optional disconnect/flush failures do not loop indefinitely.

Actual ESP32 deep sleep stops CPU/Arduino-loop and normal FreeRTOS execution;
Wi-Fi, MQTT, filesystem writes, normal logging, and sensor measurement therefore
cannot continue until a wake source boots the firmware again. This hardware
behavior is distinct from the statically verified preparation guards.

## Timer, task, and callback inventory

| Source | Awake behavior | Sleep-preparation behavior |
|---|---|---|
| Arduino `loop()` / `millis()` schedulers | button, battery, Wi-Fi, web, MQTT, LED and periodic measurement maintenance | top-level guard returns without normal work |
| WebServer callbacks | synchronous route handlers | server stopped; no delayed normal action is created |
| Delayed web sleep/restart | checked in `WebServerManager::loop()` | manual sleep consumes its flag before central entry |
| MQTT callback | handled synchronously by PubSubClient loop | MQTT loop is guarded, then disconnected |
| Wi-Fi reconnect scheduler | bounded `millis()` retry | loop/connect guarded, then radio disconnected |
| Button state machine | polled, no interrupt | main loop guard prevents new action dispatch |
| Sensor loop | currently no scheduled work | measurement entry itself is guarded |
| Hardware timer / Ticker | none created by project code | not applicable |
| Custom FreeRTOS task/timer | none created by project code | framework tasks are not manually deleted |
| Project interrupt handler | none registered | ext0 is wake hardware, not an awake callback |

The current sensor implementation is simulated; its trigger pin is nevertheless
initialized low and explicitly returned low before sleep. Status and RGB outputs
are turned off. GPIO hold is not enabled because the current wiring does not
demonstrate a need for it. Fixed GPIO 33 and its active-low wake level are
unchanged.

## Automated logic coverage

Native Unity cases cover:

- automatic sleep enabled/disabled;
- manual override;
- recovery suppression;
- normal-work rejection during preparation/sleep;
- scheduler before/at the interval;
- zero interval;
- unsigned `millis()` rollover.

The firmware build compiles these shared helpers. Native execution still
requires a host `gcc`/`g++`, which was not present on the audit workstation.

## Hardware verification checklist

### A — automatic sleep enabled

1. Boot and complete one measurement.
2. Confirm the final preparation message occurs once.
3. Verify Wi-Fi disconnect and serial silence until wake.
4. Verify no history, log, or MQTT updates during the interval.
5. Verify one timer wake and exactly one subsequent cycle.

### B — automatic sleep disabled

1. Disable and save the setting.
2. Verify the device remains awake and the web UI stays reachable.
3. Verify scheduled non-overlapping measurements at the configured interval.
4. Verify there is no automatic reboot and MQTT remains maintainable.

### C — manual override

1. Keep automatic sleep disabled and invoke `/sleep`.
2. Verify real deep sleep and timer/GPIO 33 wake.

### D — recovery

1. Wake sleeping hardware with fixed GPIO 33.
2. Verify unchanged recovery behavior and recovery-AP login bypass.

### E — electrical proof

Use a suitable current meter or power analyzer and record awake idle,
measurement, Wi-Fi/MQTT, and deep-sleep current. Missing serial output alone is
not proof of deep sleep; low measured current is the strongest confirmation.

No hardware procedure in this document was executed as part of the host audit.
The optional `DEEP_SLEEP_AUDIT_DIAGNOSTICS` retained marker is disabled by
default and does not add wake sources or flash writes.
