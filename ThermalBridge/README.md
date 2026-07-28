# ThermalBridge

Windows companion app for the Smart Laptop Cooler. Reads live CPU/GPU temperature and power draw using [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor), and streams it to the [ESP32 firmware](../Firmware) over Bluetooth SPP.

⚠️ **Work in progress.**

## Features

- Reads CPU package temperature, CPU package power, and GPU core temperature via LibreHardwareMonitorLib
- Auto-detects the Smart Laptop Cooler's COM port — scans available ports, verifies device identity via a `PING`/`THERMALBRIDGE` handshake, no manual COM port configuration required after first run
- Remembers the last known-good COM port (`lastcomport.txt`) for fast reconnects
- Automatic reconnection if the Bluetooth link drops
- Full sensor dump on startup for diagnostics

## Requirements

- Windows 10/11
- .NET 10 SDK (to build) or the .NET 10 runtime (to run a pre-built release)
- **Administrator privileges** — required for LibreHardwareMonitorLib to access hardware sensors
- A paired Smart Laptop Cooler device (see [firmware setup](https://github.com/Hans930v/Smart-Laptop-Cooler/blob/Firmware/README.md))

## Building

```bash
cd ThermalBridge
dotnet build
```

Requires:
```bash
dotnet add package LibreHardwareMonitorLib
dotnet add package System.IO.Ports
```

## Running

Run `ThermalBridge.exe` **as Administrator** (right-click → Run as administrator, or elevate via Task Scheduler — see below).

On first run:
1. It will dump all detected hardware sensors — confirm CPU Package temp/power and GPU Core temp are found (`NOT FOUND` means your specific CPU/GPU reports sensors under different names; check the dump and adjust `ReadAll()` in `Program.cs` accordingly)
2. It will scan for the Smart Laptop Cooler and confirm its identity before connecting
3. The COM port is saved to `lastcomport.txt` for faster reconnects on future runs

## Running automatically at startup

Since this needs to run continuously with admin rights, the recommended approach is **Windows Task Scheduler**:

1. Open Task Scheduler → **Create Task**
2. General tab: check **"Run with highest privileges"**
3. Triggers tab: **New → At log on**
4. Actions tab: **New → Start a program** → point to `ThermalBridge.exe`
5. Conditions tab: uncheck **"Start the task only if the computer is on AC power"** (important for laptops)
6. Settings tab: enable **"If the task fails, restart every: 1 minute"** for resilience

## Known limitations

- Sensor names (`"CPU Package"`, `"GPU Core"`) are matched by string and were confirmed against an 11th Gen Intel Core i5-1135G7 + NVIDIA GeForce MX330. Other CPU/GPU combinations may report sensors under different names — check the startup sensor dump if values show `NOT FOUND`.
- Bluetooth SPP communication is not encrypted or authenticated beyond a basic identity handshake and standard OS-level Bluetooth pairing. Not intended for use cases requiring strong security guarantees.
- Closing the console window (X button) may not terminate the process immediately if it's mid-operation; use Ctrl+C or Task Manager if needed.

## License

AGPL-3.0 — see [LICENSE](../LICENSE) in the repo root.

This project uses [LibreHardwareMonitorLib](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor), licensed under MPL-2.0.****
