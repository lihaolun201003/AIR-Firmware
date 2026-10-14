#pragma once

// USART2 has one owner: HC-06 or the ST-Link debug console.
#ifndef AIR_ENABLE_BLUETOOTH
#define AIR_ENABLE_BLUETOOTH 1
#endif

#ifndef AIR_BLUETOOTH_BAUD
#define AIR_BLUETOOTH_BAUD 9600
#endif
