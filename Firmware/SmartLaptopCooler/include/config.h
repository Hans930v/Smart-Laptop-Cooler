/*
 * config.h - Extern declarations for all tunable firmware constants.
 *
 * Pin definitions live here as #defines; everything else is an extern whose
 * value is set in config.cpp. Includes timing bounds, packet validation limits,
 * EMA smoothing factors, base fan curve breakpoints, predictive-boost tuning,
 * emergency threshold, and safe-mode fallback PWM.
 */
#pragma once
#include <Arduino.h>

#define LED_PIN     2
#define FAN_PWM_PIN 4

extern const unsigned long TIMEOUT_MS;

extern const float TEMP_MIN_VALID;
extern const float TEMP_MAX_VALID;
extern const float POWER_MIN_VALID;
extern const float POWER_MAX_VALID;
extern const float SENSOR_MISSING;

extern const float ALPHA_TEMP;
extern const float ALPHA_POWER;
extern const float ALPHA_RATE;

extern const uint8_t RAMP_STEP;

extern const uint8_t IDLE_PWM;
extern const float   TEMP_RAMP_START;
extern const uint8_t RAMP_START_PWM;
extern const float   TEMP_RAMP_MID;
extern const uint8_t RAMP_MID_PWM;
extern const float   TEMP_RAMP_FULL;
extern const uint8_t RAMP_FULL_PWM;

extern const float SOFT_TARGET_TEMP;

extern const float EMERGENCY_TEMP;

extern const float   TEMP_RATE_BOOST_GAIN;
extern const uint8_t TEMP_RATE_BOOST_CAP;

extern const uint8_t MIN_TARGET_PWM;

extern const float   RISE_RATE_THRESHOLD;
extern const uint8_t RISE_BOOST_PWM;
extern const float   POWER_RATE_THRESHOLD;
extern const uint8_t POWER_RATE_BOOST_PWM;
extern const float   POWER_LEVEL_THRESHOLD;
extern const uint8_t POWER_LEVEL_BOOST_PWM;

extern const uint8_t SAFE_MODE_PWM;
