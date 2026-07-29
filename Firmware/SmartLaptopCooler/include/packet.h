#pragma once
#include <Arduino.h>

struct PacketValidity {
  bool cpuTempValid;
  bool cpuPowerValid;
  bool gpuTempValid;
  bool anyValid;
};

PacketValidity validatePacket(float cpuTemp, float cpuPower, float gpuTemp);

// Returns number of fields parsed (0-3). Writes results into out params.
int parsePacket(const char *line, float &cpuTemp, float &cpuPower, float &gpuTemp);