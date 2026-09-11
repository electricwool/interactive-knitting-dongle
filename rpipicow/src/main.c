/*
 * main.c — rpipicow firmware.
 *
 * Boot order:
 *   1. level shifter enable (GP6 high),
 *   2. carriage-pass reed sensor (GP27, pull-up, debounced),
 *   3. machine UART link + router,
 *   4. FTDI FT232R emulation (owns the machine link at startup).
 *
 * The main loop drives the USB stack, drains the machine link into the router,
 * pumps the FTDI host->machine path, and turns reed events into BL5
 * carriage-pass bytes for DesignaKnit.
 */

#include "pico/stdlib.h"
#include "tusb.h"

#include "board_config.h"
#include "ftdi.h"
#include "reed_sensor.h"
#include "uart_link.h"
#include "uart_router.h"

int main(void) {
    // 1) Level shifter power first: hold GP6 high so the 3.3V <-> 5V converter
    //    is stable before any other I/O happens.
    gpio_init(PIN_LEVEL_SHIFT_EN);
    gpio_set_dir(PIN_LEVEL_SHIFT_EN, GPIO_OUT);
    gpio_put(PIN_LEVEL_SHIFT_EN, 1);

    // 2) Carriage-pass reed sensor (internal pull-up, debounced).
    reed_sensor_init();

    // 3) Machine UART link + router.
    static uart_link_t machine_link;
    uart_link_config_t link_cfg = {
        .inst     = BOARD_UART_INST,
        .tx_pin   = PIN_UART_TX,
        .rx_pin   = PIN_UART_RX,
        .baud     = BOARD_UART_BAUD,
        .inverted = BOARD_UART_INVERTED,
    };
    uart_link_init(&machine_link, &link_cfg);
    uart_router_init(&machine_link);

    // 4) FTDI FT232R emulation.
    tusb_init();
    ftdi_init();

    while (true) {
        tud_task();          // USB device stack (control + bulk transfers)
        uart_router_poll();  // machine RX -> owner + sniffers
        ftdi_task();         // host -> machine

        if (reed_sensor_task() == REED_EVT_PASS) {
            ftdi_carriage_pass();
        }
    }

    return 0;
}
