#include "app_taskmgr.h"

void draw_taskmgr(struct Window* w) {
    int px = win_client_x(w), py = win_client_y(w), cw = win_client_w(w);

    gui_rect(px, py, cw, 18, 0x000080);
    gui_print("BrickOS Task Manager", px + 4, py + 1, 0xFFFFFF);
    py += 20;

    gui_rect(px, py, cw, 18, 0xC0C0C0);
    gui_print("Process           PID  CPU  Status", px + 2, py + 2, 0x000000);
    py += 20;

    const char* procs[] = {"System Idle", "Kernel", "GUI Server", "Mouse Drv", "Kbd Driver"};
    for (int i = 0; i < 5; i++) {
        if (i & 1) gui_rect(px, py, cw, 16, 0xEEEEFF);
        gui_print(procs[i], px + 2, py + 1, 0x000000);
        char pb[4];
        itoa(pb, (u32)i);
        gui_print(pb, px + 140, py + 1, 0x000000);
        gui_print(i == 0 ? "99%" : "0%", px + 180, py + 1, 0x000000);
        gui_print("Running", px + 220, py + 1, 0x008000);
        py += 16;
    }

    for (int i = 0; i < nwins; i++) {
        if (!wins[i].visible) continue;
        if ((5 + i) & 1) gui_rect(px, py, cw, 16, 0xEEEEFF);
        gui_print(wins[i].title, px + 2, py + 1, 0x000000);
        char pb[4];
        itoa(pb, (u32)(5 + i));
        gui_print(pb, px + 140, py + 1, 0x000000);
        gui_print("0%", px + 180, py + 1, 0x000000);
        gui_print(wins[i].minimized ? "Minimized" : "Running", px + 220, py + 1, wins[i].minimized ? 0x808080 : 0x008000);
        py += 16;
        if (py > win_client_y(w) + win_client_h(w) - 60) break;
    }

    py += 6;
    gui_rect(px, py, cw, 18, 0x000080);
    gui_print("Performance", px + 4, py + 1, 0xFFFFFF);
    py += 20;

    char upb[16];
    itoa(upb, ticks / 100);
    gui_print("Uptime: ", px, py, 0x000000);
    gui_print(upb, px + 64, py, 0x000000);
    gui_print("s", px + 64 + gui_str_width(upb), py, 0x000000);
    py += 16;

    char tb[16];
    itoa(tb, (u32)nwins);
    gui_print("Windows:", px, py, 0x000000);
    gui_print(tb, px + 72, py, 0x000000);
}
