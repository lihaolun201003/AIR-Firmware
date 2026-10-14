#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <stdio.h>
#include "air_config.h"
#include "air_bluetooth.h"
#include "pms5003st.h"
#include "boot_animation.h"
#include "boot_display.h"

namespace {
constexpr uint8_t TFT_CS = PB6;   // D10
constexpr uint8_t TFT_DC = PB5;   // D4
constexpr uint8_t TFT_RST = PA8;  // D7
constexpr uint32_t TFT_SPI_HZ = 8000000;
constexpr uint32_t STALE_MS = AirTelemetry::STALE_MS;
constexpr uint32_t WARMUP_MS = AirTelemetry::WARMUP_MS;
constexpr uint16_t BACKGROUND = 0x0841;
constexpr uint16_t CARD = 0x10C4;
constexpr uint16_t MUTED = 0xAD55;

BootDisplay lcd(&SPI, TFT_DC, TFT_CS, TFT_RST);
// STM32duino 3.0 calls its concrete serial class Uart.
// PA10 receives sensor TXD. PA9 is the required UART TX pin but is UNCONNECTED.
// Never use USART1's automatic pin selection: PB6 is already LCD CS.
Uart sensor(PA10, PA9);
Pms5003stParser parser;
PmsReading latest;
#if AIR_ENABLE_BLUETOOTH
AirBluetooth bluetooth;
#endif
bool hasFrame = false;
uint32_t receivedBytes = 0;
uint32_t lastFrameMs = 0;
uint32_t sessionStartMs = 0;
uint32_t lastUiMs = 0;
#if !AIR_ENABLE_BLUETOOTH
uint32_t lastLogMs = 0;
#endif
uint32_t displayedFrame = 0;
bool displayedValues = false;

AirTelemetry telemetry() {
  AirTelemetry sample;
  sample.reading = latest;
  sample.hasFrame = hasFrame;
  sample.lastFrameMs = lastFrameMs;
  sample.sessionStartMs = sessionStartMs;
  sample.frames = parser.frames;
  sample.badFrames = parser.checksumErrors + parser.lengthErrors;
  return sample;
}

void textAt(int16_t x, int16_t y, uint8_t size, uint16_t color,
            const char *text) {
  lcd.setCursor(x, y);
  lcd.setTextSize(size);
  lcd.setTextColor(color);
  lcd.print(text);
}

void tenths(char *out, size_t capacity, int32_t value) {
  const bool negative = value < 0;
  const uint32_t magnitude = negative ? -value : value;
  snprintf(out, capacity, "%s%lu.%lu", negative ? "-" : "",
           static_cast<unsigned long>(magnitude / 10),
           static_cast<unsigned long>(magnitude % 10));
}

void hchoText(char *out, size_t capacity, uint16_t value) {
  snprintf(out, capacity, "%u.%03u", static_cast<unsigned>(value / 1000),
           static_cast<unsigned>(value % 1000));
}

void valueAt(int16_t x, int16_t y, int16_t width, int16_t height,
             uint8_t size, const char *text, uint16_t color = ILI9341_WHITE) {
  lcd.fillRect(x, y, width, height, CARD);
  textAt(x, y, size, color, text);
}

void drawLayout() {
  lcd.fillScreen(BACKGROUND);
  lcd.setTextWrap(false);
  textAt(10, 8, 2, ILI9341_CYAN, "AIR MONITOR");
  textAt(232, 12, 1, MUTED, "PMS5003ST");
  lcd.fillRoundRect(8, 48, 304, 64, 4, CARD);
  textAt(18, 54, 1, MUTED, "PM2.5  ug/m3");
  textAt(184, 57, 1, MUTED, "PM1.0");
  textAt(184, 85, 1, MUTED, "PM10");
  lcd.fillRoundRect(8, 120, 148, 48, 4, CARD);
  lcd.fillRoundRect(164, 120, 148, 48, 4, CARD);
  textAt(18, 126, 1, MUTED, "TEMP (C)");
  textAt(174, 126, 1, MUTED, "HUMIDITY (%)");
  lcd.fillRoundRect(8, 176, 304, 32, 4, CARD);
  textAt(18, 188, 1, MUTED, "HCHO");
  textAt(248, 188, 1, MUTED, "mg/m3");
}

void drawValues(bool visible) {
  char value[20];
  if (visible) snprintf(value, sizeof(value), "%u", latest.pm25);
  else snprintf(value, sizeof(value), "--");
  valueAt(18, 72, 144, 32, 4, value, ILI9341_CYAN);

  if (visible) snprintf(value, sizeof(value), "%u", latest.pm1);
  valueAt(244, 54, 60, 16, 2, value);
  if (visible) snprintf(value, sizeof(value), "%u", latest.pm10);
  valueAt(244, 82, 60, 16, 2, value);

  if (visible) tenths(value, sizeof(value), latest.temperatureTenths);
  valueAt(18, 144, 132, 16, 2, value);
  if (visible) tenths(value, sizeof(value), latest.humidityTenths);
  valueAt(174, 144, 132, 16, 2, value);
  if (visible) hchoText(value, sizeof(value), latest.hchoUgM3);
  valueAt(100, 184, 140, 16, 2, value);
}

void drawStatus(uint32_t now) {
  char status[52];
  uint16_t color = ILI9341_GREEN;
  const AirStatus state = telemetry().status(now);
  lcd.fillRect(8, 32, 304, 8, BACKGROUND);
#if AIR_ENABLE_BLUETOOTH
  textAt(10, 32, 1, MUTED, bluetooth.streamEnabled()
      ? "Bluetooth: streaming" : "Bluetooth: paused");
#else
  textAt(10, 32, 1, MUTED, "PA10 / D2   9600 8N1   PM: ATM");
#endif
  if (state == AirStatus::Waiting) {
    snprintf(status, sizeof(status), "WAITING: check sensor power / TXD -> D2");
    color = ILI9341_YELLOW;
  } else if (state == AirStatus::NoData) {
    snprintf(status, sizeof(status), "NO DATA: connection lost, readings cleared");
    color = ILI9341_ORANGE;
  } else if (state == AirStatus::SensorError) {
    snprintf(status, sizeof(status), "SENSOR ERROR: 0x%02X", latest.error);
    color = ILI9341_ORANGE;
  } else if (state == AirStatus::Warmup) {
    const uint32_t left = (WARMUP_MS - (now - sessionStartMs) + 999) / 1000;
    snprintf(status, sizeof(status), "WARMUP: %lu s   receiving valid data",
             static_cast<unsigned long>(left));
    color = ILI9341_YELLOW;
  } else {
    snprintf(status, sizeof(status), "LIVE: sensor data / checksum OK");
  }
  lcd.fillRect(8, 216, 304, 8, BACKGROUND);
  textAt(10, 216, 1, color, status);

  const uint32_t bad = parser.checksumErrors + parser.lengthErrors;
  if (hasFrame) {
    snprintf(status, sizeof(status), "Frames:%lu  Age:%lus  Bad:%lu",
             static_cast<unsigned long>(parser.frames),
             static_cast<unsigned long>((now - lastFrameMs) / 1000),
             static_cast<unsigned long>(bad));
  } else {
    snprintf(status, sizeof(status), "RX bytes:%lu  Bad:%lu  Waiting for 40 bytes",
             static_cast<unsigned long>(receivedBytes),
             static_cast<unsigned long>(bad));
  }
  lcd.fillRect(8, 230, 304, 8, BACKGROUND);
  textAt(10, 230, 1, MUTED, status);
}

void logReading() {
#if !AIR_ENABLE_BLUETOOTH
  char temperature[16], humidity[16], hcho[16], line[180];
  tenths(temperature, sizeof(temperature), latest.temperatureTenths);
  tenths(humidity, sizeof(humidity), latest.humidityTenths);
  hchoText(hcho, sizeof(hcho), latest.hchoUgM3);
  snprintf(line, sizeof(line),
           "PMS frame=%lu PM1=%u PM2.5=%u PM10=%u ug/m3 TEMP=%s C RH=%s %% HCHO=%s mg/m3 FW=%u ERR=%u",
           static_cast<unsigned long>(parser.frames), latest.pm1, latest.pm25,
           latest.pm10, temperature, humidity, hcho, latest.version, latest.error);
  Serial.println(line);
  if (parser.frames <= 2) {
    Serial.print(F("PMS RAW:"));
    const uint8_t *raw = parser.frameBytes();
    for (size_t i = 0; i < Pms5003stParser::FRAME_SIZE; ++i) {
      Serial.print(' ');
      if (raw[i] < 16) Serial.print('0');
      Serial.print(raw[i], HEX);
    }
    Serial.println();
  }
#endif
}

void readSensor() {
  while (sensor.available() > 0) {
    const int byte = sensor.read();
    if (byte < 0) break;
    ++receivedBytes;
    PmsReading reading;
    const uint32_t now = millis();
    if (parser.feed(static_cast<uint8_t>(byte), now, reading)) {
      if (!hasFrame || static_cast<uint32_t>(now - lastFrameMs) >= STALE_MS ||
          (latest.error && !reading.error)) {
        sessionStartMs = now;
      }
      latest = reading;
      hasFrame = true;
      lastFrameMs = now;
      logReading();
    }
  }
}

// The animation also calls this service between strips and while waiting.
void serviceIo() {
  readSensor();
#if AIR_ENABLE_BLUETOOTH
  bluetooth.poll(Serial, telemetry(), millis());
#endif
}
}  // namespace

