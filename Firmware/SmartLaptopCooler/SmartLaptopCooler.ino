#include <Arduino.h>
#include "BluetoothSerial.h"
#include "esp_bt.h"
#include <U8g2lib.h>
#include <Wire.h>

BluetoothSerial SerialBT;

// SH1106 128x32, I2C, hardware I2C, no reset pin
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ===== Pin config =====
const int LED_PIN = 2;
const int FAN_PWM_PIN = 25;

// ===== Bluetooth / connection state =====
bool          wasConnected = false;
unsigned long lastBlinkTime = 0;
bool          ledState = false;

unsigned long       lastRxTime = 0;
const unsigned long TIMEOUT_MS = 5000;

// ===== Packet validation bounds =====
const float TEMP_MIN_VALID = 0.0;
const float TEMP_MAX_VALID = 120.0;
const float POWER_MIN_VALID = 0.0;
const float POWER_MAX_VALID = 60.0;
const float SENSOR_MISSING = -1.0;

// ===== Temp/power smoothing (EMA) =====
const float ALPHA_TEMP = 0.2;
const float ALPHA_POWER = 0.3;
const float ALPHA_RATE = 0.15;

float smoothedCpuTemp = 0;
float smoothedGpuTemp = 0;
float smoothedPower = 0;
bool  firstReading = true;
bool  firstCpuReading = true;
bool  firstGpuReading = true;
bool  firstPowerReading = true;

// ===== Rate-of-change tracking =====
float         prevCpuTemp = 0;
float         prevGpuTemp = 0;
float         prevPower = 0;
unsigned long prevSampleTime = 0;

float cpuTempRate = 0;
float gpuTempRate = 0;
float powerRate = 0;

// ===== PWM ramping =====
int       currentPWM = 76;
const int RAMP_STEP = 3;

// ===== External cooler compensation tuning =====
const float RISE_RATE_THRESHOLD = 0.4;
const int   RISE_BOOST_PWM = 35;
const float POWER_RATE_THRESHOLD = 3.0;
const int   POWER_RATE_BOOST_PWM = 25;
const float POWER_LEVEL_THRESHOLD = 20.0;
const int   POWER_LEVEL_BOOST_PWM = 20;

// ===== Safe mode =====
const int SAFE_MODE_PWM = 178;

// ===== OLED display state =====
float displayCpuTemp = -1;
float displayGpuTemp = -1;
int   displayPWM = 76;
bool  displayConnected = false;
bool  displaySafeMode = false;

//===== Bitmaps =====

