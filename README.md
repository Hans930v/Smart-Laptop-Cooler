# Smart Laptop Cooler or SLC

⚠️ **Work in progress.** Core logic is implemented and being tested, but fan hardware has not yet been fully validated end-to-end. Not recommended for use yet — expect breaking changes.

## What this is

A Bluetooth-connected external laptop cooler that reads live CPU/GPU temperature and power draw from Windows (via [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor)) and drives an ESP32-controlled fan accordingly — with rate-of-change compensation to account for external cooler thermal lag.

## Components

- **ThermalBridge** (`/ThermalBridge`) — C# Windows console app. Reads sensors, streams data to the ESP32 over Bluetooth SPP.
- **Firmware** (`/firmware`) — ESP32 (Arduino) code. Receives sensor data, computes fan PWM with smoothing and predictive compensation, drives an external fan via MOSFET.

## Status

- [x] Sensor reading (CPU/GPU temp, CPU power)
- [x] Bluetooth SPP communication with auto COM-port detection
- [x] Reconnect handling
- [x] Fan control logic (smoothing, rate-of-change compensation, safe mode)
- [ ] Fan hardware validation (pending MOSFET driver)
- [ ] Graceful app shutdown
- [ ] Full setup documentation

## License

AGPL-3.0. See [LICENSE](LICENSE).

This project uses [LibreHardwareMonitorLib](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor), licensed under MPL-2.0.

## Author

Hansoy — [github.com/Hans930v](https://github.com/Hans930v)