void setup() {
  // Reuse the core's USART2 instance; never construct a second UART on it.
  Serial.setRx(PA3);
  Serial.setTx(PA2);
#if AIR_ENABLE_BLUETOOTH
  Serial.begin(AIR_BLUETOOTH_BAUD, SERIAL_8N1);  // HC-06, PA3 RX / PA2 TX.
#else
  Serial.begin(115200);  // ST-Link debug console, USART2 PA2/PA3.
#endif
  sensor.begin(9600, SERIAL_8N1);
#if !AIR_ENABLE_BLUETOOTH
  Serial.println(F("\r\nAIR PMS5003ST monitor boot"));
  Serial.println(F("PMS TXD -> D2/PA10, VCC=5V, GND=GND; active receive only"));
  Serial.println(F("LCD CS=PB6 DC=PB5 RESET=PA8 SCK=PA5 MOSI=PA7 LED=3V3"));
#endif

  SPI.setSCLK(PA5);
  SPI.setMOSI(PA7);
  SPI.setMISO(PA6);
  lcd.begin(TFT_SPI_HZ);
  lcd.setRotation(1);
  lcd.invertDisplay(false);
  playBootAnimation(lcd, serviceIo);
  serviceIo();
  drawLayout();
  // Show the latest real reading immediately after the intro.
  displayedValues = telemetry().valuesVisible(millis());
  drawValues(displayedValues);
  displayedFrame = parser.frames;
  lastUiMs = millis();
  drawStatus(millis());
#if !AIR_ENABLE_BLUETOOTH
  Serial.println(F("BOOT monitor: ready"));
#endif
}

void loop() {
  serviceIo();
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastUiMs) >= 500) {
    lastUiMs = now;
    const bool visible = telemetry().valuesVisible(now);
    if (parser.frames != displayedFrame || visible != displayedValues) {
      drawValues(visible);
      displayedFrame = parser.frames;
      displayedValues = visible;
    }
    drawStatus(now);
  }
#if !AIR_ENABLE_BLUETOOTH
  if (static_cast<uint32_t>(now - lastLogMs) >= 5000) {
    lastLogMs = now;
    char line[160];
    snprintf(line, sizeof(line),
             "PMS status: rx=%lu valid=%lu checksum_bad=%lu length_bad=%lu partial_timeout=%lu age_ms=%lu",
             static_cast<unsigned long>(receivedBytes),
             static_cast<unsigned long>(parser.frames),
             static_cast<unsigned long>(parser.checksumErrors),
             static_cast<unsigned long>(parser.lengthErrors),
             static_cast<unsigned long>(parser.timeouts),
             static_cast<unsigned long>(hasFrame ? now - lastFrameMs : now));
    Serial.println(line);
  }
#endif
}
