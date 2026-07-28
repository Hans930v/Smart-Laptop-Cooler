#include <Arduino.h>
#include "BluetoothSerial.h"
#include "esp_bt.h"

BluetoothSerial SerialBT;

// ===== Pin config =====
const int LED_PIN = 2;
const int FAN_PWM_PIN = 25;

// ===== Bluetooth / connection state =====
bool wasConnected = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;

unsigned long lastRxTime = 0;
const unsigned long TIMEOUT_MS = 5000;

// ===== Packet validation bounds =====
const float TEMP_MIN_VALID = 0.0;
const float TEMP_MAX_VALID = 120.0;
const float POWER_MIN_VALID = 0.0;
const float POWER_MAX_VALID = 60.0;  // tightened for a U-series 15-28W part

// ===== Temp/power smoothing (EMA) =====
const float ALPHA_TEMP = 0.2;
const float ALPHA_POWER = 0.3;
const float ALPHA_RATE = 0.15;  // smooths the rate-of-change signal itself

float smoothedCpuTemp = 0;
float smoothedGpuTemp = 0;
float smoothedPower = 0;
bool firstReading = true;

// ===== Rate-of-change tracking =====
float prevCpuTemp = 0;
float prevGpuTemp = 0;
float prevPower = 0;
unsigned long prevSampleTime = 0;

float cpuTempRate = 0;  // °C/sec, smoothed
float gpuTempRate = 0;  // °C/sec, smoothed
float powerRate = 0;    // W/sec, smoothed

// ===== PWM ramping =====
int currentPWM = 76;
const int RAMP_STEP = 3;

// ===== External cooler compensation tuning =====
const float RISE_RATE_THRESHOLD = 0.4;  // °C/sec - climbing this fast triggers boost
const int RISE_BOOST_PWM = 35;
const float POWER_RATE_THRESHOLD = 3.0;  // W/sec - power ramping this fast triggers boost
const int POWER_RATE_BOOST_PWM = 25;
const float POWER_LEVEL_THRESHOLD = 20.0;  // W - sustained high power triggers floor raise
const int POWER_LEVEL_BOOST_PWM = 20;

// ===== Safe mode =====
const int SAFE_MODE_PWM = 178;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(FAN_PWM_PIN, OUTPUT);
  analogWrite(FAN_PWM_PIN, currentPWM);

  Serial.println();
  Serial.println("========================================");
  Serial.println("Smart Laptop Cooler - ESP32 Firmware");
  Serial.println("External Cooler Compensation Enabled");
  Serial.println("========================================");

  Serial.print("[INIT] Starting Bluetooth SPP as \"Smart Laptop Cooler\"... ");
  if (SerialBT.begin("Smart Laptop Cooler")) {
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
    Serial.println("[ERROR] Bluetooth failed to start. Halting.");
    while (true) { delay(1000); }
  }
  esp_bt_sleep_disable();
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);

  Serial.println("[INIT] Device visible for pairing.");
  Serial.println("[INIT] LED: SLOW BLINK = waiting to pair | SOLID ON = connected");
  Serial.println("----------------------------------------");

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

bool validatePacket(float cpuTemp, float cpuPower, float gpuTemp) {
  if (cpuTemp < TEMP_MIN_VALID || cpuTemp > TEMP_MAX_VALID) {
    Serial.printf("[VALIDATE] CPU temp out of range: %.1f\n", cpuTemp);
    return false;
  }
  if (gpuTemp < TEMP_MIN_VALID || gpuTemp > TEMP_MAX_VALID) {
    Serial.printf("[VALIDATE] GPU temp out of range: %.1f\n", gpuTemp);
    return false;
  }
  if (cpuPower < POWER_MIN_VALID || cpuPower > POWER_MAX_VALID) {
    Serial.printf("[VALIDATE] CPU power out of range: %.2f\n", cpuPower);
    return false;
  }
  return true;
}

// Applies EMA smoothing to a value given its previous smoothed state
float smoothValue(float newVal, float prevSmoothed, float alpha, bool isFirst) {
  if (isFirst) return newVal;
  return (alpha * newVal) + ((1 - alpha) * prevSmoothed);
}

// ----- Fan controller: temp -> baseline target PWM -----
int tempToPWM(float t) {
  if (t < 50) return 76;
  if (t < 70) return map(t, 50, 70, 76, 178);
  if (t < 85) return map(t, 70, 85, 178, 255);
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
}

