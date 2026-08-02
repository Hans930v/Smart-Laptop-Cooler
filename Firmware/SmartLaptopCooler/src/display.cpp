#include "display.h"
#include "bitmaps.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

float   displayCpuTemp = -1;
float   displayGpuTemp = -1;
uint8_t displayPWM = 76;
bool    displayConnected = false;
bool    displaySafeMode = false;

// ===== Simple spinning fan-blade glyph, drawn procedurally (no bitmap needed) =====
// Draws a 4-blade spinner centered at (cx, cy) with given radius, rotated by angle step.
void drawFanSpinner(int cx, int cy, int radius, uint8_t step) {
  // 8 rotation steps -> full spin cycle. Each step draws blades at a rotated offset.
  float angle = (step % 8) * (PI / 4.0); // 45° per step

  for (int i = 0; i < 4; i++) {
    float a = angle + (i * PI / 2.0); // 4 blades, 90° apart
    int   x1 = cx + (int)(cos(a) * 2);
    int   y1 = cy + (int)(sin(a) * 2);
    int   x2 = cx + (int)(cos(a) * radius);
    int   y2 = cy + (int)(sin(a) * radius);
    u8g2.drawLine(x1, y1, x2, y2);
  }
  u8g2.drawCircle(cx, cy, 2); // hub
}

// ===== drawBrand: logo wipes/fades in, spinner "powers up" beside it =====
// Non-blocking: call repeatedly from setup() in a loop with millis() gating,
// OR call once per frame if you're already looping in setup(). See usage note below.
void drawBrand(void) {
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  int logoW = 52;
  int logoH = 56;
  int x = (128 - logoW) / 2;
  int y = (64 - logoH) / 2;

  const unsigned long ANIM_DURATION_MS = 2000; // total wipe-in time
  const unsigned long FRAME_MS = 40;           // ~25fps update rate
  unsigned long       startTime = millis();
  uint8_t             spinStep = 0;
  unsigned long       lastSpin = 0;

  while (millis() - startTime < ANIM_DURATION_MS) {
    unsigned long elapsed = millis() - startTime;
    int           revealHeight = map(elapsed, 0, ANIM_DURATION_MS, 0, logoH);
    revealHeight = constrain(revealHeight, 0, logoH);

    u8g2.clearBuffer();

    // Wipe-in: only draw the top N rows of the logo, growing each frame.
    // u8g2 doesn't crop XBMP natively, so we draw full then mask the unrevealed part.
    u8g2.drawXBMP(x, y, logoW, logoH, image_HansoyLogo_bits);
    if (revealHeight < logoH) {
      u8g2.setDrawColor(0); // erase (background color) to mask the "not yet revealed" bottom
      u8g2.drawBox(x, y + revealHeight, logoW, logoH - revealHeight);
      u8g2.setDrawColor(1);
    }

    // Small spinner bottom-right, ticks independently of the wipe
    if (millis() - lastSpin > 120) {
      lastSpin = millis();
      spinStep++;
    }
    drawFanSpinner(112, 54, 8, spinStep);

    u8g2.sendBuffer();
    delay(FRAME_MS); // boot sequence only — fine here since nothing else needs to run yet
  }

  // Final settled frame: full logo, spinner still gently turning for a moment
  for (int i = 0; i < 6; i++) {
    u8g2.clearBuffer();
    u8g2.drawXBMP(x, y, logoW, logoH, image_HansoyLogo_bits);
    drawFanSpinner(112, 54, 8, spinStep++);
    u8g2.sendBuffer();
    delay(120);
  }
}

// ===== drawModel: logo slides in from the side, spinner settles beside it, "ready" pulse =====
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

  // Slide in from off-screen left
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
    drawFanSpinner(10, 8, 6, spinStep); // small spinner top-left, "already running" cue

    u8g2.sendBuffer();
    delay(FRAME_MS);
  }

  // Settled: logo in place, spinner continues a bit, then a soft "ready" blink of the whole frame
  for (int i = 0; i < 5; i++) {
    u8g2.clearBuffer();
    u8g2.drawXBMP(finalX, y, logoW, logoH, image_logo_bits);
    drawFanSpinner(10, 8, 6, spinStep++);
    u8g2.sendBuffer();
    delay(100);
  }

  // Quick invert-flash to signal "ready" (bridges into dashboard/status screens next)
  u8g2.setDrawColor(2); // XOR mode - inverts what's there
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

void drawDashboard() {
  // unchanged from your version
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

  extern float smoothedPower;

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