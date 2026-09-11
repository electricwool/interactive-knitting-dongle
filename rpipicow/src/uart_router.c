/*
 * uart_router.c — router between the single physical machine link and clients.
 *
 * Single-threaded: everything runs from the main loop. No locks are needed.
 */

#include "uart_router.h"
#include "board_config.h"

#include <string.h>

typedef struct {
    bool active;
    bool is_owner;
    bool is_listener;
    uart_router_rx_cb on_data;
    void *ctx;
} client_slot_t;

static uart_link_t *s_link = NULL;
static client_slot_t s_clients[UART_ROUTER_MAX_CLIENTS];
static client_id_t s_owner = UART_ROUTER_NO_OWNER;

static void deliver(client_id_t id, uart_dir_t dir,
                    const uint8_t *data, size_t len) {
    if (s_clients[id].active && s_clients[id].on_data) {
        s_clients[id].on_data(id, dir, data, len, s_clients[id].ctx);
    }
}

void uart_router_init(uart_link_t *link) {
    s_link = link;
    s_owner = UART_ROUTER_NO_OWNER;
    memset(s_clients, 0, sizeof(s_clients));
}

client_id_t uart_router_client_register(uart_router_rx_cb on_data, void *ctx) {
    for (uint8_t i = 0; i < UART_ROUTER_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            s_clients[i].active = true;
            s_clients[i].is_owner = false;
            s_clients[i].is_listener = false;
            s_clients[i].on_data = on_data;
            s_clients[i].ctx = ctx;
            return i;
        }
    }
    return UART_ROUTER_NO_OWNER;  // no free slot
}

void uart_router_client_unregister(client_id_t id) {
    if (id >= UART_ROUTER_MAX_CLIENTS) {
        return;
    }
    if (s_owner == id) {
        s_owner = UART_ROUTER_NO_OWNER;
    }
    memset(&s_clients[id], 0, sizeof(s_clients[id]));
}

bool uart_router_take_ownership(client_id_t id, bool force) {
    if (id >= UART_ROUTER_MAX_CLIENTS || !s_clients[id].active) {
        return false;
    }
    if (s_owner == id) {
        return true;
    }
    if (s_owner != UART_ROUTER_NO_OWNER && !force) {
        return false;  // someone else owns; force=false refuses to preempt
    }

    // Preempt the previous owner: it stays registered, it just stops owning.
    if (s_owner != UART_ROUTER_NO_OWNER) {
        s_clients[s_owner].is_owner = false;
    }
    s_owner = id;
    s_clients[id].is_owner = true;
    return true;
}

void uart_router_release_ownership(client_id_t id) {
    if (s_owner == id) {
        s_clients[id].is_owner = false;
        s_owner = UART_ROUTER_NO_OWNER;
    }
}

bool uart_router_owns(client_id_t id) {
    return s_owner == id;
}

client_id_t uart_router_owner(void) {
    return s_owner;
}

void uart_router_set_listener(client_id_t id, bool enable) {
    if (id < UART_ROUTER_MAX_CLIENTS && s_clients[id].active) {
        s_clients[id].is_listener = enable;
    }
}

bool uart_router_is_listener(client_id_t id) {
    return id < UART_ROUTER_MAX_CLIENTS && s_clients[id].is_listener;
}

size_t uart_router_write(client_id_t id, const uint8_t *data, size_t len) {
    if (s_owner != id || s_link == NULL) {
        return 0;  // only the owner may transmit
    }

    size_t written = uart_link_write(s_link, data, len);

    // Mirror outbound bytes to sniffers (not back to the owner).
    for (uint8_t i = 0; i < UART_ROUTER_MAX_CLIENTS; i++) {
        if (i != id && s_clients[i].active && s_clients[i].is_listener) {
            deliver(i, UART_DIR_TX, data, written);
        }
    }
    return written;
}

void uart_router_poll(void) {
    if (s_link == NULL) {
        return;
    }

    static uint8_t buf[256];
    size_t n = uart_link_read(s_link, buf, sizeof(buf));
    if (n == 0) {
        return;
    }

    for (uint8_t i = 0; i < UART_ROUTER_MAX_CLIENTS; i++) {
        if (s_clients[i].active &&
            (s_clients[i].is_owner || s_clients[i].is_listener)) {
            deliver(i, UART_DIR_RX, buf, n);
        }
    }
}

void uart_router_send_break(uint16_t duration_ms) {
    if (s_link != NULL) {
        uart_link_send_break(s_link, duration_ms);
    }
}
