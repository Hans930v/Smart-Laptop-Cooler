/*
 * config.cpp - Single source of truth for all tunable constants.
 *
 * Timing bounds, packet validation ranges, EMA smoothing factors, the base fan
 * curve breakpoints, emergency threshold, predictive-boost tuning, and safe-
 * mode fallback PWM. Tweak values here only; the matching externs in config.h
 * are read across main.cpp, fan_control.cpp, packet.cpp, and display.cpp.
 */
#include "config.h"

const unsigned long TIMEOUT_MS = 5000;

const float TEMP_MIN_VALID = 0.0;
const float TEMP_MAX_VALID = 120.0;
const float POWER_MIN_VALID = 0.0;
const float POWER_MAX_VALID = 60.0;
const float SENSOR_MISSING = -1.0;

const float ALPHA_TEMP = 0.2;
const float ALPHA_POWER = 0.3;
const float ALPHA_RATE = 0.15;

const uint8_t RAMP_STEP = 5;

const uint8_t IDLE_PWM = 77;
const float   TEMP_RAMP_START = 45.0;
const uint8_t RAMP_START_PWM = 77;
const float   TEMP_RAMP_MID = 52.0;
const uint8_t RAMP_MID_PWM = 200;
const float   TEMP_RAMP_FULL = 58.0;
const uint8_t RAMP_FULL_PWM = 255;

const float SOFT_TARGET_TEMP = 50.0;

const float EMERGENCY_TEMP = 65.0;

const float   TEMP_RATE_BOOST_GAIN = 40.0;
const uint8_t TEMP_RATE_BOOST_CAP = 60;

const uint8_t MIN_TARGET_PWM = 77;

const float   RISE_RATE_THRESHOLD = 0.2;
const uint8_t RISE_BOOST_PWM = 50;
const float   POWER_RATE_THRESHOLD = 1.5;
const uint8_t POWER_RATE_BOOST_PWM = 40;
const float   POWER_LEVEL_THRESHOLD = 10.0;
const uint8_t POWER_LEVEL_BOOST_PWM = 30;

const uint8_t SAFE_MODE_PWM = 220;
