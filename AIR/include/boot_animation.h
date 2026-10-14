#pragma once

class Adafruit_ILI9341;

// Expects an initialized display with DISPON deferred (BootDisplay::begin).
// Draws the first complete frame before enabling display output.
// Calls service() throughout the animation to service sensor and Bluetooth I/O.
void playBootAnimation(Adafruit_ILI9341 &display, void (*service)());