static const unsigned char image_HansoyLogo_bits[] = {
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x60, 0x95, 0x03, 0x00, 0x00, 0x00, 0x00, 0x1e,
    0x15, 0x0c, 0x00, 0x00, 0x00, 0x80, 0x01, 0xe2, 0x31, 0x00, 0x00, 0x00,
    0x60, 0x00, 0x02, 0xce, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x10, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x60, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x80, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x80, 0xc0,
    0xff, 0xf8, 0x3f, 0x14, 0x00, 0x40, 0xc0, 0xff, 0x10, 0x10, 0x28, 0x00,
    0x20, 0x80, 0x7f, 0x20, 0xc8, 0x70, 0x00, 0x90, 0x43, 0x3f, 0x20, 0x28,
    0x91, 0x00, 0x90, 0x7f, 0x3f, 0x20, 0xd8, 0x8f, 0x00, 0x88, 0x1f, 0x3f,
    0x20, 0x68, 0x60, 0x06, 0x08, 0x07, 0x3f, 0x20, 0x20, 0x1f, 0x09, 0x04,
    0x63, 0x3f, 0x20, 0x80, 0x20, 0x09, 0xf4, 0x19, 0x3f, 0x20, 0x40, 0xe7,
    0x06, 0xf2, 0x05, 0x3f, 0x20, 0x00, 0x03, 0x01, 0xf2, 0x04, 0x3f, 0x20,
    0x15, 0x81, 0x00, 0xc2, 0x02, 0xb8, 0x9f, 0x3f, 0x7e, 0x02, 0x41, 0xc1,
    0x7b, 0x40, 0x40, 0xfe, 0x05, 0x61, 0xc1, 0x03, 0x60, 0xc0, 0x00, 0x08,
    0x71, 0x61, 0xfe, 0x47, 0x40, 0xfe, 0x05, 0x79, 0x61, 0xfe, 0x6f, 0xc0,
    0x00, 0x03, 0x79, 0x61, 0xfe, 0x47, 0x40, 0x0e, 0x01, 0x71, 0xc1, 0x03,
    0x60, 0xc0, 0xf0, 0x07, 0x61, 0xc1, 0x7b, 0x40, 0x40, 0x06, 0x08, 0xc1,
    0x02, 0xb8, 0x9f, 0xbf, 0xfe, 0x09, 0xc1, 0x02, 0x3f, 0x20, 0x55, 0xe1,
    0x07, 0xe1, 0x04, 0x3f, 0x20, 0x40, 0xc3, 0x00, 0xe1, 0x09, 0x3f, 0x20,
    0x40, 0x27, 0x01, 0xe1, 0x33, 0x3f, 0x20, 0x80, 0x20, 0x02, 0x02, 0xc6,
    0x3f, 0x20, 0x20, 0x5f, 0x05, 0x02, 0x1f, 0x3f, 0x20, 0x68, 0x88, 0x05,
    0x02, 0x7f, 0x3f, 0x20, 0x98, 0x17, 0x05, 0x04, 0x47, 0x3f, 0x20, 0x10,
    0xa9, 0x02, 0x04, 0x80, 0x7f, 0x10, 0x20, 0x91, 0x02, 0x08, 0xc0, 0xff,
    0xf8, 0xff, 0x40, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x40, 0x01, 0x10,
    0x12, 0x23, 0x79, 0x4c, 0x24, 0x01, 0x10, 0x92, 0x64, 0x09, 0x92, 0xa2,
    0x00, 0x20, 0x9e, 0xa4, 0x79, 0x12, 0x51, 0x00, 0x40, 0x92, 0x27, 0x41,
    0x12, 0x29, 0x00, 0x80, 0x92, 0x24, 0x79, 0x0c, 0x15, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x02, 0x00, 0x00, 0x80, 0x05, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x18, 0x00, 0x02, 0xb0,
    0x01, 0x00, 0x00, 0x60, 0x00, 0x02, 0x4e, 0x00, 0x00, 0x00, 0x80, 0x01,
    0xc2, 0x31, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x25, 0x0e, 0x00, 0x00, 0x00,
    0x00, 0x60, 0xd5, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};

static const unsigned char image_logo_bits[] U8X8_PROGMEM = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x18, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xf8, 0x03, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x06, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x1f, 0x0f, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x3f, 0x0b, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xb8, 0x13,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f, 0x80, 0xb0, 0x13, 0x90, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x8c,
    0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x7f, 0x10, 0x10, 0x80, 0x00, 0x00, 0x00, 0xe0, 0x9f, 0x80,
    0x8f, 0x1f, 0x10, 0xff, 0x00, 0x00, 0x00, 0x00, 0x90, 0x80, 0x33, 0x1f, 0x90, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x97,
    0x80, 0x33, 0x1e, 0x90, 0xfe, 0x03, 0x00, 0x00, 0x04, 0x90, 0x00, 0x71, 0x08, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x98, 0x00, 0xf2, 0x04, 0x90, 0x01, 0x00, 0x00, 0x00, 0x00, 0x8e, 0x00, 0xec, 0x03, 0x10, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0xf0, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x03, 0xfc, 0x7f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x02, 0x00, 0x00, 0xc0,
    0x01, 0x04, 0x00, 0x01, 0x00, 0x08, 0x02, 0x80, 0x00, 0x40, 0x00, 0x04, 0x00, 0xf7, 0xd9, 0x1d, 0x82, 0xdd, 0xdd,
    0x43, 0xdc, 0x75, 0x07, 0x54, 0x5d, 0x08, 0xc2, 0x95, 0x54, 0x42, 0x54, 0x75, 0x01, 0x54, 0x55, 0x08, 0x42, 0x95,
    0x54, 0x42, 0x54, 0x15, 0x01, 0x57, 0x5d, 0x18, 0xde, 0x9d, 0xdd, 0xc3, 0xdd, 0x75, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x40, 0x00, 0x00, 0x00, 0x00};

