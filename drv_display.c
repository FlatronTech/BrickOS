#include "drv_display.h"

/* External references to kernel framebuffer globals */
extern u8* backbuf;
extern int fpitch, fbpp;

/* VESA info layout in low memory (set by bootloader) */
#define VESA_BASE   0x700
#define VESA_MODE   (*(u16*)(VESA_BASE + 0x02))
#define VESA_FB     (*(u32*)(VESA_BASE + 0x04))
#define VESA_W      (*(u16*)(VESA_BASE + 0x08))
#define VESA_H      (*(u16*)(VESA_BASE + 0x0A))
#define VESA_PITCH  (*(u16*)(VESA_BASE + 0x0C))
#define VESA_BPP    (*(u8* )(VESA_BASE + 0x0E))
#define VESA_OK     (*(u8* )(VESA_BASE + 0x0F))

void drv_display_init(void) {
    /* Nothing to do; VESA mode is set by the bootloader.
       The kernel already maps backbuf from VESA_FB on startup. */
}

int drv_display_get_width(void) {
    return (int)VESA_W;
}

int drv_display_get_height(void) {
    return (int)VESA_H;
}

int drv_display_get_bpp(void) {
    return (int)VESA_BPP;
}

int drv_display_get_pitch(void) {
    return (int)VESA_PITCH;
}

u32 drv_display_get_fb_base(void) {
    return VESA_FB;
}

u8* drv_display_get_backbuf(void) {
    return backbuf;
}

void drv_display_flip(void) {
    gui_flip();
}
