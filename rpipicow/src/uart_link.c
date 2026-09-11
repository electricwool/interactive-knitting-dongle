/*
 * uart_link.c — hardware UART link to the knitting machine.
 *
 * UART1, 9600 8N1, inverted RX and TX (idle LOW). The GPIO input/output
 * overrides do the inversion at the pad, so the UART peripheral itself sees a
 * normal-polarity signal.
 */

#include "uart_link.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/regs/uart.h"
#include "hardware/uart.h"
#include "pico/time.h"

void uart_link_init(uart_link_t *link, const uart_link_config_t *cfg) {
    link->inst = cfg->inst;

    uart_init(cfg->inst, cfg->baud);
    uart_set_format(cfg->inst, 8, 1, UART_PARITY_NONE);  // 8N1
    uart_set_fifo_enabled(cfg->inst, true);

    // UART1 defaults are TX=GPIO4, RX=GPIO5; re-assert them explicitly.
    gpio_set_function(cfg->tx_pin, GPIO_FUNC_UART);
    gpio_set_function(cfg->rx_pin, GPIO_FUNC_UART);

    if (cfg->inverted) {
        gpio_set_outover(cfg->tx_pin, GPIO_OVERRIDE_INVERT);
        gpio_set_inover(cfg->rx_pin, GPIO_OVERRIDE_INVERT);
    } else {
        gpio_set_outover(cfg->tx_pin, GPIO_OVERRIDE_NORMAL);
        gpio_set_inover(cfg->rx_pin, GPIO_OVERRIDE_NORMAL);
    }
}

size_t uart_link_read(uart_link_t *link, uint8_t *buf, size_t max) {
    size_t n = 0;
    while (n < max && uart_is_readable(link->inst)) {
        buf[n++] = (uint8_t) uart_getc(link->inst);
    }
    return n;
}

size_t uart_link_write(uart_link_t *link, const uint8_t *buf, size_t len) {
    size_t n = 0;
    while (n < len && uart_is_writable(link->inst)) {
        uart_putc_raw(link->inst, buf[n++]);
    }
    return n;
}

void uart_link_send_break(uart_link_t *link, uint16_t duration_ms) {
    uart_set_break(link->inst, true);
    busy_wait_ms(duration_ms);
    uart_set_break(link->inst, false);
}

bool uart_link_break_received(uart_link_t *link) {
    uart_hw_t *hw = uart_get_hw(link->inst);
    if (hw->rsr & UART_UARTRSR_BE_BITS) {
        hw->rsr = 0;   // write to UARTECR clears the sticky error bits
        return true;
    }
    return false;
}