// ----- OLED rendering -----
void drawBrand(void) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  int logoW = 52;
  int logoH = 56;
  int x = (128 - logoW) / 2;
  int y = (64 - logoH) / 2;

  u8g2.clearBuffer();
  u8g2.drawXBMP(x, y, logoW, logoH, image_HansoyLogo_bits);
  u8g2.sendBuffer();
}

void drawErrors(void) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  int logoW = 75;
  int logoH = 31;
  int x = (128 - logoW) / 2;
  int y = (64 - logoH) / 2;

  u8g2.clearBuffer();
  u8g2.drawXBMP(x, y, logoW, logoH, image_logo_bits);
  u8g2.sendBuffer();
}

void drawStatus(const char *msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_8x13B_tf);
  u8g2.drawStr(0, 38, msg);
  u8g2.sendBuffer();
}

void drawDashboard() {
  char line[24];
  char valBuf[10];
  u8g2.clearBuffer();

  // CPU Temp (top left)
  if (displayCpuTemp < 0) {
    snprintf(line, sizeof(line), "CPU: N/A");
  } else {
    snprintf(line, sizeof(line), "CPU: %4.1fC", displayCpuTemp);
  }
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(0, 15, line);

  // GPU Temp (top right)
  if (displayGpuTemp < 0) {
    snprintf(line, sizeof(line), "GPU: N/A");
  } else {
    snprintf(line, sizeof(line), "GPU: %4.1fC", displayGpuTemp);
  }
  u8g2.drawStr(67, 15, line);

  // Power (bottom left)
  if (smoothedPower < 0) {
    snprintf(line, sizeof(line), "PWR: N/A");
  } else {
    snprintf(line, sizeof(line), "PWR: %4.2fW", smoothedPower);
  }
  u8g2.drawStr(0, 40, line);

  // Fan speed (bottom right)
  snprintf(line, sizeof(line), "Fan: %3d%%", (displayPWM * 100) / 255);
  u8g2.drawStr(67, 40, line);

  // Safe mode banner if active
  if (displaySafeMode) {
    u8g2.setFont(u8g2_font_8x13B_tf);
    u8g2.drawStr(20, 60, "SAFE MODE");
  }

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(FAN_PWM_PIN, OUTPUT);
  analogWrite(FAN_PWM_PIN, currentPWM);

  u8g2.begin();

  Serial.println();
  Serial.println("========================================");
  Serial.println("Smart Laptop Cooler - ESP32 Firmware");
  Serial.println("External Cooler Compensation Enabled");
  Serial.println("========================================");

  drawBrand();
  delay(2000);

  drawErrors();
  delay(2000);

  drawStatus("Booting...");

  Serial.print("[INIT] Starting Bluetooth SPP as \"Smart Laptop Cooler\"... ");
  if (SerialBT.begin("Smart Laptop Cooler")) {
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
    Serial.println("[ERROR] Bluetooth failed to start. Halting.");
    drawStatus("BT FAILED");
    while (true) {
      delay(1000);
    }
  }
  esp_bt_sleep_disable();
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);

  Serial.println("[INIT] Device visible for pairing.");
  Serial.println("[INIT] LED: SLOW BLINK = waiting to pair | SOLID ON = connected");
  Serial.println("----------------------------------------");

  drawStatus("Waiting for BT..");

  lastRxTime = millis();
  prevSampleTime = millis();
}

void updateLED(bool connected) {
  if (connected) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    if (millis() - lastBlinkTime > 500) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
}

// ===== Per-field packet validation =====
struct PacketValidity {
  bool cpuTempValid;
  bool cpuPowerValid;
  bool gpuTempValid;
  bool anyValid;
};

