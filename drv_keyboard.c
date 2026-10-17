#include "drv_keyboard.h"

/* PS/2 keyboard I/O ports */
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64
#define KBD_CMD_PORT    0x64

/* US QWERTY scancode set 1 — unshifted */
static const char kbd_map[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0,   ' ', 0
};

/* US QWERTY scancode set 1 — shifted */
static const char kbd_map_shift[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0,   ' ', 0
};

static int kbd_shift = 0;
static int kbd_caps  = 0;

void drv_keyboard_init(void) {
    kbd_shift = 0;
    kbd_caps  = 0;
    /* Flush any stale scan codes */
    while (inb(KBD_STATUS_PORT) & 0x01) {
        inb(KBD_DATA_PORT);
    }
}

/* Returns 1 and stores ASCII in *out_char if a key-press is available,
   0 if nothing is ready, -1 on key-release only events. */
int drv_keyboard_poll(char* out_char) {
    if (!(inb(KBD_STATUS_PORT) & 0x01)) return 0; /* nothing ready */

    u8 sc = inb(KBD_DATA_PORT);

    /* Track shift keys (left: 0x2A, right: 0x36) */
    if (sc == 0x2A || sc == 0x36) { kbd_shift = 1; return -1; }
    if (sc == 0xAA || sc == 0xB6) { kbd_shift = 0; return -1; }

    /* Track caps lock (0x3A) */
    if (sc == 0x3A) { kbd_caps ^= 1; drv_keyboard_set_leds(kbd_caps ? KBD_LED_CAPS : 0); return -1; }

    /* Key release (bit 7 set) */
    if (sc & 0x80) return -1;

    if (sc >= 128) return -1;

    char c;
    if (kbd_shift) {
        c = kbd_map_shift[sc];
    } else {
        c = kbd_map[sc];
        if (kbd_caps && c >= 'a' && c <= 'z') c -= 32;
    }

    if (!c) return -1;

    *out_char = c;
    return 1;
}

void drv_keyboard_set_leds(u8 leds) {
    /* Wait for input buffer empty */
    while (inb(KBD_STATUS_PORT) & 0x02) {}
    outb(KBD_DATA_PORT, 0xED); /* set LEDs command */
    while (inb(KBD_STATUS_PORT) & 0x02) {}
    outb(KBD_DATA_PORT, leds);
}
