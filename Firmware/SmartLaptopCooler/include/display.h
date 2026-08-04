/*
 * display.h - OLED instance, dashboard state, and rendering function prototypes.
 *
 * Exposes the SH1106 U8g2 instance plus the display-state globals shared with
 * main.cpp/fan_control.cpp (sensor readings, PWM, connection flags, runtime
 * status overlay enum + boost tag), and the prototypes for the boot animation
 * and live dashboard draw functions, including resetMaxTrackers() used by main
 * to start fresh bar scales on BT reconnect.
 */
#pragma once
#include <U8g2lib.h>

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

extern float   displayCpuTemp;
extern float   displayGpuTemp;
extern uint8_t displayPWM;
extern bool    displayConnected;
extern bool    displaySafeMode;
extern bool    wasConnected;

enum class DashStatus : uint8_t { NORMAL, BOOST, RECONNECTING, SAFE_MODE, EMERGENCY };
extern DashStatus displayStatus;
extern char       displayBoostTag[8];

void drawBrand();
void drawModel();
void drawStatus(const char *msg);
void drawDashboard();
void resetMaxTrackers();
