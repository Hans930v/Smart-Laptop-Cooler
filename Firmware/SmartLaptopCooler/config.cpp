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

const uint8_t RAMP_STEP = 3;

const float   RISE_RATE_THRESHOLD = 0.4;
const uint8_t RISE_BOOST_PWM = 35;
const float   POWER_RATE_THRESHOLD = 3.0;
const uint8_t POWER_RATE_BOOST_PWM = 25;
const float   POWER_LEVEL_THRESHOLD = 20.0;
const uint8_t POWER_LEVEL_BOOST_PWM = 20;

const uint8_t SAFE_MODE_PWM = 179;