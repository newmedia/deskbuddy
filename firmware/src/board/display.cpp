// CO5300 AMOLED via QSPI with ESP-IDF esp_lcd, asynchronous with DMA:
// while chunk N goes to the panel via DMA, LVGL is already drawing chunk N+1.
//
// Pipeline per chunk (10 lines):  LVGL draws (PSRAM)  ->  rotate 90° into a free DMA buffer
// (internal)  ->  set window + send pixels async  ->  LVGL may continue immediately.
// On the XL the panel rotates itself (MADCTL), so the chunk is only copied instead of rotated.
// LVGL draws directly in RGB565_SWAPPED (big-endian, what the panel wants): no separate byteswap pass.

#include <Arduino.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include "board.h"
#include "pins.h"

#ifndef DESKBUDDY_QSPI_HZ
#define DESKBUDDY_QSPI_HZ 80000000  // back to 40000000 if the picture is not right
#endif

namespace board {

namespace {

// DMA buffers: 466 x 10 x 2 = 9.3 KB per chunk (XL: 480 x 10 x 2 = 9.6 KB), 2 = 18.6 KB internal. Larger doesn't fit next to Wi-Fi + BLE
// (the BT controller itself already holds a good part of the internal RAM). LVGL's draw buffer is
// full-screen and lives in PSRAM: then LVGL draws each changed area in one go, instead of walking the
// whole face drawing again per strip of 20 lines.
constexpr int kBufLines = 10;
constexpr size_t kBufBytes = LCD_WIDTH * kBufLines * 2;
constexpr size_t kDrawBufBytes = LCD_WIDTH * LCD_HEIGHT * 2;
constexpr int kColOffset = LCD_COL_OFFSET;              // first column of the panel (pins.h)
#if LCD_SW_ROTATION
// The board sits rotated 90° in the enclosure: the physical top of the panel points left.
// ROTATION_270 puts the logical top on the physical right side, so the picture stands upright.
constexpr lv_display_rotation_t kRotation = LV_DISPLAY_ROTATION_270;
#endif

esp_lcd_panel_io_handle_t io = nullptr;
uint8_t* txBuf[2] = {nullptr, nullptr};  // DMA buffers (internal), used alternately
int txCur = 0;
SemaphoreHandle_t txFree = nullptr;       // counts free DMA buffers

// Performance (see takeFrameStats)
volatile uint32_t frames = 0;
volatile uint64_t renderUs = 0, rotateUs = 0, sendUs = 0;
volatile uint32_t timeouts = 0;  // number of times a DMA buffer was not free in time
int64_t refrStart = 0;

inline uint32_t cmdWord(uint8_t op, uint8_t cmd) { return ((uint32_t)op << 24) | ((uint32_t)cmd << 8); }

void writeCmd(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0) {
  esp_lcd_panel_io_tx_param(io, cmdWord(0x02, cmd), data, len);
}

void setWindow(int x1, int y1, int x2, int y2) {
  x1 += kColOffset;
  x2 += kColOffset;
  uint8_t ca[4] = {(uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2};
  uint8_t ra[4] = {(uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2};
  writeCmd(0x2A, ca, 4);  // CASET
  writeCmd(0x2B, ra, 4);  // RASET
}

bool IRAM_ATTR onTxDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(txFree, &woken);
  return woken == pdTRUE;
}

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  // LVGL draws the whole changed area in one go (draw buffer in PSRAM); we rotate and send
  // it in strips of kBufLines lines via the two internal DMA buffers.
  const lv_color_format_t cf = LV_COLOR_FORMAT_RGB565;  // same pixel size as RGB565_SWAPPED
  const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  const uint32_t srcStride = lv_draw_buf_width_to_stride(w, cf);
  for (int32_t y = 0; y < h; y += kBufLines) {
    const int32_t sh = LV_MIN(kBufLines, h - y);
    int64_t t0 = esp_timer_get_time();
    // Wait until a DMA buffer is free, but never forever: if a "done" notification is lost (e.g.
    // during flash writes in an OTA update), continue after 50 ms anyway.
    if (xSemaphoreTake(txFree, pdMS_TO_TICKS(50)) != pdTRUE) timeouts++;
    int64_t t1 = esp_timer_get_time();

    lv_area_t a = {area->x1, area->y1 + y, area->x2, area->y1 + y + sh - 1};
#if LCD_SW_ROTATION
    lv_display_rotate_area(disp, &a);
    lv_draw_sw_rotate(px + y * srcStride, txBuf[txCur], w, sh, srcStride,
                      lv_draw_buf_width_to_stride(lv_area_get_width(&a), cf), kRotation, cf);
#else
    memcpy(txBuf[txCur], px + y * srcStride, (size_t)srcStride * sh);  // panel rotates: copy only
#endif
    int64_t t2 = esp_timer_get_time();

    setWindow(a.x1, a.y1, a.x2, a.y2);  // waits internally until the previous transfer is done
    esp_lcd_panel_io_tx_color(io, cmdWord(0x32, 0x2C), txBuf[txCur], (size_t)w * sh * 2);  // async
    txCur ^= 1;
    int64_t t3 = esp_timer_get_time();
    rotateUs += (uint64_t)(t2 - t1);  // XL: the copy into the DMA buffer
    sendUs += (uint64_t)((t1 - t0) + (t3 - t2));  // waiting for the panel (not overlapped)
  }
  lv_display_flush_ready(disp);
}

// The CO5300 only accepts windows with even start and odd end coordinates.
void rounderCb(lv_event_t* e) {
  auto* r = (lv_area_t*)lv_event_get_param(e);
  r->x1 &= ~1;
  r->y1 &= ~1;
  r->x2 |= 1;
  r->y2 |= 1;
}

void refrStartCb(lv_event_t*) { refrStart = esp_timer_get_time(); }
void refrReadyCb(lv_event_t*) {
  frames++;
  renderUs += (uint64_t)(esp_timer_get_time() - refrStart);
}

uint32_t tickCb() { return millis(); }

bool initPanel() {
  spi_bus_config_t bus = {};
  bus.data0_io_num = LCD_SDIO0;
  bus.data1_io_num = LCD_SDIO1;
  bus.data2_io_num = LCD_SDIO2;
  bus.data3_io_num = LCD_SDIO3;
  bus.sclk_io_num = LCD_SCLK;
  bus.max_transfer_sz = kBufBytes + 16;
  bus.flags = SPICOMMON_BUSFLAG_QUAD;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

  esp_lcd_panel_io_spi_config_t cfg = {};
  cfg.cs_gpio_num = LCD_CS;
  cfg.dc_gpio_num = -1;
  cfg.spi_mode = 0;
  cfg.pclk_hz = DESKBUDDY_QSPI_HZ;
  cfg.trans_queue_depth = 4;
  cfg.on_color_trans_done = onTxDone;
  cfg.lcd_cmd_bits = 32;
  cfg.lcd_param_bits = 8;
  cfg.flags.quad_mode = 1;
  if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &cfg, &io) != ESP_OK) return false;

