#include "app_welcome.h"

void draw_welcome(struct Window* w) {
    int px = win_client_x(w), py = win_client_y(w);
    gui_rect(w->x + BORDER + 2, win_client_y(w), win_client_w(w), win_client_h(w), 0xFFFFCC);

    gui_print("Welcome to BrickOS!", px, py, 0x000080); py += 20;
    
}
