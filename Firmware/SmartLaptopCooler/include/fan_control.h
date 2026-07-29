#pragma once
#include <Arduino.h>

// Smoothed sensor state
extern float smoothedCpuTemp;
extern float smoothedGpuTemp;
extern float smoothedPower;
extern bool  firstReading;
extern bool  firstCpuReading;
extern bool  firstGpuReading;
extern bool  firstPowerReading;

// Rate-of-change tracking
extern float         prevCpuTemp;
extern float         prevGpuTemp;
extern float         prevPower;
extern unsigned long prevSampleTime;
extern float         cpuTempRate;
extern float         gpuTempRate;
extern float         powerRate;

// PWM state
extern int currentPWM;

int   rampPWM(int target);
void  applySafeMode();
int   tempToPWM(float t);
float smoothValue(float newVal, float prevSmoothed, float alpha, bool isFirst);