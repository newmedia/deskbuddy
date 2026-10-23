#pragma once
// Pins and panel settings per board (see docs/hardware.md and docs/xl-port.md). Every pin the firmware
// uses is the same on both boards; the XL (env esp32-xl) differs in its panel.

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39

#define I2C_SDA 15
#define I2C_SCL 14

#define TP_INT 11
#define TP_RESET 40
#define TP_ADDR 0x5A

#define BOOT_BUTTON 0

#ifdef DESKBUDDY_BOARD_XL
// Waveshare ESP32-S3-Touch-AMOLED-2.16: square 480x480 CO5300, touch CST9220
#define LCD_WIDTH 480
#define LCD_HEIGHT 480
#define LCD_COL_OFFSET 0
// The panel rotates the picture itself (MADCTL): 0x00, 0x60, 0xC0 or 0xA0 (90° steps). 0xA0 is the
// orientation of Waveshare's BSP; the right one depends on how the board sits in the XL case.
#define LCD_MADCTL 0xA0
#define LCD_SW_ROTATION 0
#define KEY3_BUTTON 18  // extra user button (pull-up, low when pressed); not used yet
#else
// Waveshare ESP32-S3-Touch-AMOLED-1.75: round 466x466 CO5300, touch CST9217
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define LCD_COL_OFFSET 6  // the panel starts at column 6
#define LCD_MADCTL 0x00
// The board sits rotated by 90° in the case: LVGL rotates every area in software (ROTATION_270).
#define LCD_SW_ROTATION 1
#endif
