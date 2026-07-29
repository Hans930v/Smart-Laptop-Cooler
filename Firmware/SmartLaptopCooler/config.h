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

// ===== External cooler compensation tuning =====
extern const float   RISE_RATE_THRESHOLD;
extern const uint8_t RISE_BOOST_PWM;
extern const float   POWER_RATE_THRESHOLD;
extern const uint8_t POWER_RATE_BOOST_PWM;
extern const float   POWER_LEVEL_THRESHOLD;
extern const uint8_t POWER_LEVEL_BOOST_PWM;

// ===== Safe mode =====
extern const uint8_t SAFE_MODE_PWM;