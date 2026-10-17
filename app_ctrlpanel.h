#ifndef APP_CTRLPANEL_H
#define APP_CTRLPANEL_H

#include "kernel.h"

extern int ctrlpanel_tab;
extern u32 desktop_color;
extern int pending_res;

void ctrlpanel_init(void);
void draw_ctrlpanel(struct Window* w);
void ctrlpanel_click(struct Window* w, int mx, int my);
int  ctrlpanel_is_hover_link(struct Window* w, int mx, int my);

#endif
