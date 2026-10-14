#pragma once

// Adafruit_ILI9341 1.6.4 unconditionally includes this legacy Arduino header
// on STM32. STM32duino 3.0 removed it; the driver uses no private declarations.
// Keep the compatibility shim in the project, not in downloaded dependencies.
#include <Arduino.h>
