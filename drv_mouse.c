#include "drv_mouse.h"

/* PS/2 controller ports */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

/* Absolute cursor position (updated on each poll) */
static int mouse_abs_x = 0;
static int mouse_abs_y = 0;
static int mouse_buttons = 0;

/* 3-byte packet accumulator */
static u8  mouse_pkt[3];
static int mouse_pkt_idx = 0;

static void ps2_wait_write(void) {
    int timeout = 100000;
    while ((inb(PS2_STATUS) & 0x02) && timeout-- > 0) {}
}

static void ps2_wait_read(void) {
    int timeout = 100000;
    while (!(inb(PS2_STATUS) & 0x01) && timeout-- > 0) {}
}

static void ps2_send_cmd(u8 cmd) {
    ps2_wait_write();
    outb(PS2_CMD, cmd);
}

static void ps2_send_data(u8 data) {
    ps2_wait_write();
    outb(PS2_DATA, data);
}

static u8 ps2_read_data(void) {
    ps2_wait_read();
    return inb(PS2_DATA);
}

void drv_mouse_init(void) {
    mouse_abs_x   = fw / 2;
    mouse_abs_y   = fh / 2;
    mouse_buttons = 0;
    mouse_pkt_idx = 0;

    /* Enable auxiliary (mouse) device */
    ps2_send_cmd(0xA8);

    /* Enable mouse interrupt (IRQ12) via command byte */
    ps2_send_cmd(0x20);           /* read command byte */
    u8 cb = ps2_read_data();
    cb |= 0x02;                   /* enable aux interrupt */
    cb &= ~0x20;                  /* clear aux clock disable */
    ps2_send_cmd(0x60);           /* write command byte */
    ps2_send_data(cb);

    /* Send "Set Defaults" to mouse */
    ps2_send_cmd(0xD4);
    ps2_send_data(0xF6);
    ps2_read_data(); /* ACK */

    /* Enable mouse data reporting */
    ps2_send_cmd(0xD4);
    ps2_send_data(0xF4);
    ps2_read_data(); /* ACK */
}

/* Poll for a complete 3-byte packet.
   Returns 1 on new packet, 0 if partial/nothing, -1 on overflow. */
int drv_mouse_poll(int* dx, int* dy, int* buttons) {
    if (!(inb(PS2_STATUS) & 0x21)) return 0; /* not from aux device */

    u8 b = inb(PS2_DATA);

    if (mouse_pkt_idx == 0 && !(b & 0x08)) {
        /* Bit 3 must be set in byte 0; desync guard */
        return -1;
    }

    mouse_pkt[mouse_pkt_idx++] = b;

    if (mouse_pkt_idx < 3) return 0; /* need more bytes */
    mouse_pkt_idx = 0;

    /* Decode packet */
    u8  flags = mouse_pkt[0];
    int rel_x = (int)mouse_pkt[1] - ((flags & 0x10) ? 256 : 0);
    int rel_y = (int)mouse_pkt[2] - ((flags & 0x20) ? 256 : 0);
    rel_y = -rel_y; /* PS/2 Y is inverted vs screen */

    mouse_abs_x += rel_x;
    mouse_abs_y += rel_y;

    /* Clamp to screen */
    if (mouse_abs_x < 0) mouse_abs_x = 0;
    if (mouse_abs_x >= fw) mouse_abs_x = fw - 1;
    if (mouse_abs_y < 0) mouse_abs_y = 0;
    if (mouse_abs_y >= fh) mouse_abs_y = fh - 1;

    mouse_buttons = flags & 0x07;

    if (dx)      *dx      = rel_x;
    if (dy)      *dy      = rel_y;
    if (buttons) *buttons = mouse_buttons;
    return 1;
}

int drv_mouse_get_x(void) { return mouse_abs_x; }
int drv_mouse_get_y(void) { return mouse_abs_y; }
int drv_mouse_get_buttons(void) { return mouse_buttons; }
