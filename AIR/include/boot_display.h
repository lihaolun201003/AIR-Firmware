#pragma once

#include <Adafruit_ILI9341.h>

// Same rendering API; begin() initializes the panel but defers display-on.
class BootDisplay : public Adafruit_ILI9341 {
 public:
  BootDisplay(SPIClass *spi, int8_t dc, int8_t cs, uint8_t resetPin)
      : Adafruit_ILI9341(spi, dc, cs, -1), resetPin_(resetPin) {}
  void begin(uint32_t frequency) override;

 private:
  uint8_t resetPin_;
};