  // Hardware reset and init sequence (Original: from Arduino_GFX Arduino_CO5300; XL: additions from
  // Waveshare's ESP-IDF BSP for the 2.16)
  pinMode(LCD_RESET, OUTPUT);
  digitalWrite(LCD_RESET, HIGH);
  delay(10);
  digitalWrite(LCD_RESET, LOW);
  delay(200);
  digitalWrite(LCD_RESET, HIGH);
  delay(200);
  writeCmd(0x11);  // sleep out
  delay(120);
  const uint8_t v00 = 0x00, v80 = 0x80, v55 = 0x55, v20 = 0x20, vFF = 0xFF, madctl = LCD_MADCTL;
#ifdef DESKBUDDY_BOARD_XL
  const uint8_t v10 = 0x10, vA0 = 0xA0;
  writeCmd(0xFE, &v20, 1);  // manufacturer page 0x20
  writeCmd(0x19, &v10, 1);
  writeCmd(0x1C, &vA0, 1);
#endif
  writeCmd(0xFE, &v00, 1);
  writeCmd(0xC4, &v80, 1);  // SPI mode
  writeCmd(0x3A, &v55, 1);  // 16 bits per pixel
  writeCmd(0x53, &v20, 1);
  writeCmd(0x63, &vFF, 1);  // brightness HBM
  writeCmd(0x36, &madctl, 1);  // MADCTL: RGB; XL: orientation of the panel (pins.h)
  writeCmd(0x20);           // inversion off
  writeCmd(0x29);           // display on
  writeCmd(0x58, &v00, 1);  // high-contrast off
  setBrightness(0);         // dark first, LVGL draws the first frame black
  delay(10);
  return true;
}

}  // namespace

bool beginDisplay() {
  txBuf[0] = (uint8_t*)heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  txBuf[1] = (uint8_t*)heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* drawBuf = heap_caps_malloc(kDrawBufBytes, MALLOC_CAP_SPIRAM);  // LVGL draws into this
  if (!txBuf[0] || !txBuf[1] || !drawBuf) return false;
  txFree = xSemaphoreCreateCounting(2, 2);
  if (!initPanel()) return false;

  // Clear the screen (black), in chunks
  memset(txBuf[0], 0, kBufBytes);
  for (int y = 0; y < LCD_HEIGHT; y += kBufLines) {
    int y2 = LV_MIN(y + kBufLines, LCD_HEIGHT) - 1;
    xSemaphoreTake(txFree, portMAX_DELAY);
    setWindow(0, y, LCD_WIDTH - 1, y2);
    esp_lcd_panel_io_tx_color(io, cmdWord(0x32, 0x2C), txBuf[0], (size_t)LCD_WIDTH * (y2 - y + 1) * 2);
  }
  setBrightness(200);

  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t* disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
#if LCD_SW_ROTATION
  lv_display_set_rotation(disp, kRotation);
#endif
  lv_display_set_buffers(disp, drawBuf, nullptr, kDrawBufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_add_event_cb(disp, rounderCb, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_display_add_event_cb(disp, refrStartCb, LV_EVENT_REFR_START, nullptr);
  lv_display_add_event_cb(disp, refrReadyCb, LV_EVENT_REFR_READY, nullptr);
  return true;
}

uint32_t displayTimeouts() { return timeouts; }

void setRefreshPeriod(uint32_t ms) {
  static uint32_t current = 0;
  if (ms == current) return;
  current = ms;
  lv_timer_t* t = lv_display_get_refr_timer(lv_display_get_default());
  if (t) lv_timer_set_period(t, ms);
}

void setBrightness(uint8_t v) {
  if (io) writeCmd(0x51, &v, 1);
}

void takeFrameStats(uint32_t& n, uint64_t& us, uint64_t& rotUs, uint64_t& txUs) {
  n = frames;
  us = renderUs;
  rotUs = rotateUs;
  txUs = sendUs;
  frames = 0;
  renderUs = rotateUs = sendUs = 0;
}

}  // namespace board
