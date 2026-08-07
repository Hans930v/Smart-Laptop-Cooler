#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ---- Config ----
#define MOSFET_PIN 18            // GPIO pin connected to the gate of the MOSFET
const int PWM_CHANNEL    = 0;    // LEDC channel 0-15
const int PWM_FREQ       = 600;  // Hz
const int PWM_RESOLUTION = 8;    // 8-bit = 0-255 duty range
const int MIN_DUTY       = 76;   // never go below idle (~30%) - keeps fan's controller alive
const int MAX_DUTY       = 255;  // Maximum duty cycle
const int RAMP_STEP      = 5;    // Step size for ramping up/down
const int RAMP_DELAY     = 50;   // Delay in milliseconds between each step

// ---- OLED ----
// Adjust SDA/SCL pins if needed for your ESP32
#define SDA 21
#define SCL 22
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);
  delay(500);

  // Configure LEDC PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOSFET_PIN, PWM_CHANNEL);

  // Init OLED
  Wire.begin(SDA, SCL);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  Serial.println("Setup complete.");
}

void displayStatus(const char *status, int duty) {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "Fan Control");
  u8g2.drawStr(0, 28, status);

  char buf[32];
  sprintf(buf, "Duty: %d / 255", duty);
  u8g2.drawStr(0, 44, buf);

  int percent = map(duty, MIN_DUTY, MAX_DUTY, 0, 100);
  sprintf(buf, "Approx: %d%%", percent);
  u8g2.drawStr(0, 60, buf);

  u8g2.sendBuffer();
}

void simpleOnOff() {
  Serial.println("ON");
  for (int duty = MIN_DUTY; duty <= MAX_DUTY; duty += 10) {  // soft-start kick instead of slam
    ledcWrite(PWM_CHANNEL, duty);
    delay(20);
  }
  ledcWrite(PWM_CHANNEL, MAX_DUTY);
  displayStatus("ON", MAX_DUTY);
  delay(2000);

  Serial.println("OFF");
  for (int duty = MAX_DUTY; duty >= MIN_DUTY; duty -= 10) {  // soft stop instead of cut
    ledcWrite(PWM_CHANNEL, duty);
    delay(20);
  }
  ledcWrite(PWM_CHANNEL, MIN_DUTY);
  displayStatus("IDLE", MIN_DUTY);
  delay(2000);
}

void rampUpDown() {
  // ON
  ledcWrite(PWM_CHANNEL, MAX_DUTY);
  displayStatus("ON", MAX_DUTY);
  delay(2000);

  Serial.println("Ramp down");
  for (int duty = MAX_DUTY; duty >= MIN_DUTY; duty--) {
    ledcWrite(PWM_CHANNEL, duty);
    displayStatus("Ramping Down", duty);
    delay(RAMP_DELAY);
  }

  Serial.println("Ramp up");
  for (int duty = MIN_DUTY; duty <= MAX_DUTY; duty++) {
    ledcWrite(PWM_CHANNEL, duty);
    displayStatus("Ramping Up", duty);
    // Slow the high-current mid-range (60-85% duty is where inrush peaks)
    int pace = RAMP_DELAY;
    if (duty >= 153 && duty <= 217) // 60%..85%
      pace = 120;
    delay(pace);
  }

  // Off to idle, not zero
  ledcWrite(PWM_CHANNEL, MIN_DUTY);
  displayStatus("IDLE", MIN_DUTY);
  delay(2000);
}

void loop() {
  // simpleOnOff();
  rampUpDown();
  // delay(1000);
}