PacketValidity validatePacket(float cpuTemp, float cpuPower, float gpuTemp) {
  PacketValidity v;

  v.cpuTempValid = (cpuTemp != SENSOR_MISSING) &&
                   (cpuTemp >= TEMP_MIN_VALID && cpuTemp <= TEMP_MAX_VALID);
  v.gpuTempValid = (gpuTemp != SENSOR_MISSING) &&
                   (gpuTemp >= TEMP_MIN_VALID && gpuTemp <= TEMP_MAX_VALID);
  v.cpuPowerValid = (cpuPower != SENSOR_MISSING) &&
                    (cpuPower >= POWER_MIN_VALID && cpuPower <= POWER_MAX_VALID);

  // Fan control needs at least one temp source; power alone can't drive the curve
  v.anyValid = v.cpuTempValid || v.gpuTempValid;

  if (!v.cpuTempValid && cpuTemp != SENSOR_MISSING) {
    Serial.printf("[VALIDATE] CPU temp out of range: %.1f\n", cpuTemp);
  }
  if (!v.gpuTempValid && gpuTemp != SENSOR_MISSING) {
    Serial.printf("[VALIDATE] GPU temp out of range: %.1f\n", gpuTemp);
  }
  if (!v.cpuPowerValid && cpuPower != SENSOR_MISSING) {
    Serial.printf("[VALIDATE] CPU power out of range: %.2f\n", cpuPower);
  }

  return v;
}

float smoothValue(float newVal, float prevSmoothed, float alpha, bool isFirst) {
  if (isFirst)
    return newVal;
  return (alpha * newVal) + ((1 - alpha) * prevSmoothed);
}

int tempToPWM(float t) {
  if (t < 50)
    return 76;
  if (t < 70)
    return map(t, 50, 70, 76, 178);
  if (t < 85)
    return map(t, 70, 85, 178, 255);
  return 255;
}

int rampPWM(int target) {
  if (currentPWM < target) {
    currentPWM = min(currentPWM + RAMP_STEP, target);
  } else if (currentPWM > target) {
    currentPWM = max(currentPWM - RAMP_STEP, target);
  }
  return currentPWM;
}

void applySafeMode() {
  int rampedSafe = rampPWM(SAFE_MODE_PWM);
  analogWrite(FAN_PWM_PIN, rampedSafe);
  Serial.printf("[SAFE MODE] No data received in >%lums. Fan ramping to safe PWM: %d\n",
                TIMEOUT_MS, rampedSafe);
  displaySafeMode = true;
  displayPWM = rampedSafe;
}

