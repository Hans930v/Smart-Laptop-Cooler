#include "display.h"
#include "bitmaps.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

float   displayCpuTemp = -1;
float   displayGpuTemp = -1;
uint8_t displayPWM = 76;
bool    displayConnected = false;
bool    displaySafeMode = false;

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

void drawModel(void) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  int logoW = 121;
  int logoH = 51;
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
  u8g2.clearBuffer();

  static bool blink = false;
  blink = !blink;

  u8g2.drawLine(0, 16, 128, 16);
  u8g2.drawLine(63, 17, 63, 63);
  u8g2.drawLine(0, 37, 127, 37);

  if (displayConnected) {
    u8g2.drawXBMP(0, 0, 14, 16, image_bluetooth_connected_bits);
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(20, 12, "Connected");
  } else {
    if (blink) {
      u8g2.drawXBMP(0, 0, 14, 16, image_bluetooth_not_connected_bits);
    }
    u8g2.setFont(u8g2_font_6x13_tr);
    if (wasConnected) {
      u8g2.drawStr(20, 12, "Reconnecting...");
    } else {
      u8g2.drawStr(20, 12, "Connecting...");
    }
  }

  if (displayCpuTemp < 0) {
    snprintf(line, sizeof(line), "CPU:N/A");
  } else {
    int t = (int)(displayCpuTemp * 10);
    snprintf(line, sizeof(line), "CPU:%d.%dC", t / 10, abs(t) % 10);
  }
  u8g2.drawStr(0, 31, line);

  if (displayGpuTemp < 0) {
    snprintf(line, sizeof(line), "GPU: N/A");
  } else {
    int t = (int)(displayGpuTemp * 10);
    snprintf(line, sizeof(line), "GPU:%d.%dC", t / 10, abs(t) % 10);
  }
  u8g2.drawStr(67, 31, line);

  extern float smoothedPower; // defined in fan_control.cpp

  if (displaySafeMode) {
    u8g2.drawStr(0, 56, "SAFE MODE");
  } else {
    if (smoothedPower < 0) {
      snprintf(line, sizeof(line), "PWR: N/A");
    } else {
      int p = (int)(smoothedPower * 100);
      snprintf(line, sizeof(line), "PWR:%d.%02dW", p / 100, abs(p) % 100);
    }
    u8g2.drawStr(0, 56, line);
  }

  snprintf(line, sizeof(line), "Fan:%3d%%", (displayPWM * 100) / 255);
  u8g2.drawStr(67, 56, line);

  u8g2.sendBuffer();
}