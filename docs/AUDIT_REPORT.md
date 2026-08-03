# WaterTankSensor audit report

Audit updated for the 0.11 firmware working tree.

## Persistence inventory

All primary settings use the `tank` Preferences namespace. Missing keys use the
listed defaults; `Settings::reset()` clears this namespace and reloads defaults.
Existing key names were retained for compatibility.

| Area | Key(s) and type | Default / validation | Web integration |
|---|---|---|---|
| Wi-Fi | `ssid`, `pass` String; `dhcp` bool | empty credentials; DHCP on | `wifiSSID`, password is write-only, `networkMode` |
| Manual network | `staticIp`, `gateway`, `subnet`, `dns1`, `dns2` String | empty; validated as IPv4 before save | matching `wifi*` fields/placeholders |
| Access point | `apSsid`, `apPass`, `apIp`, `apGateway`, `apSubnet` String | compile-time AP defaults; address/password rules checked before save | `ap*`; password is write-only and no secret placeholder remains |
| Web login | `webLogin` bool, `webUser`, `webPass` String, `webTimeout` ushort | off, `admin`, empty, 30 min; username 1–32, password 4–64 when enabled, timeout 1–1440 | `webLoginEnabled`, `webUsername`, write-only password, timeout |
| MQTT | `mqtt`, `user`, `mpass` String; `port` ushort; `mqttEnabled` bool | empty, 1883, disabled; host/port checked before save | matching `mqtt*`; password is write-only |
| Device/tank | `device` String; `tank`, `clearance` float; `interval` ulong; `deepSleep` bool | hostname; configured tank defaults; positive tank, nonnegative clearance, interval 1–86400 s; deep sleep enabled | matching fields and template values; `deepSleepEnabled` checkbox |
| Time | `ntpEnabled` bool; `timeZone`, `ntp1..3` String; `ntpTimeout` uchar | on, `UTC0`, documented servers, 8 s; timeout 1–30 | matching NTP fields/placeholders |
| Battery | `batEmpty`, `batFull` float; `batCapacity` ulong; `batChem` String; `batCells` uchar; `batteryTest` bool | 3.20/4.20 V, 2000 mAh, custom, 1, off; finite ordered voltages, capacity 1–100000, at least one cell | matching battery fields/placeholders |
| Pins | `pinButton`, `pinStatus`, `pinRed`, `pinGreen`, `pinBlue`, `pinBattery`, `pinTrigger`, `pinEcho` uchar | compile-time defaults; board allowlists, ADC1 requirement and collision checks | Info-page selects and generated pin placeholders |

Additional namespaces are intentional:

| Namespace | Keys | Lifecycle |
|---|---|---|
| `batteryEst` | sample count, learning seconds, start/last voltage | bounded learned state; reset through its dedicated action and factory reset path |
| `deviceTime` | last successful NTP sync | restored only when plausible; refreshed after successful sync |
| `recovery` | one-shot configuration-AP flag | set by recovery/factory-reset flow and consumed on boot |

GPIO 33 remains fixed for recovery/wake and is not a configurable stored key.
MQTT topics and Discovery status are derived/runtime state, not persisted settings.
Unknown NVS keys are not individually erased during normal operation.

## Route and authentication audit

Public endpoints are `/login`, `/style.css`, `/favicon.ico`, and captive-portal
probe routes. `/`, `/info`, `/logs`, `/status`, `/api/history`, and `/api/logs`
pass through the central authentication check. All state-changing routes use
POST and check authentication before changing state: `/save`, `/save-pins`,
`/restore-default-pins`, `/measure`, `/reset-battery-estimate`,
`/mqtt-discovery`, `/clear-history`, `/clear-logs`, `/sleep`, `/restart`,
`/firmware`, and `/logout`. The firmware page and multipart upload both use the
same central authentication/session policy.

Protected browser pages redirect to `/login`; APIs return JSON 401 and protected
actions return 401 text. Sessions use an opaque random token, `HttpOnly`, and
`SameSite=Strict`; credentials and tokens are not logged. Redirect targets are
restricted to local safe paths. Recovery/configuration AP bypass remains
intentional to prevent lockout.

## Automated and static checks

`test/test_core_logic/test_main.cpp` covers nominal, boundary, clamped, and
invalid tank and battery calculations. `scripts/validate_web.ps1` checks form
nesting, duplicate IDs, literal JavaScript ID references, form/route agreement,
local assets, placeholders/password placeholders, and CSS brace balance.

The native Unity suite requires a host C/C++ compiler. PlatformIO installed the
native platform and Unity, but this workstation has no `gcc`/`g++`; therefore
the suite was added but not reported as passed.

## Functional test matrix

| Area | Build/static coverage | Requires actual hardware or infrastructure |
|---|---|---|
| Startup/configuration | defaults, validation, bounded waits and recovery paths manually inspected; firmware compiled | first boot, corrupt NVS, filesystem failure |
| Measurement | pure calculation cases implemented as Unity tests; invalid readings inspected | ultrasonic success/timeout/impossible echo |
| Battery | percentage calculation cases implemented; ADC/divider code inspected | ADC calibration, divider accuracy, learning over sleep cycles |
| Web | routes/forms/placeholders/assets statically checked | browser interaction, live session timeout, response timing |
| Authentication | every route and session invalidation path manually inspected | end-to-end login/logout/credential-change/recovery-AP test |
| Network | IPv4 validation and bounded connection logic inspected | DHCP/manual reconnect, fallback AP |
| MQTT | enable/reconnect/manual Discovery, bounded retry and payload construction inspected | broker publish/retain and Home Assistant Discovery |
| Button/power | thresholds, release-to-reset, unsigned subtraction, wake paths inspected | 5/15/30-second boundaries, timer/button wake, current draw |
| History/logging | bounds, JSON escaping, corruption/empty-file handling inspected | filesystem-full and interrupted-write fault injection |

## Findings and remaining risks

- Fixed a pending-log queue out-of-bounds write after a failed filesystem flush.
- Removed dead JavaScript for an element no longer present.
- Removed the rendered factory AP password; secret fields remain write-only.
- Tank and battery percentage logic is now shared, finite-input checked, clamped,
  and host-testable without changing nominal results.
- The sensor driver performs real JSN-SR04T pulse/echo acquisition with a
  bounded timeout and range validation.
- Battery cell count is persisted/displayed but is not applied to the voltage
  thresholds or ADC conversion. The present divider/calibration assumptions
  appear suitable for a single-cell range; multi-cell hardware interpretation
  must be specified before changing behavior.
- File replacement for history/log persistence is bounded and checked, but
  power-loss fault injection was not performed.
- Firmware OTA uses the inactive application partition and reports validation
  failures without scheduling a restart. Hardware OTA fault injection remains a
  manual test.
