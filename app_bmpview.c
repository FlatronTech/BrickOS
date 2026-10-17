#include "app_bmpview.h"

static char bmpview_file[16] = "";
#define BMPVIEW_BUF ((u8*)0x600000)
#define BMPVIEW_BUF_SZ (54 + 800 * 600 * 4)

static int bmpview_loaded = 0, bmpview_w = 0, bmpview_h = 0;

void open_bmp_viewer(const char* filename) {
    int i = 0;
    while (filename[i] && i < 15) {
        bmpview_file[i] = filename[i];
        i++;
    }
    bmpview_file[i] = 0;

    bmpview_loaded = 0;
    bmpview_w = 0;
    bmpview_h = 0;

    int n = fat_read_file(filename, BMPVIEW_BUF, BMPVIEW_BUF_SZ);
    if (n > 54 && BMPVIEW_BUF[0] == 'B' && BMPVIEW_BUF[1] == 'M') {
        bmpview_w = BMPVIEW_BUF[18] | (BMPVIEW_BUF[19] << 8) | (BMPVIEW_BUF[20] << 16) | (BMPVIEW_BUF[21] << 24);
        int h_raw = (int)(BMPVIEW_BUF[22] | (BMPVIEW_BUF[23] << 8) | (BMPVIEW_BUF[24] << 16) | (BMPVIEW_BUF[25] << 24));
        bmpview_h = h_raw < 0 ? -h_raw : h_raw;
        bmpview_loaded = 1;
    }

    char title[32] = "View: ";
    my_strcat(title, filename);

    int ex = win_find(APP_BMPVIEW);
    if (ex >= 0) {
        win_raise(ex);
        my_strcpy(wins[nwins - 1].title, title);
        wins[nwins - 1].minimized = 0;
        wins[nwins - 1].visible = 1;
        return;
    }
    win_new(APP_BMPVIEW, 80, 60, 500, 380, title);
}

void draw_bmpview(struct Window* w) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);
    gui_rect(px, py0, cw, ch, 0x303030);

    if (!bmpview_loaded) {
        gui_print("Invalid or unsupported BMP", px + 10, py0 + 20, 0xFF4444);
        return;
    }
    if (bmpview_w <= 0 || bmpview_h <= 0) {
        gui_print("Empty image", px + 10, py0 + 20, 0xFF8800);
        return;
    }

    int bpp = BMPVIEW_BUF[28];
    if (bpp != 24 && bpp != 32) {
        gui_print("Only 24/32-bit BMP supported", px + 10, py0 + 20, 0xFF8800);
        return;
    }

    int pxsz = bpp / 8;
    int data_off = BMPVIEW_BUF[10] | (BMPVIEW_BUF[11] << 8) | (BMPVIEW_BUF[12] << 16) | (BMPVIEW_BUF[13] << 24);
    int row_stride = ((bmpview_w * pxsz + 3) / 4) * 4;

    int max_w = cw - 2;
    if (max_w > bmpview_w) max_w = bmpview_w;
    int max_h = ch - 20;
    if (max_h > bmpview_h) max_h = bmpview_h;

    int h_raw = (int)(BMPVIEW_BUF[22] | (BMPVIEW_BUF[23] << 8) | (BMPVIEW_BUF[24] << 16) | (BMPVIEW_BUF[25] << 24));
    int flipped = (h_raw > 0);

    for (int y = 0; y < max_h; y++) {
        int src_y = flipped ? (bmpview_h - 1 - y) : y;
        if (src_y < 0 || src_y >= bmpview_h) continue;

        u8* row = BMPVIEW_BUF + data_off + src_y * row_stride;
        for (int x = 0; x < max_w; x++) {
            u8 b = row[x * pxsz + 0];
            u8 g = row[x * pxsz + 1];
            u8 r = row[x * pxsz + 2];
            gui_pixel(px + 1 + x, py0 + 1 + y, (r << 16) | (g << 8) | b);
        }
    }

    char info[40];
    itoa(info, (u32)bmpview_w);
    my_strcat(info, "x");
    char tmp[8];
    itoa(tmp, (u32)bmpview_h);
    my_strcat(info, tmp);
    my_strcat(info, " px | ");
    char bppb[4];
    itoa(bppb, (u32)bpp);
    my_strcat(info, bppb);
    my_strcat(info, "bpp");

    gui_rect(px, py0 + ch - 18, cw, 18, 0x000000);
    gui_print(info, px + 4, py0 + ch - 16, 0xCCCCCC);
}
