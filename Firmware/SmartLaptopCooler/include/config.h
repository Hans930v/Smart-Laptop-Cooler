#pragma once
#include <Arduino.h>

// ===== Pin config =====
#define LED_PIN     2
#define FAN_PWM_PIN 25

// ===== Timing =====
extern const unsigned long TIMEOUT_MS;

// ===== Packet validation bounds =====
extern const float TEMP_MIN_VALID;
extern const float TEMP_MAX_VALID;
extern const float POWER_MIN_VALID;
extern const float POWER_MAX_VALID;
extern const float SENSOR_MISSING;

// ===== Smoothing (EMA) =====
extern const float ALPHA_TEMP;
extern const float ALPHA_POWER;
extern const float ALPHA_RATE;

// ===== PWM ramping =====
extern const uint8_t RAMP_STEP;

// ===== Base fan curve (temperature -> base PWM) =====
// Aggressive "stay cool, fan noise OK" curve.
// Stays quiet until TEMP_RAMP_START, then ramps HARD to keep temps near SOFT_TARGET.
//   t < TEMP_RAMP_START        -> IDLE_PWM
//   TEMP_RAMP_START..MID        -> IDLE_PWM .. MID_PWM   (gentle warm-up)
//   MID..FULL                   -> MID_PWM  .. FULL_PWM   (hard ramp to pin temps)
//   t >= FULL                   -> FULL_PWM  (max fan)
extern const uint8_t IDLE_PWM;
extern const float   TEMP_RAMP_START;
extern const uint8_t RAMP_START_PWM;
extern const float   TEMP_RAMP_MID;
extern const uint8_t RAMP_MID_PWM;
extern const float   TEMP_RAMP_FULL;
extern const uint8_t RAMP_FULL_PWM;

// Soft target temperature the curve is designed to hold (informational).
extern const float   SOFT_TARGET_TEMP;

// ===== Emergency / runaway protection =====
// If either smoothed temp >= EMERGENCY_TEMP, fan goes straight to 255
// (bypasses ramping) to catch thermal runaways the curve can't keep up with.
extern const float   EMERGENCY_TEMP;

// ===== Temp-rate predictive boost =====
// Scaled proportional boost: extra PWM = (rate - THRESHOLD) * GAIN, capped at CAP.
// Lower THRESHOLD = triggers on smaller temp rises; higher GAIN/CAP = bigger kick.
extern const float   TEMP_RATE_BOOST_GAIN;
extern const uint8_t TEMP_RATE_BOOST_CAP;

// ===== Target PWM clamp floor =====
// Fan target is never allowed below this (excluding ramping-down dynamics).
extern const uint8_t MIN_TARGET_PWM;

// ===== External cooler compensation tuning =====
extern const float   RISE_RATE_THRESHOLD;
extern const uint8_t RISE_BOOST_PWM;
extern const float   POWER_RATE_THRESHOLD;
extern const uint8_t POWER_RATE_BOOST_PWM;
extern const float   POWER_LEVEL_THRESHOLD;
extern const uint8_t POWER_LEVEL_BOOST_PWM;

// ===== Safe mode =====
extern const uint8_t SAFE_MODE_PWM;