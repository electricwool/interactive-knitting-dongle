/*
 * ftdi.c — FTDI FT232R emulation.
 *
 * USB side: vendor-specific interface, EP 0x02 OUT (host -> machine) and
 * EP 0x81 IN (machine -> host). Every bulk-IN transfer carries the FTDI 2-byte
 * status header (modem status then line status) before any data — this is what
 * the D2XX driver strips from FT_Read().
 *
 * Control: the FTDI SIO vendor requests are answered here (reset, modem ctrl,
 * flow ctrl, baud, data format, modem status, event/error char, latency timer,
 * EEPROM read).
 */

#include "ftdi.h"
#include "board_config.h"
#include "uart_router.h"
#include "tusb.h"

#include "device/usbd.h"

#include <string.h>

// FTDI SIO requests (class-type vendor requests on EP0).
#define SIO_RESET            0x00
#define SIO_MODEM_CTRL       0x01
#define SIO_SET_FLOW_CTRL    0x02
#define SIO_SET_BAUDRATE     0x03
#define SIO_SET_DATA         0x04
#define SIO_GET_MODEM_STATUS 0x05
#define SIO_SET_EVENT_CHAR   0x06
#define SIO_SET_ERROR_CHAR   0x07
#define SIO_SET_LATENCY      0x09
#define SIO_GET_LATENCY      0x0A
#define SIO_SET_BITMODE      0x0B
#define SIO_READ_PINS        0x0C
#define SIO_READ_EEPROM      0x90
#define SIO_WRITE_EEPROM     0x91
#define SIO_ERASE_EEPROM     0x92

// FTDI status-header bytes (protocol-brother-kh930.md / protocol.md).
#define FTDI_MSR_IDLE 0x01   // modem status: reserved bit only (no DCD/DSR)
#define FTDI_LSR_IDLE 0x60   // line status: THRE | TEMT (no break)

// FT232R EEPROM (128 bytes = 64 x 16-bit words, little-endian).
// Best-effort image; if img2track/DesignaKnit enumerate by description or serial
// and need the exact strings, replace with a dump of the official cable
// (FT_PROG -> read 64 words) and re-run the checksum at word 63.
static const uint16_t s_eeprom[64] = {
    [0]  = 0x0000,   // device type (FT232R)
    [1]  = 0x0403,   // vendor ID
    [2]  = 0x6001,   // product ID
    [3]  = 0x0600,   // release number (identifies FT232R)
    [4]  = 0x0080,   // bus powered, no remote wakeup
    [5]  = 0x0090,   // max power
    // [6..62] = 0x0000 (empty strings, no config flags)
    [63] = 0xC8B8,   // checksum: 0xAAAA ^ XOR(words 0..62)
};

static bool _dtr = false;
static bool _rts = false;
static uint8_t _latency_ms = 16;
static client_id_t _ftdi_client = UART_ROUTER_NO_OWNER;
static uint8_t _carriage_byte = '0';

// Machine -> host: prepend the FTDI status header and push to bulk IN.
static void ftdi_tx(const uint8_t *data, size_t len) {
    if (!tud_vendor_n_mounted(0)) {
        return;
    }
    if (len > 256) {
        len = 256;
    }

    uint8_t buf[2 + 256];
    buf[0] = FTDI_MSR_IDLE;
    buf[1] = FTDI_LSR_IDLE;
    memcpy(&buf[2], data, len);

    tud_vendor_n_write(0, buf, (uint32_t) (len + 2));
    tud_vendor_n_write_flush(0);
}

// Router delivery callback: machine RX -> host.
static void ftdi_on_rx(client_id_t id, uart_dir_t dir,
                       const uint8_t *data, size_t len, void *ctx) {
    (void) id;
    (void) dir;
    (void) ctx;
    ftdi_tx(data, len);
}

