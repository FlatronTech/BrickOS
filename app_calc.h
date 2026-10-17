#ifndef APP_CALC_H
#define APP_CALC_H

#include "kernel.h"

extern int calc_active;

void calc_init(void);
void draw_calc(struct Window* w);
void calc_click(struct Window* w, int mx, int my);
void calc_key(char c);
int  calc_is_hover_link(struct Window* w, int mx, int my);

#endif
