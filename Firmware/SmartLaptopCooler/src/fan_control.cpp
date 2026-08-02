#include "fan_control.h"
#include "config.h"
#include "display.h" // for displaySafeMode, displayPWM

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
  int rampedSafe = rampPWM(SAFE_MODE_PWM);
  analogWrite(FAN_PWM_PIN, rampedSafe);
  Serial.print("[SAFE MODE] No data received in >");
  Serial.print((unsigned long)TIMEOUT_MS);
  Serial.print("ms. Fan ramping to safe PWM: ");
  Serial.println(rampedSafe);
  displaySafeMode = true;
  displayPWM = rampedSafe;
}

int tempToPWM(float t) {
  if (t < TEMP_RAMP_START)
    return IDLE_PWM;
  if (t < TEMP_RAMP_MID)
    return map(t, TEMP_RAMP_START, TEMP_RAMP_MID, RAMP_START_PWM, RAMP_MID_PWM);
  if (t < TEMP_RAMP_FULL)
    return map(t, TEMP_RAMP_MID, TEMP_RAMP_FULL, RAMP_MID_PWM, RAMP_FULL_PWM);
  return RAMP_FULL_PWM;
}

float smoothValue(float newVal, float prevSmoothed, float alpha, bool isFirst) {
  if (isFirst)
    return newVal;
  return (alpha * newVal) + ((1 - alpha) * prevSmoothed);
}