#ifndef DRV_MOUSE_H
#define DRV_MOUSE_H

#include "kernel.h"

/* PS/2 mouse driver interface */
void drv_mouse_init(void);
int  drv_mouse_poll(int* dx, int* dy, int* buttons);
int  drv_mouse_get_x(void);
int  drv_mouse_get_y(void);
int  drv_mouse_get_buttons(void);

/* Button bitmask flags */
#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

#endif
