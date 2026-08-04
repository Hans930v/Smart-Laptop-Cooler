/*
 * fan_control.h - Externs for smoothed sensor state, rate-of-change tracking,
 * PWM state, and the fan-control function prototypes.
 *
 * Globals are owned by fan_control.cpp; main.cpp reads/writes them through
 * these externs. Functions exposed are: rampPWM (adaptive PWM stepping),
 * applySafeMode (timeout fallback), tempToPWM (piecewise base curve), and
 * smoothValue (EMA filter used across temps, power, and rates).
 */
#pragma once
#include <Arduino.h>

extern float smoothedCpuTemp;
extern float smoothedGpuTemp;
extern float smoothedPower;
extern bool  firstReading;
extern bool  firstCpuReading;
extern bool  firstGpuReading;
extern bool  firstPowerReading;

extern float         prevCpuTemp;
extern float         prevGpuTemp;
extern float         prevPower;
extern unsigned long prevSampleTime;
extern float         cpuTempRate;
extern float         gpuTempRate;
extern float         powerRate;

extern int currentPWM;

int   rampPWM(int target);
void  applySafeMode();
int   tempToPWM(float t);
float smoothValue(float newVal, float prevSmoothed, float alpha, bool isFirst);
