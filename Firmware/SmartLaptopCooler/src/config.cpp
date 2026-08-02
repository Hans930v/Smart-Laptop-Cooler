#include "config.h"

// ===== Bluetooth / connection timing =====
const unsigned long TIMEOUT_MS = 5000; // No data for this long -> trigger safe mode.

// ===== Packet validation bounds =====
const float TEMP_MIN_VALID = 0.0;   // Reject CPU/GPU temp readings below this (°C).
const float TEMP_MAX_VALID = 120.0; // Reject CPU/GPU temp readings above this (°C).
const float POWER_MIN_VALID = 0.0;  // Reject CPU power readings below this (W).
const float POWER_MAX_VALID = 60.0; // Reject CPU power readings above this (W).
const float SENSOR_MISSING = -1.0; // Sentinel value ThermalBridge sends when a sensor wasn't found.

// ===== EMA smoothing factors (0-1, higher = more responsive/less smoothed) =====
const float ALPHA_TEMP = 0.2;  // Smoothing weight for CPU/GPU temp readings.
const float ALPHA_POWER = 0.3; // Smoothing weight for CPU power readings.
const float ALPHA_RATE = 0.15; // Smoothing weight for the rate-of-change (°C/s, W/s) values.

const uint8_t RAMP_STEP = 5; // PWM step per cycle when ramping up/down.

// ===== Base fan curve (ACTIVE VALUES) =====
// Aggressive: idle only when truly cool, ramp hard to hold ~50°C, full speed by 58°C.
const uint8_t IDLE_PWM = 76;          // Below TEMP_RAMP_START, fan sits here.
const float   TEMP_RAMP_START = 45.0; // When to start ramping up from IDLE_PWM.
const uint8_t RAMP_START_PWM = 76;    // PWM at TEMP_RAMP_START.
const float   TEMP_RAMP_MID = 52.0;   // Mid-ramp point; from here we ramp hard.
const uint8_t RAMP_MID_PWM = 200;     // PWM at TEMP_RAMP_MID.
const float   TEMP_RAMP_FULL = 58.0;  // At/above this -> full blast.
const uint8_t RAMP_FULL_PWM = 255;    // PWM at/above TEMP_RAMP_FULL.

const float SOFT_TARGET_TEMP = 50.0; // Informational: temp the curve is tuned to hold.

// ===== Emergency runaway threshold =====
const float EMERGENCY_TEMP = 65.0; // Force 255 if smoothed temp >= this.

// ===== Temp-rate predictive boost =====
// Adds extra PWM when temp is climbing fast, ahead of the base curve catching up.
const float   TEMP_RATE_BOOST_GAIN = 40.0; // PWM per °C/s above RISE_RATE_THRESHOLD.
const uint8_t TEMP_RATE_BOOST_CAP = 60;    // Max predictive boost added per cycle.

// ===== Target PWM clamp floor =====
const uint8_t MIN_TARGET_PWM = 76; // Final targetPWM is clamped >= this.

// ===== External cooler compensation tuning =====
// Extra PWM applied on top of the base curve when temp/power is rising or already high.
const float   RISE_RATE_THRESHOLD = 0.2; // °C/s — was 0.4 (lower = trigger sooner)
const uint8_t RISE_BOOST_PWM = 50;       // PWM added when temp rise rate exceeds threshold (was 35)
const float   POWER_RATE_THRESHOLD = 1.5;   // W/s — was 3.0 (lower = trigger sooner)
const uint8_t POWER_RATE_BOOST_PWM = 40;    // PWM added when power draw is spiking fast (was 25)
const float   POWER_LEVEL_THRESHOLD = 10.0; // W — was 20.0 (lower = trigger sooner)
const uint8_t POWER_LEVEL_BOOST_PWM = 30;   // PWM added when sustained power draw is high (was 20)

// ===== Safe mode =====
const uint8_t SAFE_MODE_PWM = 220; // Fallback PWM when Bluetooth data is lost (was 179).