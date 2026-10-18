#ifndef APP_FILEPROPS_H
#define APP_FILEPROPS_H

#include "kernel.h"

void open_file_props(const char* nm, u32 sz, const char* ty, int clust);
void draw_fileprops(struct Window* w);
int  fileprops_click(struct Window* w, int mx, int my);
int  fileprops_is_hover_link(struct Window* w, int mx, int my);

#endif
