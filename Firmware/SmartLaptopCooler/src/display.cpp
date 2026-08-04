/*
 * display.cpp - SH1106 128x64 OLED rendering for the Smart Laptop Cooler.
 *
 * Provides three boot animations (logo wipe-in, logo slide-in, single-line
 * status message) and the live dashboard: a 2x2 text grid (CPU/GPU/PWR/Fan%)
 * with auto-scaling horizontal bars under each cell, a live fan spinner whose
 * speed tracks PWM, and a prioritized top-bar status overlay (NORMAL / BOOST
 * / RECONNECTING / SAFE_MODE / EMERGENCY). Bar scales track the session peak
 * per sensor and reset on BT reconnect.
 */
#include "display.h"
#include "bitmaps.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

float   displayCpuTemp = -1;
float   displayGpuTemp = -1;
uint8_t displayPWM = 76;
bool    displayConnected = false;
bool    displaySafeMode = false;

DashStatus displayStatus = DashStatus::NORMAL;
char       displayBoostTag[8] = "";

static uint8_t       dashSpinStep = 0;
static unsigned long lastDashSpin = 0;

void drawFanSpinner(int cx, int cy, int radius, uint8_t step) {
  float angle = (step % 8) * (PI / 4.0);

  for (int i = 0; i < 4; i++) {
    float a = angle + (i * PI / 2.0);
    int   x1 = cx + (int)(cos(a) * 2);
    int   y1 = cy + (int)(sin(a) * 2);
    int   x2 = cx + (int)(cos(a) * radius);
    int   y2 = cy + (int)(sin(a) * radius);
    u8g2.drawLine(x1, y1, x2, y2);
  }
  u8g2.drawCircle(cx, cy, 2);
}

void drawBrand(void) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  int logoW = 52;
  int logoH = 56;
  int x = (128 - logoW) / 2;
  int y = (64 - logoH) / 2;

  const unsigned long ANIM_DURATION_MS = 2000;
  const unsigned long FRAME_MS = 40;
  unsigned long       startTime = millis();
  uint8_t             spinStep = 0;
  unsigned long       lastSpin = 0;

  while (millis() - startTime < ANIM_DURATION_MS) {
    unsigned long elapsed = millis() - startTime;
    int           revealHeight = map(elapsed, 0, ANIM_DURATION_MS, 0, logoH);
    revealHeight = constrain(revealHeight, 0, logoH);

    u8g2.clearBuffer();

    u8g2.drawXBMP(x, y, logoW, logoH, image_HansoyLogo_bits);
    if (revealHeight < logoH) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(x, y + revealHeight, logoW, logoH - revealHeight);
      u8g2.setDrawColor(1);
    }

    if (millis() - lastSpin > 120) {
      lastSpin = millis();
      spinStep++;
    }
    drawFanSpinner(112, 54, 8, spinStep);

    u8g2.sendBuffer();
    delay(FRAME_MS);
  }

  for (int i = 0; i < 6; i++) {
    u8g2.clearBuffer();
    u8g2.drawXBMP(x, y, logoW, logoH, image_HansoyLogo_bits);
    drawFanSpinner(112, 54, 8, spinStep++);
    u8g2.sendBuffer();
    delay(120);
  }
}

void drawModel(void) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  int logoW = 121;
  int logoH = 51;
  int finalX = (128 - logoW) / 2;
  int y = (64 - logoH) / 2;

  const unsigned long SLIDE_DURATION_MS = 700;
  const unsigned long FRAME_MS = 30;
  unsigned long       startTime = millis();
  uint8_t             spinStep = 0;
  unsigned long       lastSpin = 0;

  while (millis() - startTime < SLIDE_DURATION_MS) {
    unsigned long elapsed = millis() - startTime;
    int           currentX = map(elapsed, 0, SLIDE_DURATION_MS, -logoW, finalX);
    currentX = constrain(currentX, -logoW, finalX);

    u8g2.clearBuffer();
    u8g2.drawXBMP(currentX, y, logoW, logoH, image_logo_bits);

    if (millis() - lastSpin > 100) {
      lastSpin = millis();
      spinStep++;
    }
    drawFanSpinner(10, 8, 6, spinStep);

    u8g2.sendBuffer();
    delay(FRAME_MS);
  }

  for (int i = 0; i < 5; i++) {
    u8g2.clearBuffer();
    u8g2.drawXBMP(finalX, y, logoW, logoH, image_logo_bits);
    drawFanSpinner(10, 8, 6, spinStep++);
    u8g2.sendBuffer();
    delay(100);
  }

  u8g2.setDrawColor(2);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.sendBuffer();
  delay(80);
  u8g2.setDrawColor(1);
  u8g2.clearBuffer();
  u8g2.drawXBMP(finalX, y, logoW, logoH, image_logo_bits);
  u8g2.sendBuffer();
}

void drawStatus(const char *msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_8x13B_tf);
  u8g2.drawStr(0, 38, msg);
  u8g2.sendBuffer();
}

struct MaxTracker {
  float val;
  float floor;
  void  begin(float f) {
    floor = f;
    val = f;
  }
  void reset() {
    val = floor;
  }
  void observe(float v) {
    if (v < 0)
      return;
    if (v > val)
      val = v;
  }
  float scale() const {
    return val > 0 ? val : 1.0f;
  }
};

