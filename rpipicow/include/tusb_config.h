#pragma once
/*
 * tusb_config.h — TinyUSB configuration for this project.
 *
 * Device-only build with a single CDC (USB serial) function. The RP2040/RP2350
 * board specifics (CFG_TUSB_MCU) and the RTOS glue (CFG_TUSB_OS = OPT_OS_PICO)
 * are supplied by the pico-sdk build, so they are not repeated here.
 */

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board / speed
//--------------------------------------------------------------------+
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT        0
#endif

#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED     OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// CFG_TUSB_MCU and CFG_TUSB_OS are defined by the pico-sdk compiler flags.
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined (pico-sdk should provide it)
#endif

#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | BOARD_TUD_MAX_SPEED)

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG          0
#endif

// Enable the device stack.
#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       BOARD_TUD_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN      __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

//------------- CLASS -------------//
#define CFG_TUD_VENDOR           1   // FTDI FT232R emulation (D2XX)
#define CFG_TUD_CDC              0
#define CFG_TUD_MSC              0
#define CFG_TUD_HID              0
#define CFG_TUD_MIDI             0

// Vendor FIFO sizes: room for one full 1024-byte pattern block.
#define CFG_TUD_VENDOR_EPSIZE        64
#define CFG_TUD_VENDOR_RX_BUFSIZE    1024
#define CFG_TUD_VENDOR_TX_BUFSIZE    1024

#ifdef __cplusplus
}
#endif
