/*
 * usb_descriptors.c — FTDI FT232R device descriptors (VID 0403:6001).
 *
 * Vendor-specific interface with two bulk endpoints, exactly as the D2XX driver
 * expects for an FT232R:
 *   EP 0x02 OUT : host -> device (TX data)
 *   EP 0x81 IN  : device -> host (RX data + 2-byte FTDI status header)
 *
 * DesignaKnit and img2track open this with FT_Open(), so they need the FTDI
 * VID/PID and the 0xFF/0xFF/0xFF vendor interface.
 */

#include <string.h>

#include "tusb.h"

#define USB_VID 0x0403
#define USB_PID 0x6001   // FT232R

enum {
    ITF_NUM_VENDOR = 0,
    ITF_NUM_TOTAL
};

#define EPNUM_TX_OUT 0x02   // host -> device
#define EPNUM_RX_IN  0x81   // device -> host

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL
};

//--------------------------------------------------------------------
// Device descriptor
//--------------------------------------------------------------------
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0600,   // identifies an FT232R
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1
};

//--------------------------------------------------------------------
// Configuration descriptor (32 bytes, hand-written to match FTDI exactly:
// interface class 0xFF, subclass 0xFF, protocol 0xFF)
//--------------------------------------------------------------------
uint8_t const desc_configuration[] = {
    // Config descriptor
    9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(32), 1, 1, 0, 0x80, 50,   // 100 mA

    // Vendor-specific interface, 2 endpoints
    9, TUSB_DESC_INTERFACE, ITF_NUM_VENDOR, 0, 2,
    0xFF, 0xFF, 0xFF, STRID_PRODUCT,

    // Endpoint 2 OUT (host -> device), bulk, 64 bytes
    7, TUSB_DESC_ENDPOINT, EPNUM_TX_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,

    // Endpoint 1 IN (device -> host), bulk, 64 bytes
    7, TUSB_DESC_ENDPOINT, EPNUM_RX_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

//--------------------------------------------------------------------
// String descriptors
//--------------------------------------------------------------------
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: supported language is English (0x0409)
    "FTDI",                         // 1: Manufacturer
    "FT232R USB UART",              // 2: Product
    "A50285BI",                     // 3: Serial number
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        const char* str = string_desc_arr[index];
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // First element is length (including header); second byte is the string type.
    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}
