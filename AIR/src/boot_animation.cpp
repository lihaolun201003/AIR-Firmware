#include "boot_animation.h"
#include "air_config.h"
#include "boot_character_data.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <algorithm>

namespace {
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t INK = 0x1082;
constexpr uint16_t GREY = 0xAD55;
constexpr uint16_t TRACK = 0xE71C;
constexpr uint16_t TEAL = 0x04B2;
constexpr uint32_t DURATION_MS = 3000;
constexpr int16_t REST_X = 6;
constexpr int16_t ENTRY_X = -24;
constexpr int16_t REST_Y = 12;
constexpr int16_t STRIP_ROWS = 8;
static uint8_t strip[320 * STRIP_ROWS * 2];

static_assert(sizeof(BOOT_CHARACTER_PIXELS) ==
              (BOOT_CHARACTER_WIDTH * BOOT_CHARACTER_HEIGHT + 1) / 2,
              "Boot image dimensions must match the generated data");

uint16_t characterPixel(int16_t x, int16_t y) {
  if (x < 0 || y < 0 || x >= BOOT_CHARACTER_WIDTH ||
      y >= BOOT_CHARACTER_HEIGHT) return WHITE;
  const uint32_t index = y * BOOT_CHARACTER_WIDTH + x;
  const uint8_t packed = BOOT_CHARACTER_PIXELS[index / 2];
  const uint8_t shade = ((index & 1) ? (packed & 15) : (packed >> 4)) * 17;
  return ((shade >> 3) << 11) | ((shade >> 2) << 5) | (shade >> 3);
}

// Compose both the old and new bounds in one pass. No erase-then-draw flash,
// and only a 5 KiB strip buffer instead of a full-screen RGB framebuffer.
void renderRegion(Adafruit_ILI9341 &lcd, int16_t left, int16_t top,
                  int16_t right, int16_t bottom, int16_t spriteX,
                  int16_t spriteY, void (*service)()) {
  left = std::max<int16_t>(0, left);
  top = std::max<int16_t>(0, top);
  right = std::min<int16_t>(320, right);
  bottom = std::min<int16_t>(240, bottom);
  const int16_t width = right - left;
  if (width <= 0 || bottom <= top) return;

  for (int16_t y = top; y < bottom; y += STRIP_ROWS) {
    const int16_t rows = std::min<int16_t>(STRIP_ROWS, bottom - y);
    size_t count = 0;
    for (int16_t row = 0; row < rows; ++row) {
      for (int16_t x = left; x < right; ++x) {
        const uint16_t pixel = characterPixel(x - spriteX, y + row - spriteY);
        strip[count++] = pixel >> 8;
        strip[count++] = pixel & 0xFF;
      }
    }
    lcd.startWrite();
    lcd.setAddrWindow(left, y, width, rows);
    // STM32 bulk transfer sends the prepared RGB565 bytes in wire order.
    // setAddrWindow() leaves DC high for pixel data; the same SPI1 is used.
    SPI.transfer(static_cast<const void *>(strip), nullptr, count);
    lcd.endWrite();
    service();
  }
}

void label(Adafruit_ILI9341 &lcd, int16_t x, int16_t y, uint8_t size,
           uint16_t color, const char *text) {
  lcd.setCursor(x, y);
  lcd.setTextSize(size);
  lcd.setTextColor(color);
  lcd.print(text);
}
}  // namespace

void playBootAnimation(Adafruit_ILI9341 &lcd, void (*service)()) {
#if !AIR_ENABLE_BLUETOOTH
  Serial.println(F("BOOT animation: start"));
  const uint32_t totalStart = millis();
#endif
  lcd.setTextWrap(false);
  // The display is still OFF: prepare a real first frame, not an empty white
  // page. The character is already visible at the left when DISPON is sent.
  renderRegion(lcd, 0, 0, 320, 240, ENTRY_X, REST_Y, service);
  label(lcd, 178, 64, 4, INK, "AIR");
  label(lcd, 178, 105, 2, INK, "MONITOR");
  label(lcd, 178, 137, 1, GREY, "HELLO!");
  label(lcd, 178, 202, 1, GREY, "STARTING UP");
  lcd.fillRoundRect(176, 183, 120, 5, 2, TRACK);
  for (uint8_t dot = 0; dot < 3; ++dot) {
    lcd.fillCircle(186 + dot * 18, 165, 3, dot == 0 ? TEAL : TRACK);
  }
  lcd.sendCommand(ILI9341_DISPON);
#if !AIR_ENABLE_BLUETOOTH
  Serial.print(F("LCD first complete frame enabled at uptime_ms="));
  Serial.println(millis());
#endif

  const uint32_t start = millis();
  uint32_t nextFrame = 0;
#if !AIR_ENABLE_BLUETOOTH
  uint32_t renderedFrames = 0;
#endif
  int16_t oldX = ENTRY_X, oldY = REST_Y;
  const int8_t bob[] = {0, 0, -1, -2, -3, -3, -2, -1,
                       0, 0, 1, 2, 3, 3, 2, 1};
  while (static_cast<uint32_t>(millis() - start) < DURATION_MS) {
    service();
    const uint32_t elapsed = millis() - start;
    if (elapsed >= nextFrame) {
      int16_t x = REST_X, y = REST_Y;
      if (elapsed < 700) {
        const float remaining = 1.0f - elapsed / 700.0f;
        x -= static_cast<int16_t>((REST_X - ENTRY_X) *
                                 remaining * remaining * remaining);
      } else {
        y += bob[((elapsed - 700) / 80) % 16];
      }
      if (x != oldX || y != oldY) {
        renderRegion(lcd, std::min(oldX, x), std::min(oldY, y),
                     std::max(oldX, x) + BOOT_CHARACTER_WIDTH,
                     std::max(oldY, y) + BOOT_CHARACTER_HEIGHT,
                     x, y, service);
        oldX = x;
        oldY = y;
      }
      for (uint8_t dot = 0; dot < 3; ++dot) {
        lcd.fillCircle(186 + dot * 18, 165, 3,
                       ((elapsed / 180) % 3 == dot) ? TEAL : TRACK);
      }
      const int16_t progress = std::min<uint32_t>(120, elapsed * 120 / DURATION_MS);
      if (progress) lcd.fillRect(176, 183, progress, 5, TEAL);
#if !AIR_ENABLE_BLUETOOTH
      ++renderedFrames;
#endif
      nextFrame = elapsed + 50;
    }
    delay(1);
  }
  lcd.fillRect(176, 183, 120, 5, TEAL);
  const uint32_t hold = millis();
  while (static_cast<uint32_t>(millis() - hold) < 120) {
    service();
    delay(1);
  }
#if !AIR_ENABLE_BLUETOOTH
  Serial.print(F("BOOT animation: done duration_ms="));
  Serial.print(millis() - totalStart);
  Serial.print(F(" frames="));
  Serial.println(renderedFrames);
#endif
}
