#include <Arduino.h>
#include <SPI.h>

// ILI9341, 240 x 320. SPI1: PA5=SCK, PA6=MISO, PA7=MOSI.
static constexpr uint32_t TFT_CS = PB6;
static constexpr uint32_t TFT_DC = PC7;
static constexpr uint32_t TFT_RST = PB4;
static constexpr uint32_t TFT_BL = PB3;
static constexpr uint16_t TFT_WIDTH = 240;
static constexpr uint16_t TFT_HEIGHT = 320;

static const SPISettings tftSpi(8000000, MSBFIRST, SPI_MODE0);

static void command(uint8_t value) {
  digitalWrite(TFT_DC, LOW);
  SPI.transfer(value);
  digitalWrite(TFT_DC, HIGH);
}

static void data(uint8_t value) { SPI.transfer(value); }

static void commandData(uint8_t cmd, const uint8_t *values, size_t length) {
  command(cmd);
  for (size_t i = 0; i < length; ++i) data(values[i]);
}

static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     uint16_t color) {
  if (x >= TFT_WIDTH || y >= TFT_HEIGHT || w == 0 || h == 0) return;
  if (w > TFT_WIDTH - x) w = TFT_WIDTH - x;
  if (h > TFT_HEIGHT - y) h = TFT_HEIGHT - y;

  const uint8_t columns[] = {
      static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
      static_cast<uint8_t>((x + w - 1) >> 8), static_cast<uint8_t>(x + w - 1)};
  const uint8_t rows[] = {
      static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
      static_cast<uint8_t>((y + h - 1) >> 8), static_cast<uint8_t>(y + h - 1)};

  SPI.beginTransaction(tftSpi);
  digitalWrite(TFT_CS, LOW);
  commandData(0x2A, columns, sizeof(columns));
  commandData(0x2B, rows, sizeof(rows));
  command(0x2C);
  const uint8_t high = color >> 8;
  const uint8_t low = color;
  for (uint32_t i = 0; i < static_cast<uint32_t>(w) * h; ++i) {
    data(high);
    data(low);
  }
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
}

static void initDisplay() {
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_BL, LOW);

  SPI.setSCLK(PA5);
  SPI.setMISO(PA6);
  SPI.setMOSI(PA7);
  SPI.begin();

  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  SPI.beginTransaction(tftSpi);
  digitalWrite(TFT_CS, LOW);
  command(0x01);
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  delay(150);

  SPI.beginTransaction(tftSpi);
  digitalWrite(TFT_CS, LOW);
  const uint8_t power1[] = {0x23};
  const uint8_t power2[] = {0x10};
  const uint8_t vcom1[] = {0x3E, 0x28};
  const uint8_t vcom2[] = {0x86};
  const uint8_t madctl[] = {0x48};
  const uint8_t pixelFormat[] = {0x55};
  const uint8_t frameRate[] = {0x00, 0x18};
  const uint8_t displayFunction[] = {0x08, 0x82, 0x27};
  commandData(0xC0, power1, sizeof(power1));
  commandData(0xC1, power2, sizeof(power2));
  commandData(0xC5, vcom1, sizeof(vcom1));
  commandData(0xC7, vcom2, sizeof(vcom2));
  commandData(0x36, madctl, sizeof(madctl));
  commandData(0x3A, pixelFormat, sizeof(pixelFormat));
  commandData(0xB1, frameRate, sizeof(frameRate));
  commandData(0xB6, displayFunction, sizeof(displayFunction));
  command(0x11);
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  delay(120);

  SPI.beginTransaction(tftSpi);
  digitalWrite(TFT_CS, LOW);
  command(0x29);
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  delay(20);
}

void setup() {
  initDisplay();
  fillRect(0, 0, 240, 64, 0xFFFF);    // White
  fillRect(0, 64, 240, 64, 0xF800);   // Red
  fillRect(0, 128, 240, 64, 0x07E0); // Green
  fillRect(0, 192, 240, 64, 0x001F); // Blue
  fillRect(0, 256, 240, 64, 0xFFE0); // Yellow
  digitalWrite(TFT_BL, HIGH);
}

void loop() {}
