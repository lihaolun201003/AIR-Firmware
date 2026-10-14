#pragma once

class Adafruit_ILI9341;

// Calls service() throughout the animation to keep the sensor UART drained.
void playBootAnimation(Adafruit_ILI9341 &display, void (*service)());
