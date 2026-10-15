// CO5300 AMOLED via QSPI met ESP-IDF esp_lcd, asynchroon met DMA:
// terwijl stuk N via DMA naar het panel gaat, tekent LVGL al stuk N+1.
//
// Pijplijn per stuk (40 regels):  LVGL tekent (PSRAM)  ->  90° draaien naar een vrije DMA-buffer
// (intern)  ->  venster zetten + pixels async versturen  ->  LVGL mag meteen verder.
// LVGL tekent direct in RGB565_SWAPPED (big-endian, wat het panel wil): geen aparte byteswap-pass.

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
#define DESKBUDDY_QSPI_HZ 80000000  // terug naar 40000000 als het beeld niet goed is
#endif

namespace board {

namespace {

constexpr int kBufLines = 40;                         // 466 x 40 x 2 = 37 KB per buffer
constexpr size_t kBufBytes = LCD_WIDTH * kBufLines * 2;
constexpr int kColOffset = 6;                          // het panel begint op kolom 6 (Waveshare)
// Het bord zit 90° gedraaid in de behuizing: de fysieke bovenkant van het panel wijst naar links.
// ROTATION_270 legt de logische bovenkant op de fysieke rechterkant, zodat het beeld rechtop staat.
constexpr lv_display_rotation_t kRotation = LV_DISPLAY_ROTATION_270;

esp_lcd_panel_io_handle_t io = nullptr;
uint8_t* txBuf[2] = {nullptr, nullptr};  // DMA-buffers (intern), afwisselend gebruikt
int txCur = 0;
SemaphoreHandle_t txFree = nullptr;       // telt vrije DMA-buffers

// Prestaties (zie takeFrameStats)
volatile uint32_t frames = 0;
volatile uint64_t renderUs = 0, rotateUs = 0, sendUs = 0;
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
  int64_t t0 = esp_timer_get_time();
  xSemaphoreTake(txFree, portMAX_DELAY);  // wacht tot een DMA-buffer vrij is
  int64_t t1 = esp_timer_get_time();

  lv_area_t a = *area;
  int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  lv_display_rotate_area(disp, &a);
  const lv_color_format_t cf = LV_COLOR_FORMAT_RGB565;  // zelfde pixelgrootte als RGB565_SWAPPED
  lv_draw_sw_rotate(px, txBuf[txCur], w, h, lv_draw_buf_width_to_stride(w, cf),
                    lv_draw_buf_width_to_stride(lv_area_get_width(&a), cf), kRotation, cf);
  int64_t t2 = esp_timer_get_time();

  // LVGL mag direct het volgende stuk tekenen: de pixels staan nu in onze DMA-buffer.
  lv_display_flush_ready(disp);

  setWindow(a.x1, a.y1, a.x2, a.y2);  // wacht intern tot de vorige transfer klaar is
  esp_lcd_panel_io_tx_color(io, cmdWord(0x32, 0x2C), txBuf[txCur], (size_t)w * h * 2);  // async
  txCur ^= 1;
  int64_t t3 = esp_timer_get_time();

  rotateUs += (uint64_t)(t2 - t1);
  sendUs += (uint64_t)((t1 - t0) + (t3 - t2));  // wachten op het panel (niet overlapt)
}

// De CO5300 accepteert alleen vensters met even start- en oneven eindcoördinaten.
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

  // Hardware-reset en init-reeks (overgenomen uit Arduino_GFX Arduino_CO5300)
  pinMode(LCD_RESET, OUTPUT);
  digitalWrite(LCD_RESET, HIGH);
  delay(10);
  digitalWrite(LCD_RESET, LOW);
  delay(200);
  digitalWrite(LCD_RESET, HIGH);
  delay(200);
  writeCmd(0x11);  // sleep out
  delay(120);
  const uint8_t v00 = 0x00, v80 = 0x80, v55 = 0x55, v20 = 0x20, vFF = 0xFF;
  writeCmd(0xFE, &v00, 1);
  writeCmd(0xC4, &v80, 1);  // SPI-modus
  writeCmd(0x3A, &v55, 1);  // 16 bit per pixel
  writeCmd(0x53, &v20, 1);
  writeCmd(0x63, &vFF, 1);  // helderheid HBM
  writeCmd(0x36, &v00, 1);  // MADCTL: RGB, geen spiegeling
  writeCmd(0x20);           // inversie uit
  writeCmd(0x29);           // display aan
  writeCmd(0x58, &v00, 1);  // high-contrast uit
  setBrightness(0);         // eerst donker, LVGL tekent het eerste frame zwart
  delay(10);
  return true;
}

}  // namespace

bool beginDisplay() {
  txBuf[0] = (uint8_t*)heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  txBuf[1] = (uint8_t*)heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* drawBuf = heap_caps_malloc(kBufBytes, MALLOC_CAP_SPIRAM);  // LVGL tekent hierin
  if (!txBuf[0] || !txBuf[1] || !drawBuf) return false;
  txFree = xSemaphoreCreateCounting(2, 2);
  if (!initPanel()) return false;

  // Scherm leegmaken (zwart), in stukken
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
  lv_display_set_rotation(disp, kRotation);
  lv_display_set_buffers(disp, drawBuf, nullptr, kBufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_add_event_cb(disp, rounderCb, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_display_add_event_cb(disp, refrStartCb, LV_EVENT_REFR_START, nullptr);
  lv_display_add_event_cb(disp, refrReadyCb, LV_EVENT_REFR_READY, nullptr);
  return true;
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
