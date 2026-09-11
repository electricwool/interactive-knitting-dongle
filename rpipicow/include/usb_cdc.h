#pragma once
/*
 * usb_cdc.h — USB CDC (virtual COM port) interface.
 *
 * One CDC port: the machine pass-through. It is registered with the UART router
 * as the CDC data client and, by default, owns the machine link at startup.
 *
 * Ownership is explicit and lives in the router — it is NOT tied to DTR. When a
 * future client (web API, SPP-C) takes ownership, the router stops routing to
 * this client, so the CDC port is effectively disabled until it owns again.
 *
 * Modem signals are emulated with CDC SERIAL_STATE notifications on the
 * interrupt endpoint, matching what DesignaKnit expects (protocol.md):
 *   - DTR -> DSR loopback (presence check when the port is opened).
 *   - carriage pass -> DCD + break, plus a 0x00 byte.
 */

#include <stdbool.h>
#include <stdint.h>

// Call once at startup, after tusb_init() and uart_router_init().
void usb_cdc_init(void);

// Call repeatedly from the main loop.
void usb_cdc_task(void);

// Signal a carriage pass (present=true) or its end (present=false) to the host.
void usb_cdc_carriage(bool present);

bool usb_cdc_connected(void);

// Host control-line state (SET_CONTROL_LINE_STATE).
bool usb_cdc_dtr(void);
bool usb_cdc_rts(void);

// Host line coding (SET_LINE_CODING).
uint32_t usb_cdc_baud(void);
uint8_t  usb_cdc_data_bits(void);
uint8_t  usb_cdc_parity(void);
uint8_t  usb_cdc_stop_bits(void);
