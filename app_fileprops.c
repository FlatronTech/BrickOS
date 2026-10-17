#include "app_fileprops.h"

static char fileprops_name[16] = "";
static u32  fileprops_size = 0;
static char fileprops_type[20] = "";
static int  fileprops_clusters = 0;

void open_file_props(const char* nm, u32 sz, const char* ty, int clust) {
    int i = 0;
    while (nm[i] && i < 15) {
        fileprops_name[i] = nm[i];
        i++;
    }
    fileprops_name[i] = 0;
    fileprops_size = sz;

    i = 0;
    while (ty[i] && i < 19) {
        fileprops_type[i] = ty[i];
        i++;
    }
    fileprops_type[i] = 0;
    fileprops_clusters = clust;

    char title[32] = "Properties: ";
    my_strcat(title, nm);

    int ex = win_find(APP_FILEPROPS);
    if (ex >= 0) {
        win_raise(ex);
        my_strcpy(wins[nwins - 1].title, title);
        wins[nwins - 1].minimized = 0;
        wins[nwins - 1].visible = 1;
        return;
    }
    win_new(APP_FILEPROPS, 200, 150, 300, 220, title);
}

static int fileprops_ok_y(struct Window* w) {
    return win_client_y(w) + 54 + 18 * 5 + 12;
}

void draw_fileprops(struct Window* w) {
    int px = win_client_x(w), py = win_client_y(w), cw = win_client_w(w);
    int ix = px + 10, iy = py + 10;

    /* File icon */
    gui_rect(ix, iy, 32, 32, 0xFFFFFF);
    gui_hline(ix, iy, 32, 0x808080); gui_vline(ix, iy, 32, 0x808080);
    gui_hline(ix, iy + 31, 32, 0x808080); gui_vline(ix + 31, iy, 32, 0x808080);
    gui_rect(ix + 22, iy, 10, 10, 0xC0C0C0);
    gui_pixel(ix + 22, iy, 0x808080); gui_pixel(ix + 31, iy + 9, 0x808080);
    gui_hline(ix + 4, iy + 14, 20, 0x0000AA);
    gui_hline(ix + 4, iy + 18, 20, 0x0000AA);
    gui_hline(ix + 4, iy + 22, 15, 0x0000AA);

    gui_print(fileprops_name, px + 52, py + 14, 0x000000);
    gui_hline(px, py + 46, cw, 0x808080);

    int ly = py + 54;
    int lx2 = px + 100;

    gui_print("Name:", px + 8, ly, 0x000000);
    gui_print(fileprops_name, lx2, ly, 0x000080); ly += 18;

    char szb[20];
    format_size(szb, fileprops_size);
    gui_print("Size:", px + 8, ly, 0x000000);
    gui_print(szb, lx2, ly, 0x000080); ly += 18;

    char szb2[16];
    itoa(szb2, fileprops_size);
    my_strcat(szb2, " bytes");
    gui_print("", px + 8, ly, 0x000000);
    gui_print(szb2, lx2, ly, 0x606060); ly += 18;

    gui_print("Type:", px + 8, ly, 0x000000);
    gui_print(fileprops_type, lx2, ly, 0x000080); ly += 18;

    char cb[8];
    itoa(cb, (u32)fileprops_clusters);
    my_strcat(cb, " cluster(s)");
    gui_print("Alloc:", px + 8, ly, 0x000000);
    gui_print(cb, lx2, ly, 0x000080); ly += 18;

    gui_hline(px, ly + 4, cw, 0x808080); ly += 12;
    gui_btn(px + cw / 2 - 30, ly, 60, 20, "OK", 0);
}

int fileprops_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), cw = win_client_w(w);
    int ly = fileprops_ok_y(w);
    if (mx >= px + cw / 2 - 30 && mx < px + cw / 2 + 30 && my >= ly && my < ly + 20) {
        return 1;
    }
    return 0;
}

int fileprops_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), cw = win_client_w(w);
    int ly = fileprops_ok_y(w);
    if (mx >= px + cw / 2 - 30 && mx < px + cw / 2 + 30 && my >= ly && my < ly + 20) return 1;
    return 0;
}
