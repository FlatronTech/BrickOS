#ifndef APP_EXPLORER_H
#define APP_EXPLORER_H

#include "kernel.h"

void draw_explorer(struct Window* w);
void explorer_click(struct Window* w, int mx, int my);
void explorer_key(char c, struct Window* w);
int  explorer_is_hover_link(struct Window* w, int mx, int my);

#endif
