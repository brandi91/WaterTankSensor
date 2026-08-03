# Changelog

## [Unreleased]

## [0.11.0] - 2026-08-04

- Added authenticated firmware uploads through the web interface.
- Added verified JSN-SR04T operation on GPIO 23 (trigger) and GPIO 22 (echo).
- Added charger detection on GPIO 34 and moved battery ADC input to GPIO 35.
- Fixed scheduled measurements in both deep-sleep and persistent web modes.
- Changed Home Assistant MQTT expiry to measurement interval plus 10 percent.
- Refresh Home Assistant discovery automatically after MQTT connects.
- Standardized the web interface and source comments on English.
- Removed dead transmission code and disabled the release-build LED self-test.
