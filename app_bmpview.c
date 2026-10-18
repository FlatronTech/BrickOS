#include "app_bmpview.h"

extern u8* backbuf;
extern int fpitch, fbpp;

static char bmpview_file[16] = "";
/* 0xD00000 (13 MB) is well separated from backbuf (2MB..6.7MB), ramdisk (8MB), and paint (10MB-12.5MB) */
#define BMPVIEW_BUF ((u8*)0xD00000)
#define BMPVIEW_BUF_SZ (54 + 800 * 600 * 4)

static int bmpview_loaded = 0, bmpview_w = 0, bmpview_h = 0;
static int bmpview_bpp = 0;
static u32 bmpview_data_off = 54;
static int bmpview_flipped = 1;

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
    bmpview_bpp = 0;

    int n = fat_read_file(filename, BMPVIEW_BUF, BMPVIEW_BUF_SZ);
    if (n >= 54 && BMPVIEW_BUF[0] == 'B' && BMPVIEW_BUF[1] == 'M') {
        u32 w_u = (u32)BMPVIEW_BUF[18] | ((u32)BMPVIEW_BUF[19] << 8) | ((u32)BMPVIEW_BUF[20] << 16) | ((u32)BMPVIEW_BUF[21] << 24);
        u32 h_u = (u32)BMPVIEW_BUF[22] | ((u32)BMPVIEW_BUF[23] << 8) | ((u32)BMPVIEW_BUF[24] << 16) | ((u32)BMPVIEW_BUF[25] << 24);
        bmpview_w = (int)w_u;
        int h_raw = (int)h_u;
        bmpview_h = h_raw < 0 ? -h_raw : h_raw;
        bmpview_flipped = (h_raw > 0); /* positive height = standard bottom-up */

        bmpview_bpp = BMPVIEW_BUF[28] | (BMPVIEW_BUF[29] << 8);
        bmpview_data_off = (u32)BMPVIEW_BUF[10] | ((u32)BMPVIEW_BUF[11] << 8) | ((u32)BMPVIEW_BUF[12] << 16) | ((u32)BMPVIEW_BUF[13] << 24);
        if (bmpview_data_off < 54 || bmpview_data_off >= (u32)BMPVIEW_BUF_SZ) {
            bmpview_data_off = 54;
        }

        if (bmpview_w > 0 && bmpview_h > 0 && (bmpview_bpp == 8 || bmpview_bpp == 24 || bmpview_bpp == 32)) {
            bmpview_loaded = 1;
        }
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
    gui_rect(px, py0, cw, ch, 0x2A2A2A);

    if (!bmpview_loaded) {
        gui_print("Invalid or unsupported BMP", px + 14, py0 + 20, 0xFF4444);
        return;
    }
    if (bmpview_w <= 0 || bmpview_h <= 0) {
        gui_print("Empty image", px + 14, py0 + 20, 0xFF8800);
        return;
    }

    int pxsz = (bmpview_bpp == 8) ? 1 : (bmpview_bpp / 8);
    int row_stride = ((bmpview_w * pxsz + 3) / 4) * 4;

    /* Palette location for 8-bit BMP */
    const u8* palette = BMPVIEW_BUF + 54;
    u32 dib_sz = (u32)BMPVIEW_BUF[14] | ((u32)BMPVIEW_BUF[15] << 8) | ((u32)BMPVIEW_BUF[16] << 16) | ((u32)BMPVIEW_BUF[17] << 24);
    if (dib_sz >= 40 && dib_sz < 128) {
        palette = BMPVIEW_BUF + 14 + dib_sz;
    }

    int max_w = cw - 2;
    if (max_w > bmpview_w) max_w = bmpview_w;
    int max_h = ch - 22;
    if (max_h > bmpview_h) max_h = bmpview_h;

    int bpp_bytes = fbpp / 8;

    /* Blit BMP rows to display backbuffer */
    for (int y = 0; y < max_h; y++) {
        int src_y = bmpview_flipped ? (bmpview_h - 1 - y) : y;
        if (src_y < 0 || src_y >= bmpview_h) continue;

        int dy = py0 + 1 + y;
        if (dy < 0 || dy >= fh) continue;

        u32 row_offset = bmpview_data_off + (u32)src_y * (u32)row_stride;
        if (row_offset + (u32)max_w * (u32)pxsz > (u32)BMPVIEW_BUF_SZ) continue;

        u8* row = BMPVIEW_BUF + row_offset;
        u8* dst = backbuf + dy * fpitch + (px + 1) * bpp_bytes;

        for (int x = 0; x < max_w; x++) {
            u8 r, g, b;
            if (bmpview_bpp == 8) {
                u8 idx = row[x];
                b = palette[idx * 4 + 0];
                g = palette[idx * 4 + 1];
                r = palette[idx * 4 + 2];
            } else {
                b = row[x * pxsz + 0];
                g = row[x * pxsz + 1];
                r = row[x * pxsz + 2];
            }

            if (bpp_bytes == 4) {
                u32* dst32 = (u32*)dst;
                dst32[x] = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
            } else {
                dst[x * 3 + 0] = b;
                dst[x * 3 + 1] = g;
                dst[x * 3 + 2] = r;
            }
        }
    }

    /* Status bar at bottom */
    char info[48];
    itoa(info, (u32)bmpview_w);
    my_strcat(info, "x");
    char tmp[8];
    itoa(tmp, (u32)bmpview_h);
    my_strcat(info, tmp);
    my_strcat(info, " px | ");
    char bppb[4];
    itoa(bppb, (u32)bmpview_bpp);
    my_strcat(info, bppb);
    my_strcat(info, "bpp | ");
    my_strcat(info, bmpview_file);

    gui_rect(px, py0 + ch - 18, cw, 18, 0x1E1E1E);
    gui_hline(px, py0 + ch - 19, cw, 0x505050);
    gui_print(info, px + 6, py0 + ch - 15, 0xE0E0E0);
}
