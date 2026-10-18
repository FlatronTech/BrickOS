#ifndef APP_SAVEAS_H
#define APP_SAVEAS_H

#include "kernel.h"

void open_saveas_dialog(int for_paint);
void saveas_close(void);
void saveas_confirm(void);
void draw_saveas(struct Window* w);
void saveas_click(struct Window* w, int mx, int my);
void saveas_key(char c);
int  saveas_is_hover_link(struct Window* w, int mx, int my);

#endif
