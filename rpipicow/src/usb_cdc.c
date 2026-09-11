/*
 * usb_cdc.c — single CDC port: machine pass-through + FTDI-style modem signals.
 *
 * The port is registered with the UART router as the CDC data client and owns
 * the machine link by default at startup. While it owns, host bytes are
 * forwarded to the machine and machine bytes come back; the router drops its
 * writes as soon as another client takes ownership.
 *
 * Ownership is explicit and lives in the router — it is NOT tied to DTR.
 *
 * DesignaKnit's modem signals (protocol.md) are emulated with CDC SERIAL_STATE
 * notifications sent on the interrupt endpoint:
 *   - DTR -> DSR loopback: the presence check DesignaKnit does on port open.
 *   - carriage pass: DCD (bRxCarrier) + break, plus a 0x00 byte.
 */

#include "usb_cdc.h"
#include "board_config.h"
#include "uart_router.h"
#include "tusb.h"

#include "device/usbd_pvt.h"

#define CDC_ITF      0
#define CDC_NOTIF_EP 0x81   // must match EPNUM_CDC_NOTIF in usb_descriptors.c

// CDC PSTN120 line-state bits (SERIAL_STATE notification payload).
#define SERIAL_DCD    0x01   // bRxCarrier
#define SERIAL_DSR    0x02   // bTxCarrier
#define SERIAL_BREAK  0x04   // bBreak

typedef struct TU_ATTR_PACKED {
    uint8_t  bmRequestType;   // 0xA1: class, interface, device -> host
    uint8_t  bNotification;   // 0x20: SERIAL_STATE
    uint16_t wValue;          // 0
    uint16_t wIndex;          // interface number
    uint16_t wLength;         // 2
    uint16_t data;            // line-state bitmask
} cdc_serial_state_notif_t;

static bool _dtr = false;
static bool _rts = false;
static volatile bool _line_dirty = false;
static uint16_t _serial_state = 0;

static cdc_line_coding_t _line_coding = {
    .bit_rate  = 9600,
    .stop_bits = 0,     // 1 stop bit
    .parity    = 0,     // none
    .data_bits = 8,
};

static client_id_t _cdc_client = UART_ROUTER_NO_OWNER;

static void cdc_serial_state_send(void);

// Router delivery callback: machine traffic -> host.
static void cdc_on_rx(client_id_t id, uart_dir_t dir,
                      const uint8_t *data, size_t len, void *ctx) {
    (void) id;
    (void) dir;
    (void) ctx;
    if (tud_cdc_n_connected(CDC_ITF)) {
        tud_cdc_n_write(CDC_ITF, data, (uint32_t) len);
    }
}

void usb_cdc_init(void) {
    _cdc_client = uart_router_client_register(cdc_on_rx, NULL);
    if (_cdc_client == UART_ROUTER_NO_OWNER) {
        return;  // no free router slot (should not happen)
    }

    // Default at startup: USB CDC <-> UART1 pass-through (this port owns).
    if (UART_ROUTER_CDC_DEFAULT_OWNER) {
        uart_router_take_ownership(_cdc_client, false);
    }
    if (UART_ROUTER_CDC_DEFAULT_LISTENER) {
        uart_router_set_listener(_cdc_client, true);
    }
}

void usb_cdc_task(void) {
    // Deferred SERIAL_STATE update: DSR follows DTR (presence check). Deferred
    // out of tud_cdc_line_state_cb() so the notification is sent from the main
    // loop, not from inside a control-transfer callback.
    if (_line_dirty) {
        _line_dirty = false;
        uint16_t prev = _serial_state;
        if (_dtr) {
            _serial_state |= SERIAL_DSR;
        } else {
            _serial_state &= (uint16_t) ~SERIAL_DSR;
        }
        if (_serial_state != prev) {
            cdc_serial_state_send();
        }
    }

    // Host -> machine. The router forwards only while we own the link.
    uint8_t buf[64];
    while (tud_cdc_n_available(CDC_ITF)) {
        uint32_t n = tud_cdc_n_read(CDC_ITF, buf, sizeof(buf));
        uart_router_write(_cdc_client, buf, n);
    }

    if (tud_cdc_n_connected(CDC_ITF)) {
        tud_cdc_n_write_flush(CDC_ITF);
    }
}

// Report a carriage pass (or its end) to the host the way the FT230X sensor
// does: DCD + break, plus one 0x00 byte on the pass edge.
void usb_cdc_carriage(bool present) {
    if (present) {
        _serial_state |= (SERIAL_DCD | SERIAL_BREAK);
        cdc_serial_state_send();

        uint8_t zero = 0x00;
        tud_cdc_n_write(CDC_ITF, &zero, 1);
        tud_cdc_n_write_flush(CDC_ITF);
    } else {
        _serial_state &= (uint16_t) ~(SERIAL_DCD | SERIAL_BREAK);
        cdc_serial_state_send();
    }
}

static void cdc_serial_state_send(void) {
    if (!tud_cdc_n_ready(CDC_ITF)) {
        return;                     // interface not configured yet
    }
    if (usbd_edpt_busy(0, CDC_NOTIF_EP)) {
        return;                     // a previous notification is still in flight
    }

    cdc_serial_state_notif_t notif = {
        .bmRequestType = 0xA1,      // CDC class, interface, device -> host
        .bNotification = 0x20,      // SERIAL_STATE
        .wValue        = 0,
        .wIndex        = CDC_ITF,
        .wLength       = 2,
        .data          = _serial_state,
    };

    usbd_edpt_xfer(0, CDC_NOTIF_EP, (uint8_t *) &notif, sizeof(notif));
}

// ---- Accessors --------------------------------------------------------------

bool usb_cdc_connected(void) {
    return tud_cdc_n_connected(CDC_ITF);
}

bool usb_cdc_dtr(void) {
    return _dtr;
}

bool usb_cdc_rts(void) {
    return _rts;
}

uint32_t usb_cdc_baud(void) {
    return _line_coding.bit_rate;
}

uint8_t usb_cdc_data_bits(void) {
    return _line_coding.data_bits;
}

uint8_t usb_cdc_parity(void) {
    return _line_coding.parity;
}

uint8_t usb_cdc_stop_bits(void) {
    return _line_coding.stop_bits;
}

// ---- TinyUSB CDC callbacks --------------------------------------------------

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void) itf;
    _dtr = dtr;
    _rts = rts;
    _line_dirty = true;   // DSR loopback handled from the main loop
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding) {
    (void) itf;
    _line_coding = *p_line_coding;
}

// Host sent a break on the CDC port (SEND_BREAK). Forward it to the machine
// UART as a break condition.
void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms) {
    (void) itf;

    // 0xFFFF means "break until told otherwise"; we don't hold break state, so
    // approximate it with a short fixed break. Cap anything long to avoid a
    // multi-minute busy-wait in the main loop.
    if (duration_ms == 0) {
        return;          // end-break request: nothing to release
    }
    if (duration_ms == 0xFFFF) {
        duration_ms = 10;
    }
    if (duration_ms > 1000) {
        duration_ms = 1000;
    }

    uart_router_send_break(duration_ms);
}
