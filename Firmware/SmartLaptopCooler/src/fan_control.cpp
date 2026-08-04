/*
 * fan_control.cpp - Smoothing, fan-curve mapping, and ramping logic.
 *
 * Holds the EMA smoothing helpers, the piecewise temperature -> PWM base curve
 * (idle -> ramp-start -> mid -> full), adaptive PWM ramping toward a target,
 * and the safe-mode fallback applied when Bluetooth data is lost (ramps the fan
 * toward SAFE_MODE_PWM and marks the dashboard status). All smoothed state
 * shared with main loop lives here as globals visible via fan_control.h.
 */
#include "fan_control.h"
#include "config.h"
#include "display.h"

float smoothedCpuTemp = 0;
float smoothedGpuTemp = 0;
float smoothedPower = 0;
bool  firstReading = true;
bool  firstCpuReading = true;
bool  firstGpuReading = true;
bool  firstPowerReading = true;

float         prevCpuTemp = 0;
float         prevGpuTemp = 0;
float         prevPower = 0;
unsigned long prevSampleTime = 0;
float         cpuTempRate = 0;
float         gpuTempRate = 0;
float         powerRate = 0;

int currentPWM = 77;

int rampPWM(int target) {
  int diff = abs(target - currentPWM);

  int step;
  if (diff > 120)
    step = 18;
  else if (diff > 80)
    step = 12;
  else if (diff > 40)
    step = 7;
  else
    step = RAMP_STEP;

  if (currentPWM < target)
    currentPWM = min(currentPWM + step, target);
  else if (currentPWM > target)
    currentPWM = max(currentPWM - step, target);

  return currentPWM;
}

void applySafeMode() {
  currentPWM = rampPWM(SAFE_MODE_PWM);
  analogWrite(FAN_PWM_PIN, currentPWM);
  Serial.print("[SAFE MODE] No data received in >");
  Serial.print((unsigned long)TIMEOUT_MS);
  Serial.print("ms. Fan ramping to safe PWM: ");
  Serial.println(currentPWM);
  displaySafeMode = true;
  displayStatus = DashStatus::SAFE_MODE;
  displayPWM = currentPWM;
}

int tempToPWM(float t) {
  if (t < TEMP_RAMP_START)
    return IDLE_PWM;
  if (t < TEMP_RAMP_MID) {
    float frac = (t - TEMP_RAMP_START) / (TEMP_RAMP_MID - TEMP_RAMP_START);
    return (int)(RAMP_START_PWM + frac * (RAMP_MID_PWM - RAMP_START_PWM));
  }
  if (t < TEMP_RAMP_FULL) {
    float frac = (t - TEMP_RAMP_MID) / (TEMP_RAMP_FULL - TEMP_RAMP_MID);
    return (int)(RAMP_MID_PWM + frac * (RAMP_FULL_PWM - RAMP_MID_PWM));
  }
  return RAMP_FULL_PWM;
}

float smoothValue(float newVal, float prevSmoothed, float alpha, bool isFirst) {
  if (isFirst)
    return newVal;
  return (alpha * newVal) + ((1 - alpha) * prevSmoothed);
}