void ftdi_init(void) {
    _ftdi_client = uart_router_client_register(ftdi_on_rx, NULL);
    if (_ftdi_client == UART_ROUTER_NO_OWNER) {
        return;  // no free router slot (should not happen)
    }

    // Default at startup: FTDI <-> UART1 pass-through (this client owns).
    if (UART_ROUTER_FTDI_DEFAULT_OWNER) {
        uart_router_take_ownership(_ftdi_client, false);
    }
    if (UART_ROUTER_FTDI_DEFAULT_LISTENER) {
        uart_router_set_listener(_ftdi_client, true);
    }
}

void ftdi_task(void) {
    // Host -> machine (EP 0x02 OUT). Raw bytes, no header. Drain one byte at a
    // time, only while the UART TX FIFO can accept it, so the main loop keeps
    // servicing UART RX (full duplex: no dropped bytes on either side).
    uint8_t b;
    while (uart_is_writable(BOARD_UART_INST) && tud_vendor_n_available(0)) {
        if (tud_vendor_n_read(0, &b, 1) == 1) {
            uart_router_write(_ftdi_client, &b, 1);
        }
    }
}

void ftdi_carriage_pass(void) {
    // BrotherLink BL5: inject one ASCII byte into the machine -> PC stream.
    uint8_t b = _carriage_byte;
    if (++_carriage_byte > '9') {
        _carriage_byte = '0';
    }
    ftdi_tx(&b, 1);
}

// ---- FTDI SIO vendor control requests --------------------------------------

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request) {
    switch (request->bRequest) {
        case SIO_RESET:
            if (stage == CONTROL_STAGE_SETUP) {
                tud_control_status(rhport, request);
            }
            return true;

        case SIO_MODEM_CTRL:
            if (stage == CONTROL_STAGE_SETUP) {
                tud_control_status(rhport, request);
            } else if (stage == CONTROL_STAGE_ACK) {
                // wValue low byte = signal (0x01 DTR, 0x02 RTS); high byte = value.
                bool value = (request->wValue >> 8) & 1;
                if ((request->wValue & 0xFF) == 0x01) {
                    _dtr = value;
                } else if ((request->wValue & 0xFF) == 0x02) {
                    _rts = value;
                }
            }
            return true;

        case SIO_SET_FLOW_CTRL:
        case SIO_SET_BAUDRATE:
        case SIO_SET_DATA:
        case SIO_SET_EVENT_CHAR:
        case SIO_SET_ERROR_CHAR:
            if (stage == CONTROL_STAGE_SETUP) {
                tud_control_status(rhport, request);
            }
            return true;

        case SIO_GET_MODEM_STATUS:
            if (stage == CONTROL_STAGE_SETUP) {
                uint8_t status[2] = { FTDI_MSR_IDLE, FTDI_LSR_IDLE };
                tud_control_xfer(rhport, request, status, 2);
            }
            return true;

        case SIO_SET_LATENCY:
            if (stage == CONTROL_STAGE_SETUP) {
                tud_control_status(rhport, request);
            } else if (stage == CONTROL_STAGE_ACK) {
                _latency_ms = (uint8_t) request->wValue;
            }
            return true;

        case SIO_GET_LATENCY:
            if (stage == CONTROL_STAGE_SETUP) {
                tud_control_xfer(rhport, request, &_latency_ms, 1);
            }
            return true;

        case SIO_SET_BITMODE:
        case SIO_WRITE_EEPROM:
        case SIO_ERASE_EEPROM:
            if (stage == CONTROL_STAGE_SETUP) {
                tud_control_status(rhport, request);
            }
            return true;

        case SIO_READ_PINS:
            if (stage == CONTROL_STAGE_SETUP) {
                uint8_t pins = 0x00;
                tud_control_xfer(rhport, request, &pins, 1);
            }
            return true;

        case SIO_READ_EEPROM:
            if (stage == CONTROL_STAGE_SETUP) {
                uint16_t word = (request->wIndex < 64) ? s_eeprom[request->wIndex] : 0x0000;
                tud_control_xfer(rhport, request, &word, 2);
            }
            return true;

        default:
            return false;   // stall unsupported requests
    }
}