static MaxTracker maxCpuTemp;
static MaxTracker maxGpuTemp;
static MaxTracker maxPower;
static bool       maxTrackersReady = false;

static void initMaxTrackers() {
  maxCpuTemp.begin(50.0f);
  maxGpuTemp.begin(50.0f);
  maxPower.begin(15.0f);
  maxTrackersReady = true;
}

void resetMaxTrackers() {
  if (!maxTrackersReady)
    initMaxTrackers();
  maxCpuTemp.reset();
  maxGpuTemp.reset();
  maxPower.reset();
}

static void drawBar(int x, int y, int w, int h, float value, float maxV, float /*notchFrac*/) {
  if (value < 0 || maxV <= 0) {
    return;
  }
  float v = value;
  if (v > maxV)
    v = maxV;
  int fill = constrain((int)((v / maxV) * w), 0, w);
  if (fill > 0 && h > 0) {
    u8g2.drawBox(x, y, fill, h);
  }
}

void drawDashboard() {
  char line[24];
  u8g2.clearBuffer();

  static bool blink = false;
  blink = !blink;

  u8g2.drawLine(0, 16, 128, 16);
  u8g2.drawLine(63, 17, 63, 63);
  u8g2.drawLine(0, 37, 127, 37);

  switch (displayStatus) {
    case DashStatus::EMERGENCY: {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 0, 128, 15);
      u8g2.setDrawColor(0);
      u8g2.setFont(u8g2_font_8x13B_tf);
      u8g2.drawStr(2, 12, "! EMERGENCY !");
      u8g2.setDrawColor(1);
      break;
    }
    case DashStatus::SAFE_MODE:
    case DashStatus::NORMAL:
    case DashStatus::BOOST:
      if (displayConnected) {
        u8g2.drawXBMP(0, 0, 14, 16, image_bluetooth_connected_bits);
        u8g2.setFont(u8g2_font_6x13_tr);
        u8g2.drawStr(18, 12, "Connected");
      } else {
        if (blink) {
          u8g2.drawXBMP(0, 0, 14, 16, image_bluetooth_not_connected_bits);
        }
        u8g2.setFont(u8g2_font_6x13_tr);
        if (wasConnected) {
          u8g2.drawStr(18, 12, "Reconnecting...");
        } else {
          u8g2.drawStr(18, 12, "Connecting...");
        }
      }
      if (displayStatus == DashStatus::BOOST && displayBoostTag[0] != '\0') {
        u8g2.setFont(u8g2_font_6x13_tr);
        snprintf(line, sizeof(line), " +%s", displayBoostTag);
        u8g2.drawStr(78, 12, line);
      }
      break;
    case DashStatus::RECONNECTING:
      if (blink) {
        u8g2.setDrawColor(2);
        u8g2.drawBox(0, 0, 128, 15);
        u8g2.setDrawColor(1);
        u8g2.drawXBMP(0, 0, 14, 16, image_bluetooth_not_connected_bits);
      }
      u8g2.setFont(u8g2_font_6x13_tr);
      u8g2.drawStr(18, 12, "Reconnecting...");
      break;
  }

  if (displayStatus != DashStatus::EMERGENCY) {
    int spinInterval;
    if (displaySafeMode) {
      spinInterval = 250;
    } else {
      spinInterval = constrain(map(displayPWM, 76, 255, 400, 60), 60, 400);
    }
    if (millis() - lastDashSpin > (unsigned long)spinInterval) {
      lastDashSpin = millis();
      dashSpinStep++;
    }
    drawFanSpinner(119, 8, 6, dashSpinStep);
  }

  extern float smoothedPower;
  if (!maxTrackersReady)
    initMaxTrackers();
  maxCpuTemp.observe(displayCpuTemp);
  maxGpuTemp.observe(displayGpuTemp);
  maxPower.observe(smoothedPower);

  if (displayCpuTemp < 0) {
    snprintf(line, sizeof(line), "CPU: N/A");
  } else {
    int t = (int)(displayCpuTemp * 10);
    snprintf(line, sizeof(line), "CPU:%d.%dC", t / 10, abs(t) % 10);
  }
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(0, 31, line);
  drawBar(0, 33, 62, 4, displayCpuTemp, maxCpuTemp.scale(), 0.58);

  if (displayGpuTemp < 0) {
    snprintf(line, sizeof(line), "GPU: N/A");
  } else {
    int t = (int)(displayGpuTemp * 10);
    snprintf(line, sizeof(line), "GPU:%d.%dC", t / 10, abs(t) % 10);
  }
  u8g2.drawStr(66, 31, line);
  drawBar(64, 33, 62, 4, displayGpuTemp, maxGpuTemp.scale(), 0.58);

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
    drawBar(0, 59, 62, 4, smoothedPower, maxPower.scale(), 0.1667);
  }

  snprintf(line, sizeof(line), "Fan:%3d%%", (displayPWM * 100) / 255);
  u8g2.drawStr(66, 56, line);
  drawBar(64, 59, 62, 4, (float)displayPWM, 255.0, -1.0);

  u8g2.sendBuffer();
}
