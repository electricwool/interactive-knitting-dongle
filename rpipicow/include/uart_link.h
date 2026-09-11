#pragma once
/*
 * uart_link.h — physical link to the knitting machine.
 *
 * Hardware UART1 on GP4(TX)/GP5(RX), inverted (idle LOW), 9600 8N1. The wire
 * polarity and baud/format are contained here; the router above knows nothing
 * about them.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "hardware/uart.h"

typedef struct {
    uart_inst_t *inst;
    uint tx_pin;
    uint rx_pin;
    uint baud;
    bool inverted;
} uart_link_config_t;

typedef struct {
    uart_inst_t *inst;
} uart_link_t;

// Configure the UART, its pins and the RX/TX inversion. Call once at boot.
void uart_link_init(uart_link_t *link, const uart_link_config_t *cfg);

// Non-blocking: read up to `max` bytes received from the machine.
size_t uart_link_read(uart_link_t *link, uint8_t *buf, size_t max);

// Write bytes to the machine WITHOUT blocking. Returns the number of bytes
// actually queued (may be less than `len` if the TX FIFO is full).
size_t uart_link_write(uart_link_t *link, const uint8_t *buf, size_t len);

// Send a break (spacing) condition for `duration_ms`. Because the machine UART
// is inverted, the GPIO override turns the UART break into a HIGH line — which
// is exactly the spacing state the machine expects.
void uart_link_send_break(uart_link_t *link, uint16_t duration_ms);

// Detect a break received FROM the machine. Returns true (and clears the flag)
// if a break arrived since the last call. Poll this from the main loop.
bool uart_link_break_received(uart_link_t *link);
