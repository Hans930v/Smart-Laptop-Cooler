# include

This directory contains the project's header files for the **Smart Laptop Cooler** ESP32 firmware.

Header files declare shared constants, structs, and function prototypes used across multiple `.cpp` files in `src/`. You include them in a source file with:

```cpp
#include "header.h"
```

## Project headers

| File | Purpose |
|---|---|
| `config.h` | Pin definitions, packet validation bounds, EMA smoothing constants, PWM ramping/boost thresholds, safe mode PWM value |
| `bitmaps.h` | XBM bitmap data for the OLED (brand logo, model logo, Bluetooth connected/disconnected icons) |
| `display.h` | OLED (`u8g2`) instance, display state variables, and draw functions (`drawBrand`, `drawModel`, `drawStatus`, `drawDashboard`) |
| `fan_control.h` | Smoothed sensor values, rate-of-change tracking, PWM ramping/safe mode/temp-curve functions |
| `packet.h` | Bluetooth packet parsing and per-field validation (`PacketValidity`, `validatePacket`, `parsePacket`) |

## Notes on this project's headers

- **`bitmaps.h` is header-only** — its byte arrays are declared `static const`, so each `.cpp` that includes it gets its own private copy. This avoids linker conflicts without needing a matching `bitmaps.cpp`.
- **`config.h`, `fan_control.h`, `packet.h`, `display.h`** declare variables with `extern` where the actual definition lives in the matching `.cpp` file in `src/` (e.g. `extern float smoothedCpuTemp;` in `fan_control.h`, defined in `fan_control.cpp`). This keeps one single source of truth per variable and avoids "multiple definition" linker errors.
- A `DEBUG_SERIAL` toggle exists locally in `main.cpp` (not in these headers) — comment/uncomment the `#define` there to enable or disable verbose Serial logging.

## Why header files

Including a header file produces the same result as copying its declarations into every source file that needs them — without the risk of those copies drifting out of sync. Change a constant or function signature once, in one header, and every file that includes it picks up the change on next recompile.

By convention, header files end in `.h`.

Further reading (official GCC documentation):
- [Include Syntax](https://gcc.gnu.org/onlinedocs/cpp/Include-Syntax.html)
- [Include Operation](https://gcc.gnu.org/onlinedocs/cpp/Include-Operation.html)
- [Once-Only Headers](https://gcc.gnu.org/onlinedocs/cpp/Once-Only-Headers.html)
- [Computed Includes](https://gcc.gnu.org/onlinedocs/cpp/Computed-Includes.html)

https://gcc.gnu.org/onlinedocs/cpp/Header-Files.html