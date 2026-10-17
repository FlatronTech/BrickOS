#ifndef DRV_DISPLAY_H
#define DRV_DISPLAY_H

#include "kernel.h"

/* VESA/display driver interface */
void drv_display_init(void);
int  drv_display_get_width(void);
int  drv_display_get_height(void);
int  drv_display_get_bpp(void);
int  drv_display_get_pitch(void);
u32  drv_display_get_fb_base(void);
u8*  drv_display_get_backbuf(void);
void drv_display_flip(void);

#endif
