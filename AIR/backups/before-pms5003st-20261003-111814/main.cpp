#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

namespace {
// Wiring from the supplied diagram, using STM32 pin names explicitly.
constexpr uint8_t TFT_CS = PB6;   // Arduino D10
constexpr uint8_t TFT_DC = PB5;   // Arduino D4
constexpr uint8_t TFT_RST = PA8;  // Arduino D7, LCD reset (not MCU NRST)
constexpr uint32_t TFT_SPI_HZ = 8000000;
constexpr uint16_t BACKGROUND = 0x0841;

// LED is wired directly to 3V3; there is no backlight GPIO.
// SDO/MISO is unconnected. No LCD register reads are performed.
Adafruit_ILI9341 lcd(&SPI, TFT_DC, TFT_CS, TFT_RST);
uint32_t lastUpdateMs = 0;
uint32_t seconds = 0;

void drawTestPage() {
  lcd.fillScreen(BACKGROUND);
  lcd.setTextWrap(false);
  lcd.setTextSize(3);
  lcd.setTextColor(ILI9341_CYAN);
  lcd.setCursor(16, 16);
  lcd.print(F("AIR LCD TEST"));

  lcd.setTextSize(2);
  lcd.setTextColor(ILI9341_WHITE);
  lcd.setCursor(16, 54);
  lcd.print(F("NUCLEO-F411RE"));
  lcd.setCursor(16, 78);
  lcd.print(F("ILI9341  320 x 240"));

  const uint16_t colors[] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE,
                             ILI9341_CYAN, ILI9341_MAGENTA, ILI9341_YELLOW};
  for (uint8_t i = 0; i < 6; ++i) {
    lcd.fillRect(16 + i * 48, 112, 48, 48, colors[i]);
  }
  lcd.drawRect(15, 111, 290, 50, ILI9341_WHITE);

  lcd.setTextSize(1);
  lcd.setCursor(16, 174);
  lcd.print(F("CS:PB6  DC:PB5  RESET:PA8"));
  lcd.setCursor(16, 190);
  lcd.print(F("SCK:PA5  MOSI:PA7  LED:3V3"));

  lcd.setTextSize(2);
  lcd.setCursor(16, 214);
  lcd.print(F("RUN: 0 s"));
  lcd.fillCircle(296, 221, 6, ILI9341_GREEN);
}

void colorTest() {
  const uint16_t colors[] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE,
                             ILI9341_WHITE, ILI9341_BLACK};
  const char *names[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
  for (uint8_t i = 0; i < 5; ++i) {
    Serial.print(F("LCD color test: "));
    Serial.println(names[i]);
    lcd.fillScreen(colors[i]);
    delay(400);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);  // ST-Link virtual COM port, USART2 PA2/PA3.
  delay(200);           // Allow the screen supply to settle after power-up.
  Serial.println(F("\r\nAIR LCD TEST boot"));
  Serial.println(F("CS=PB6 DC=PB5 RESET=PA8 SCK=PA5 MOSI=PA7"));
  Serial.println(F("LED=3V3; MISO not connected; write-only display"));

  // Do not blink LED_BUILTIN: on this board PA5 is also SPI1 SCK.
  SPI.setSCLK(PA5);
  SPI.setMOSI(PA7);
  SPI.setMISO(PA6);  // SPI1 input only; no physical MISO wire is required.

  // The driver performs hardware reset, power/gamma configuration,
  // RGB565 setup, sleep-out delay and display-on using SPI mode 0.
  lcd.begin(TFT_SPI_HZ);
  lcd.setRotation(1);  // Landscape, 320 x 240.
  lcd.invertDisplay(false);
  colorTest();
  drawTestPage();

  lastUpdateMs = millis();
  Serial.println(F("LCD test page sent. Check the physical display."));
}

void loop() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastUpdateMs) < 1000) {
    return;
  }
  const uint32_t elapsedSeconds = (now - lastUpdateMs) / 1000;
  lastUpdateMs += elapsedSeconds * 1000;
  seconds += elapsedSeconds;

  // Redraw only the changing region, leaving the test pattern stable.
  lcd.fillRect(16, 212, 264, 20, BACKGROUND);
  lcd.setTextSize(2);
  lcd.setTextColor(ILI9341_WHITE, BACKGROUND);
  lcd.setCursor(16, 214);
  lcd.print(F("RUN: "));
  lcd.print(seconds);
  lcd.print(F(" s"));
  lcd.fillCircle(296, 221, 6,
                 (seconds & 1) ? ILI9341_DARKGREEN : ILI9341_GREEN);

  Serial.print(F("LCD heartbeat: "));
  Serial.print(seconds);
  Serial.println(F(" s"));
}