void loop() {
  bool isConnected = SerialBT.hasClient();

  if (isConnected && !wasConnected) {
    Serial.println("[BT] >>> Client CONNECTED <<<");
    lastRxTime = millis();
  }
  if (!isConnected && wasConnected) {
    Serial.println("[BT] Client disconnected. Waiting for reconnection...");
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
      if (!validatePacket(cpuTemp, cpuPower, gpuTemp)) {
        Serial.println("[REJECTED] Packet failed validation. Ignoring this reading.");
        return;
      }

      unsigned long now = millis();
      float dt = (now - prevSampleTime) / 1000.0;  // seconds
      if (dt <= 0) dt = 1.0;                       // guard against div-by-zero on first/duplicate sample

      // ----- Update internal values (smoothing) -----
      smoothedCpuTemp = smoothValue(cpuTemp, smoothedCpuTemp, ALPHA_TEMP, firstReading);
      smoothedGpuTemp = smoothValue(gpuTemp, smoothedGpuTemp, ALPHA_TEMP, firstReading);
      smoothedPower = smoothValue(cpuPower, smoothedPower, ALPHA_POWER, firstReading);

      // ----- Rate of change (smoothed, not raw, to avoid noise-driven jumps) -----
      float rawCpuRate = (smoothedCpuTemp - prevCpuTemp) / dt;
      float rawGpuRate = (smoothedGpuTemp - prevGpuTemp) / dt;
      float rawPowerRate = (smoothedPower - prevPower) / dt;

      cpuTempRate = smoothValue(rawCpuRate, cpuTempRate, ALPHA_RATE, firstReading);
      gpuTempRate = smoothValue(rawGpuRate, gpuTempRate, ALPHA_RATE, firstReading);
      powerRate = smoothValue(rawPowerRate, powerRate, ALPHA_RATE, firstReading);

      prevCpuTemp = smoothedCpuTemp;
      prevGpuTemp = smoothedGpuTemp;
      prevPower = smoothedPower;
      prevSampleTime = now;
      firstReading = false;

      // ----- Fan Controller: baseline + compensation -----
      float driveTemp = max(smoothedCpuTemp, smoothedGpuTemp);
      int basePWM = tempToPWM(driveTemp);

      int boost = 0;
      String boostReason = "";

      // Compensation 1: fast-rising temp (either CPU or GPU) -> external cooler lag
      float maxTempRate = max(cpuTempRate, gpuTempRate);
      if (maxTempRate > RISE_RATE_THRESHOLD) {
        boost += RISE_BOOST_PWM;
        boostReason += "[temp-rate] ";
      }

      // Compensation 2: power ramping fast -> heat about to arrive, get ahead of it
      if (powerRate > POWER_RATE_THRESHOLD) {
        boost += POWER_RATE_BOOST_PWM;
        boostReason += "[power-rate] ";
      }

      // Compensation 3: sustained high power draw -> raise floor even if temp hasn't caught up
      if (smoothedPower > POWER_LEVEL_THRESHOLD) {
        boost += POWER_LEVEL_BOOST_PWM;
        boostReason += "[power-level] ";
      }

      int targetPWM = constrain(basePWM + boost, 76, 255);
      int appliedPWM = rampPWM(targetPWM);
      analogWrite(FAN_PWM_PIN, appliedPWM);

      Serial.println("------------");
      Serial.printf("CPU: %.1fC (raw) / %.1fC (smoothed) | rate: %.2f C/s\n",
                    cpuTemp, smoothedCpuTemp, cpuTempRate);
      Serial.printf("GPU: %.1fC (raw) / %.1fC (smoothed) | rate: %.2f C/s\n",
                    gpuTemp, smoothedGpuTemp, gpuTempRate);
      Serial.printf("Power: %.2fW (raw) / %.2fW (smoothed) | rate: %.2f W/s\n",
                    cpuPower, smoothedPower, powerRate);
      Serial.printf("Base PWM: %d | Boost: +%d %s| Target: %d | Applied (ramped): %d (%.0f%%)\n",
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
                  (millis() - lastRxTime) / 1000,
                  currentPWM);
  }

  if (millis() - lastRxTime > TIMEOUT_MS) {
    applySafeMode();
  }

  delay(50);
}