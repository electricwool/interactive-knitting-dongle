#pragma once
/*
 * board_config.h — single source of truth for pin assignments and link settings.
 *
 * Edit this file to move pins around. Nothing else in the firmware hard-codes
 * pin numbers.
 */

#include "hardware/uart.h"

// ---- Level shifter enable --------------------------------------------------
// GP6 is held HIGH from the moment firmware starts so the 3.3V <-> 5V
// voltage-level converter is powered before any UART traffic exists.
#define PIN_LEVEL_SHIFT_EN      6

// ---- Knitting-machine UART (hardware UART1, inverted 9600 8N1) -------------
// Standard UART1 mapping: GP4 = TX -> machine RX, GP5 = RX <- machine TX.
// (The machine-side wires are swapped so its TX lands on GP5.)
#define PIN_UART_TX             4
#define PIN_UART_RX             5
#define BOARD_UART_INST         uart1
#define BOARD_UART_BAUD         9600
// Brother KH-9xx UART is inverted (idle LOW); applied via GPIO output/input
// overrides inside uart_link.c. The machine also REQUIRES its RTS pin (the
// "green wire" of the official cable, machine connector pin 3) to be held at
// 5 V, or its serial port stays silent (download error 1 / RX reads 0x00).
#define BOARD_UART_INVERTED     1

// ---- Carriage-pass reed sensor ---------------------------------------------
// Reed switch wired between GP27 and GND. The internal pull-up keeps the line
// HIGH until a magnet closes the reed and pulls the line LOW.
#define PIN_REED_SENSOR         27
#define REED_ADC_CHANNEL        1       // GPIO27 == ADC input 1
#define REED_THRESHOLD_MV       3000    // < 3.0 V  => magnet present
#define REED_DEBOUNCE_MS        10      // LOW must hold this long to count a pass
#define REED_REARM_MS           100     // HIGH must hold this long before re-arming

// ---- UART router -----------------------------------------------------------
#define UART_ROUTER_MAX_CLIENTS 8

// FTDI (machine link) client defaults.
#define UART_ROUTER_FTDI_DEFAULT_OWNER    1   // start owning the machine link
#define UART_ROUTER_FTDI_DEFAULT_LISTENER 0   // don't mirror traffic by default
