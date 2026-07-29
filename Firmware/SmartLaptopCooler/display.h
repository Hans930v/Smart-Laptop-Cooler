#pragma once
#include <U8g2lib.h>

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// Display state shared with main loop / fan control
extern float   displayCpuTemp;
extern float   displayGpuTemp;
extern uint8_t displayPWM;
extern bool    displayConnected;
extern bool    displaySafeMode;
extern bool    wasConnected;

void drawBrand();
void drawModel();
void drawStatus(const char *msg);
void drawDashboard();