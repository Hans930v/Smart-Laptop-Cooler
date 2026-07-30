#include <Arduino.h>
#include "BluetoothSerial.h"
#include "esp_bt.h"

#include "config.h"
#include "display.h"
#include "fan_control.h"
#include "packet.h"

BluetoothSerial SerialBT;

bool          wasConnected = false;
bool          ledState = false;
unsigned long lastBlinkTime = 0;
unsigned long lastRxTime = 0;

void updateLED(bool connected) {
  if (connected) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    if (millis() - lastBlinkTime > 1000) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
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

  drawModel();
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

  // drawStatus("Waiting for BT..");

  lastRxTime = millis();
  prevSampleTime = millis();
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
  }
  wasConnected = isConnected;

  updateLED(isConnected);

  if (SerialBT.available()) {
    char line[48];
    int  n = 0;
    while (SerialBT.available() && n < (int)sizeof(line) - 1) {
      char c = SerialBT.read();
      if (c == '\n' || c == '\r')
        break;
      line[n++] = c;
    }
    line[n] = '\0';
    lastRxTime = millis();

    if (strcmp(line, "PING") == 0) {
      SerialBT.println("THERMALBRIDGE");
      Serial.println("[PING] Replied with THERMALBRIDGE");
      lastRxTime = millis();
      return;
    }

    Serial.print("Received: ");
    Serial.println(line);

    float cpuTemp, cpuPower, gpuTemp;
    int   parsed = parsePacket(line, cpuTemp, cpuPower, gpuTemp);

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

      float driveTemp;
      if (v.cpuTempValid && v.gpuTempValid) {
        driveTemp = max(smoothedCpuTemp, smoothedGpuTemp);
      } else if (v.cpuTempValid) {
        driveTemp = smoothedCpuTemp;
      } else {
        driveTemp = smoothedGpuTemp;
      }

      int    basePWM = tempToPWM(driveTemp);
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
        int rateBoost = constrain((int)((maxTempRate - RISE_RATE_THRESHOLD) * 40), 0, 60);
        boost += rateBoost;
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

      if (v.cpuTempValid) {
        if (smoothedCpuTemp >= 90)
          targetPWM = 255;
        else if (smoothedCpuTemp >= 88)
          targetPWM = max(targetPWM, 245);
        else if (smoothedCpuTemp >= 85)
          targetPWM = max(targetPWM, 230);
        else if (smoothedCpuTemp >= 82)
          targetPWM = max(targetPWM, 210);
      }

      if (v.gpuTempValid) {
        if (smoothedGpuTemp >= 90)
          targetPWM = 255;
        else if (smoothedGpuTemp >= 88)
          targetPWM = max(targetPWM, 245);
        else if (smoothedGpuTemp >= 85)
          targetPWM = max(targetPWM, 230);
        else if (smoothedGpuTemp >= 82)
          targetPWM = max(targetPWM, 210);
      }

      bool emergency =
          (v.cpuTempValid && smoothedCpuTemp >= 90) || (v.gpuTempValid && smoothedGpuTemp >= 90);

      int appliedPWM;
      if (emergency) {
        currentPWM = 255;
        appliedPWM = 255;
        Serial.println("[EMERGENCY] Temp >= 90C, forcing full fan!");
      } else {
        appliedPWM = rampPWM(targetPWM);
      }

      analogWrite(FAN_PWM_PIN, appliedPWM);
      displayPWM = appliedPWM;

      displayCpuTemp = v.cpuTempValid ? cpuTemp : -1;
      displayGpuTemp = v.gpuTempValid ? gpuTemp : -1;

      Serial.println("------------");
      Serial.printf("CPU: %s | GPU: %s | Power: %s -> Drive: %.1fC | PWM: %d\n",
                    v.cpuTempValid ? String(cpuTemp, 1).c_str() : "N/A",
                    v.gpuTempValid ? String(gpuTemp, 1).c_str() : "N/A",
                    v.cpuPowerValid ? String(cpuPower, 2).c_str() : "N/A",
                    driveTemp,
                    appliedPWM);
      Serial.printf("Base PWM: %d | Boost: +%d (%s) | Target: %d | Applied (ramped): %d (%.0f%%)\n",
                    basePWM,
                    boost,
                    boostReason.c_str(),
                    targetPWM,
                    appliedPWM,
                    (appliedPWM / 255.0) * 100);
      Serial.println("------------");
    } else {
      Serial.println("[REJECTED] Parse failed - malformed packet.");
    }
  }

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 3000) {
    lastHeartbeat = millis();
    Serial.print("[HEARTBEAT] Uptime: ");
    Serial.print(millis() / 1000);
    Serial.print("s | Connected: ");
    Serial.print(isConnected ? "yes" : "no");
    Serial.print(" | Time since last RX: ");
    Serial.print((millis() - lastRxTime) / 1000);
    Serial.print("s | Current PWM: ");
    Serial.println(currentPWM);
  }

  if (millis() - lastRxTime > TIMEOUT_MS) {
    applySafeMode();
  }

  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = millis();
    drawDashboard();
  }

  delay(50);
}
