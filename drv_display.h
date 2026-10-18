#ifndef DRV_DISPLAY_H
#define DRV_DISPLAY_H

#include "kernel.h"

/* Hardware driver types */
#define DRV_DISP_HW_VESA   0
#define DRV_DISP_HW_VMWARE 1
#define DRV_DISP_HW_BGA    2

/* Display driver interface */
void drv_display_init(void);
int  drv_display_get_width(void);
int  drv_display_get_height(void);
int  drv_display_get_bpp(void);
int  drv_display_get_pitch(void);
u32  drv_display_get_fb_base(void);
u8*  drv_display_get_backbuf(void);
void drv_display_flip(void);

/* Hardware detection and live mode setting */
int         drv_display_get_hw_type(void);
const char* drv_display_get_hw_name(void);
int         drv_display_set_resolution(int width, int height);

#endif
