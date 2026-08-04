/*
 * packet.h - Bluetooth packet validity struct + parser/validator prototypes.
 *
 * PacketValidity holds per-field validity flags (anyValid requires at least a
 * valid CPU or GPU temp). parsePacket returns count of fields parsed (0-3) and
 * writes results into the float out-params; validatePacket checks bounds and
 * distinguishes sentinel "missing" from out-of-range.
 */
#pragma once
#include <Arduino.h>

struct PacketValidity {
  bool cpuTempValid;
  bool cpuPowerValid;
  bool gpuTempValid;
  bool anyValid;
};

PacketValidity validatePacket(float cpuTemp, float cpuPower, float gpuTemp);
int parsePacket(const char *line, float &cpuTemp, float &cpuPower, float &gpuTemp);
