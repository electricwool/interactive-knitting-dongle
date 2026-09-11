#pragma once
/*
 * ftdi.h — FTDI FT232R emulation (VID 0403:6001).
 *
 * Presents a vendor-specific FTDI interface that the D2XX driver (used by
 * DesignaKnit and img2track) can open directly with FT_Open(). The machine UART
 * is relayed through it, with the FTDI 2-byte status header prepended to every
 * device -> host bulk-IN transfer.
 */

#include <stdbool.h>
#include <stdint.h>

// Call once at startup, after tusb_init() and uart_router_init().
void ftdi_init(void);

// Call repeatedly from the main loop.
void ftdi_task(void);

// Inject one carriage-pass byte into the machine -> PC stream (BrotherLink
// "BL5" style: any ASCII byte counts as a pass).
void ftdi_carriage_pass(void);
