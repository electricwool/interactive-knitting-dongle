#pragma once
/*
 * uart_router.h — router between the single physical machine link and many
 * clients (USB CDC, a future web API, passive sniffers, ...).
 *
 * Ownership model:
 *   - At most ONE client owns the link at a time (exclusive bidirectional).
 *   - Ownership is EXPLICIT (take/release). It is deliberately NOT tied to DTR
 *     or any other USB control-line state, so non-serial clients (web) can own
 *     too and serial clients are free to use DTR for its normal purpose.
 *   - A client may also be a LISTENER (sniffer): it receives copies of both
 *     directions without owning the link.
 *
 * Data flow:
 *   - machine -> router : delivered to the owner (dir=RX) and every listener (dir=RX)
 *   - owner   -> machine: only the owner may transmit; listeners receive a copy
 *     (dir=TX). The owner does not get its own TX echoed back.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "uart_link.h"

typedef uint8_t client_id_t;

#define UART_ROUTER_NO_OWNER 0xFF   // sentinel: nobody owns / no free slot

// Reserved client ids (fixed well-known ids).
#define CLIENT_ID_CDC_DATA   0
#define CLIENT_ID_WEB        1   // reserved for the future web API
#define CLIENT_ID_SPP        2   // reserved for the future SPP-C (Bluetooth) client

typedef enum {
    UART_DIR_RX = 0,   // bytes received FROM the machine
    UART_DIR_TX = 1    // bytes sent TO the machine
} uart_dir_t;

// Delivery callback, invoked on the main loop for each client that should see
// some traffic. `data` is only valid for the duration of the call.
typedef void (*uart_router_rx_cb)(client_id_t id, uart_dir_t dir,
                                  const uint8_t *data, size_t len, void *ctx);

void uart_router_init(uart_link_t *link);

client_id_t uart_router_client_register(uart_router_rx_cb on_data, void *ctx);
void uart_router_client_unregister(client_id_t id);

// Explicit ownership. `force` preempts the current owner (it stays registered,
// it just stops owning). Returns true on success.
bool uart_router_take_ownership(client_id_t id, bool force);
void uart_router_release_ownership(client_id_t id);
bool uart_router_owns(client_id_t id);
client_id_t uart_router_owner(void);

// Sniffer / listener control.
void uart_router_set_listener(client_id_t id, bool enable);
bool uart_router_is_listener(client_id_t id);

// Send bytes toward the machine. Only forwarded if `id` is the current owner.
// Returns the number of bytes actually forwarded (0 if not the owner).
size_t uart_router_write(client_id_t id, const uint8_t *data, size_t len);

// Drain the machine link and fan out to owner + listeners. Call from main loop.
void uart_router_poll(void);

// Send a break toward the machine.
void uart_router_send_break(uint16_t duration_ms);
