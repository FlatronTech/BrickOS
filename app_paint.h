#ifndef APP_PAINT_H
#define APP_PAINT_H

#include "kernel.h"

#define MAX_CANVAS_W 800
#define MAX_CANVAS_H 600
/* Umieszczamy canvas na 10 MB (0xA00000) i scratch na 12 MB (0xC00000) — gwarantowane w fizycznej pamieci RAM */
#define PAINT_CANVAS ((u32*)0xA00000)
#define PAINT_SCRATCH ((u8*)0xC00000)

extern int paint_dirty;
extern char paint_filename[16];

void paint_init(void);
void paint_clear(void);
void save_paint_bmp(void);
void draw_paint(struct Window* w, int mx, int my, int btn);
void paint_click(struct Window* w, int mx, int my);
void paint_mouse_move(struct Window* w, int mx, int my, int btn);
int  paint_is_hover_link(struct Window* w, int mx, int my);

#endif
