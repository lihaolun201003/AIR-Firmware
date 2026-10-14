#pragma once

// Default USART2 wiring needs solder bridges; UART6 uses direct PA11/PA12 pins.
#ifndef AIR_ENABLE_BLUETOOTH
#define AIR_ENABLE_BLUETOOTH 1
#endif

#ifndef AIR_BLUETOOTH_UART6
#define AIR_BLUETOOTH_UART6 0
#endif

// UART6 leaves USART2 available for ST-Link VCP diagnostics.
#define AIR_ENABLE_SERIAL_LOG (!AIR_ENABLE_BLUETOOTH || AIR_BLUETOOTH_UART6)

#ifndef AIR_BLUETOOTH_BAUD
#define AIR_BLUETOOTH_BAUD 9600
#endif

// Blinker number widgets use JSON lines. Set to 0 for the original ENV app.
#ifndef AIR_BLUETOOTH_BLINKER
#define AIR_BLUETOOTH_BLINKER 1
#endif
