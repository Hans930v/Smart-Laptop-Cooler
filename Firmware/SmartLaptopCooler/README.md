# Smart Laptop Cooler — Firmware

ESP32 firmware for the Smart Laptop Cooler. Receives live CPU/GPU temperature and power data from [ThermalBridge](../ThermalBridge) over Bluetooth SPP, and drives an external cooling fan with predictive, lag-compensated PWM control.

⚠️ **Work in progress.** Fan hardware (MOSFET driver stage) has not yet been fully validated end-to-end.

## Features

- Bluetooth Classic SPP communication (paired as `Smart Laptop Cooler`)
- Identity handshake (`PING` → `THERMALBRIDGE`) so the PC-side app can reliably auto-detect the correct COM port
- Packet validation — rejects out-of-range or malformed sensor readings before they can drive the fan
- Exponential moving average (EMA) smoothing on temperature and power readings, to prevent sensor noise from causing jumpy fan behavior
- Rate-of-change compensation — anticipates rising heat using both temperature *and* power draw trends, to compensate for the thermal lag inherent to an external cooler
- Gradual PWM ramping — fan speed transitions smoothly rather than jumping
- Safe mode — if no data is received for 5 seconds (e.g. Bluetooth link drops), the fan holds a safe fallback speed until communication resumes
- Optional SH1106 128x32 OLED status display (I2C, via U8x8 text mode)

## Hardware required

| Component | Notes |
|---|---|
| ESP32 dev board | Any ESP32 with Bluetooth Classic support |
| 12V fan | 2-pin DC fan |
| MOSFET/motor driver | Logic-level N-channel MOSFET (e.g. F5305S) recommended for direct 3.3V GPIO drive. IRF520 modules work if they include an onboard gate-boost transistor — check before assuming. DRV8871 also works but is unnecessary overkill for a single-direction fan. |
| 12V power supply | Sized to your fan's rated draw |
| (Optional) SH1106 128x32 OLED | I2C |

## Wiring

**Fan driver (MOSFET, single-direction):**
- Fan+ → 12V supply
- Fan− → MOSFET Drain
- MOSFET Source → GND (shared with ESP32 GND)
- MOSFET Gate → ESP32 GPIO 25 (PWM)

**OLED (optional, I2C):**
- VCC → 3.3V
- GND → GND
- SDA → GPIO 21
- SCL → GPIO 22

## Dependencies

- ESP32 Bluetooth Classic (`BluetoothSerial`)
- `U8g2` library (used here in U8x8 text mode for a smaller flash footprint)

## First-time setup

1. Flash the firmware
2. Power on the ESP32 — the onboard LED will slow-blink while unpaired
3. On your Windows PC: **Settings → Bluetooth & devices → Add device → Bluetooth**, select `Smart Laptop Cooler`
4. Enter PIN if prompted (try `1234` or `0000`; many BT SPP pairings complete without one)
5. Run [ThermalBridge](../ThermalBridge) — it will auto-detect the correct COM port and begin streaming sensor data
6. Open Serial Monitor at **115200 baud** to view diagnostic output

## Tuning

Fan response can be adjusted via constants near the top of the source file:

| Constant | Purpose |
|---|---|
| `RISE_RATE_THRESHOLD` | °C/sec of temperature rise that triggers a predictive PWM boost |
| `POWER_RATE_THRESHOLD` | W/sec of power draw increase that triggers a predictive PWM boost |
| `POWER_LEVEL_THRESHOLD` | Sustained power draw (W) that raises the PWM floor |
| `RAMP_STEP` | Max PWM change per loop cycle — lower = smoother but slower to react |
| `ALPHA_TEMP` / `ALPHA_POWER` / `ALPHA_RATE` | EMA smoothing factors — lower = smoother but slower to respond to real changes |

These are workload- and hardware-dependent. Expect to tune them against your own usage patterns.

## License

AGPL-3.0 — see [LICENSE](../LICENSE) in the repo root.