void loop() {
  bool isConnected = SerialBT.hasClient();

  if (isConnected && !wasConnected) {
    Serial.println("[BT] >>> Client CONNECTED <<<");
    lastRxTime = millis();
    displayConnected = true;
    displaySafeMode = false;
  }
  if (!isConnected && wasConnected) {
    Serial.println("[BT] Client disconnected. Waiting for reconnection...");
    displayConnected = false;
    drawStatus("Disconnected");
  }
  wasConnected = isConnected;

  updateLED(isConnected);

  if (SerialBT.available()) {
    String line = SerialBT.readStringUntil('\n');
    lastRxTime = millis();

    if (line == "PING") {
      SerialBT.println("THERMALBRIDGE");
      Serial.println("[PING] Replied with THERMALBRIDGE");
      lastRxTime = millis();
      return;
    }

    Serial.print("Received: ");
    Serial.println(line);

    float cpuTemp, cpuPower, gpuTemp;

    int parsed = sscanf(line.c_str(), "%f,%f,%f", &cpuTemp, &cpuPower, &gpuTemp);

    if (parsed == 3) {
      PacketValidity v = validatePacket(cpuTemp, cpuPower, gpuTemp);

      if (!v.anyValid) {
        Serial.println("[REJECTED] No usable temp sensors in this packet. Ignoring.");
        return;
      }

      displaySafeMode = false;

      unsigned long now = millis();
      float         dt = (now - prevSampleTime) / 1000.0;
      if (dt <= 0)
        dt = 1.0;

      // Only smooth/update fields that are actually valid this packet
      if (v.cpuTempValid) {
        smoothedCpuTemp = smoothValue(cpuTemp, smoothedCpuTemp, ALPHA_TEMP, firstCpuReading);
        firstCpuReading = false;
      }
      if (v.gpuTempValid) {
        smoothedGpuTemp = smoothValue(gpuTemp, smoothedGpuTemp, ALPHA_TEMP, firstGpuReading);
        firstGpuReading = false;
      }
      if (v.cpuPowerValid) {
        smoothedPower = smoothValue(cpuPower, smoothedPower, ALPHA_POWER, firstPowerReading);
        firstPowerReading = false;
      }

      float rawCpuRate = v.cpuTempValid ? (smoothedCpuTemp - prevCpuTemp) / dt : 0;
      float rawGpuRate = v.gpuTempValid ? (smoothedGpuTemp - prevGpuTemp) / dt : 0;
      float rawPowerRate = v.cpuPowerValid ? (smoothedPower - prevPower) / dt : 0;

      cpuTempRate = smoothValue(rawCpuRate, cpuTempRate, ALPHA_RATE, firstReading);
      gpuTempRate = smoothValue(rawGpuRate, gpuTempRate, ALPHA_RATE, firstReading);
      powerRate = smoothValue(rawPowerRate, powerRate, ALPHA_RATE, firstReading);

      prevCpuTemp = smoothedCpuTemp;
      prevGpuTemp = smoothedGpuTemp;
      prevPower = smoothedPower;
      prevSampleTime = now;
      firstReading = false;

      // Drive fan off whichever sensor(s) are actually valid
      float driveTemp;
      if (v.cpuTempValid && v.gpuTempValid) {
        driveTemp = max(smoothedCpuTemp, smoothedGpuTemp);
      } else if (v.cpuTempValid) {
        driveTemp = smoothedCpuTemp;
      } else {
        driveTemp = smoothedGpuTemp;
      }

      int basePWM = tempToPWM(driveTemp);

      int    boost = 0;
      String boostReason = "";

      float maxTempRate = 0;
      if (v.cpuTempValid && v.gpuTempValid) {
        maxTempRate = max(cpuTempRate, gpuTempRate);
      } else if (v.cpuTempValid) {
        maxTempRate = cpuTempRate;
      } else if (v.gpuTempValid) {
        maxTempRate = gpuTempRate;
      }

      if (maxTempRate > RISE_RATE_THRESHOLD) {
        boost += RISE_BOOST_PWM;
        boostReason += "T";
      }

      if (v.cpuPowerValid && powerRate > POWER_RATE_THRESHOLD) {
        boost += POWER_RATE_BOOST_PWM;
        boostReason += "P";
      }

      if (v.cpuPowerValid && smoothedPower > POWER_LEVEL_THRESHOLD) {
        boost += POWER_LEVEL_BOOST_PWM;
        boostReason += "L";
      }

      int targetPWM = constrain(basePWM + boost, 76, 255);
      int appliedPWM = rampPWM(targetPWM);
      analogWrite(FAN_PWM_PIN, appliedPWM);

      displayCpuTemp = v.cpuTempValid ? cpuTemp : -1;
      displayGpuTemp = v.gpuTempValid ? gpuTemp : -1;
      displayPWM = appliedPWM;

      Serial.println("------------");
      Serial.printf("CPU: %s | GPU: %s | Power: %s -> Drive: %.1fC | PWM: %d\n",
                    v.cpuTempValid ? String(cpuTemp, 1).c_str() : "N/A",
                    v.gpuTempValid ? String(gpuTemp, 1).c_str() : "N/A",
                    v.cpuPowerValid ? String(cpuPower, 2).c_str() : "N/A",
                    driveTemp, appliedPWM);
      Serial.printf("Base PWM: %d | Boost: +%d (%s) | Target: %d | Applied (ramped): %d (%.0f%%)\n",
                    basePWM, boost, boostReason.c_str(), targetPWM, appliedPWM,
                    (appliedPWM / 255.0) * 100);
      Serial.println("------------");
    } else {
      Serial.println("[REJECTED] Parse failed - malformed packet.");
    }
  }

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 3000) {
    lastHeartbeat = millis();
    Serial.printf("[HEARTBEAT] Uptime: %lus | Connected: %s | Time since last RX: %lus | Current PWM: %d\n",
                  millis() / 1000,
                  isConnected ? "yes" : "no",
                  (millis() - lastRxTime) / 1000, currentPWM);
  }

  if (millis() - lastRxTime > TIMEOUT_MS) {
    applySafeMode();
  }

  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = millis();
    if (displayConnected) {
      drawDashboard();
    }
  }

  delay(50);
}
