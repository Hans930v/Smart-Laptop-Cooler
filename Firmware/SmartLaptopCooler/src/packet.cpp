#include "packet.h"
#include "config.h"

PacketValidity validatePacket(float cpuTemp, float cpuPower, float gpuTemp) {
  PacketValidity v;

  v.cpuTempValid =
      (cpuTemp != SENSOR_MISSING) && (cpuTemp >= TEMP_MIN_VALID && cpuTemp <= TEMP_MAX_VALID);
  v.gpuTempValid =
      (gpuTemp != SENSOR_MISSING) && (gpuTemp >= TEMP_MIN_VALID && gpuTemp <= TEMP_MAX_VALID);
  v.cpuPowerValid =
      (cpuPower != SENSOR_MISSING) && (cpuPower >= POWER_MIN_VALID && cpuPower <= POWER_MAX_VALID);

  v.anyValid = v.cpuTempValid || v.gpuTempValid;

  if (!v.cpuTempValid && cpuTemp != SENSOR_MISSING) {
    Serial.print("[VALIDATE] CPU temp out of range (x10): ");
    Serial.println((int)(cpuTemp * 10));
  }
  if (!v.gpuTempValid && gpuTemp != SENSOR_MISSING) {
    Serial.print("[VALIDATE] GPU temp out of range (x10): ");
    Serial.println((int)(gpuTemp * 10));
  }
  if (!v.cpuPowerValid && cpuPower != SENSOR_MISSING) {
    Serial.print("[VALIDATE] CPU power out of range (x100): ");
    Serial.println((int)(cpuPower * 100));
  }

  return v;
}

// Hand-rolled parser: "cpuTemp,cpuPower,gpuTemp" into floats.
// Avoids sscanf("%f") which pulls the float-scanf formatter into flash.
int parsePacket(const char *line, float &cpuTemp, float &cpuPower, float &gpuTemp) {
  int         parsed = 0;
  float      *out[3] = {&cpuTemp, &cpuPower, &gpuTemp};
  const char *p = line;

  for (int i = 0; i < 3; i++) {
    while (*p == ' ' || *p == ',')
      p++;
    if (*p == '\0')
      break;
    bool neg = false;
    if (*p == '+')
      p++;
    else if (*p == '-') {
      neg = true;
      p++;
    }
    if (*p < '0' || *p > '9')
      break;
    long intPart = 0;
    while (*p >= '0' && *p <= '9') {
      intPart = intPart * 10 + (*p - '0');
      p++;
    }
    long frac = 0;
    long fracDiv = 1;
    if (*p == '.') {
      p++;
      while (*p >= '0' && *p <= '9' && fracDiv < 100000000L) {
        frac = frac * 10 + (*p - '0');
        fracDiv *= 10;
        p++;
      }
    }
    float val = (float)intPart + (float)frac / (float)fracDiv;
    if (neg)
      val = -val;
    *out[i] = val;
    parsed++;
    if (*p != ',' && *p != '\0')
      break;
  }

  return parsed;
}