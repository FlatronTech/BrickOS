#ifndef DRV_KEYBOARD_H
#define DRV_KEYBOARD_H

#include "kernel.h"

/* PS/2 keyboard driver interface */
void drv_keyboard_init(void);
int  drv_keyboard_poll(char* out_char);
void drv_keyboard_set_leds(u8 leds);

/* LED bitmask flags */
#define KBD_LED_SCROLL  0x01
#define KBD_LED_NUM     0x02
#define KBD_LED_CAPS    0x04

#endif
