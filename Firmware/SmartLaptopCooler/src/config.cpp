/*
 * config.cpp - Single source of truth for all tunable constants.
 *
 * Timing bounds, packet validation ranges, EMA smoothing factors, the base fan
 * curve breakpoints, emergency threshold, predictive-boost tuning, and safe-
 * mode fallback PWM. Tweak values here only; the matching externs in config.h
 * are read across main.cpp, fan_control.cpp, packet.cpp, and display.cpp.
 */
#include "config.h"

const unsigned long TIMEOUT_MS = 10000;      // ms of no BT data before safe mode engages (fail-cool)

const float TEMP_MIN_VALID  = 0.0;           // °C — reject CPU/GPU temps below this as garbage
const float TEMP_MAX_VALID  = 120.0;         // °C — reject CPU/GPU temps above this as garbage
const float POWER_MIN_VALID = 0.0;           // W — reject CPU power readings below this
const float POWER_MAX_VALID = 60.0;          // W — reject CPU power readings above this
const float SENSOR_MISSING  = -1.0;          // sentinel the PC app sends when a sensor can't be read

const float ALPHA_TEMP  = 0.2;               // EMA weight for temp (0-1, higher = less smoothing)
const float ALPHA_POWER = 0.3;               // EMA weight for power (0-1)
const float ALPHA_RATE  = 0.15;              // EMA weight for rate-of-change (0-1, lower = steadier)

const uint8_t RAMP_STEP = 5;                 // PWM units stepped per loop when |target-current| is small

const uint8_t IDLE_PWM        = 76;          // fan duty when drive temp is below TEMP_RAMP_START (~30% of 255)
const float   TEMP_RAMP_START = 50.0;        // °C — temp at which the fan curve leaves idle and starts ramping
const uint8_t RAMP_START_PWM  = 76;          // PWM at TEMP_RAMP_START (matches IDLE_PWM)
const float   TEMP_RAMP_MID   = 55.0;        // °C — mid curve breakpoint; from here the ramp steepens
const uint8_t RAMP_MID_PWM    = 179;         // PWM at TEMP_RAMP_MID (~70% duty)
const float   TEMP_RAMP_FULL  = 60.0;        // °C — temp at/above which fan hits full speed
const uint8_t RAMP_FULL_PWM   = 255;         // PWM at TEMP_RAMP_FULL (100% duty)

const float SOFT_TARGET_TEMP = 50.0;         // °C — informational: the temp the curve is roughly tuned to hold

const float EMERGENCY_TEMP = 65.0;           // °C — if smoothed CPU or GPU temp >= this, force full fan

const float   TEMP_RATE_BOOST_GAIN = 40.0;   // PWM added per °C/s of temperature rise above threshold
const uint8_t TEMP_RATE_BOOST_CAP  = 60;     // max PWM added by the temp-rate boost

const uint8_t MIN_TARGET_PWM = 26;           // floor clamp for targetPWM (target never goes below this, ~10%)

const float   RISE_RATE_THRESHOLD   = 0.4;   // °C/s — only add 'T' boost when temp rises faster than this
const uint8_t RISE_BOOST_PWM        = 50;    // (legacy/unused — kept for reference; previously the flat T-boost amount)
const float   POWER_RATE_THRESHOLD  = 2.0;   // W/s — only add 'P' boost when power spikes faster than this
const uint8_t POWER_RATE_BOOST_PWM  = 40;    // PWM added flatly when the 'P' (power-rate) condition fires
const float   POWER_LEVEL_THRESHOLD = 12.0;  // W — only add 'L' boost when smoothed power is above this
const uint8_t POWER_LEVEL_BOOST_PWM = 30;    // PWM added flatly when the 'L' (power-level) condition fires

const uint8_t SAFE_MODE_PWM = 204;           // fallback fan duty when BT data is lost (~80%)
