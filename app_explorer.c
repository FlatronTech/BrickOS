#include "app_explorer.h"

static int expl_sel_idx = -1;
static int expl_last_idx = -1;
static u32 expl_last_t = 0;

void draw_explorer(struct Window* w) {
    int px = win_client_x(w), py0 = win_client_y(w);
    int cw = win_client_w(w), ch = win_client_h(w);

    /* Toolbar / address bar */
    gui_rect(px, py0, cw, 16, 0xC0C0C0);
    if (w->app_state == 0) {
        gui_print("My Computer", px + 4, py0, 0x000000);
        /* Breadcrumb */
        gui_rect(px, py0 + 18, cw, 14, 0xFFFFFF);
        gui_print("My Computer", px + 4, py0 + 18, 0x000080);
        /* Drive icon */
        int dx = px + 30, dy = py0 + 38;
        gui_rect(dx, dy, 50, 44, 0xC0C0C0);
        gui_rect(dx + 2, dy + 2, 46, 40, 0x8080A0);
        gui_rect(dx + 8, dy + 2, 28, 14, 0xD0D0FF);
        gui_rect(dx + 14, dy + 28, 22, 14, 0x606060);
        gui_rect(dx + 18, dy + 30, 14, 10, 0xA0A0C0);
        gui_print("A:", dx + 16, dy + 15, 0x000000);
        gui_print("3.5 Floppy (A:)", px + 4, dy + 48, 0x000000);
        gui_print("Click to open", px + 4, dy + 66, 0x808080);

        /* Status bar */
        gui_rect(px, py0 + ch - 14, cw, 14, 0xC0C0C0);
        gui_hline(px, py0 + ch - 15, cw, 0x808080);
        gui_print("1 drive", px + 4, py0 + ch - 13, 0x000000);
    } else {
        /* File list view */
        gui_btn(px, py0, 32, 16, "<-", 0);
        gui_rect(px + 34, py0, cw - 34, 16, 0xFFFFFF);
        gui_print("A:\\", px + 38, py0, 0x000080);

        int hy = py0 + 18;
        gui_rect(px, hy, cw, 14, 0xC0C0C0);
        int col_nm = px + 20, col_sz = px + 170, col_ty = px + 270;
        gui_print("Name", col_nm, hy + 1, 0x000000);
        gui_print("Size", col_sz, hy + 1, 0x000000);
        gui_print("Type", col_ty, hy + 1, 0x000000);

        u8* root = ramdisk + root_sect * 512;
        int fy = hy + 16, count = 0;
        for (int i = 0; i < root_cnt; i++) {
            u8* e = root + i * 32;
            if (e[0] == 0) break;
            if (e[0] == 0xE5 || e[11] == 0x0F) continue;

            int sel = (count == expl_sel_idx);
            if (sel) gui_rect(px, fy, cw, 16, 0x000080);
            else if (count & 1) gui_rect(px, fy, cw, 16, 0xEEEEFF);

            int is_txt = (e[8] == 'T' && e[9] == 'X' && e[10] == 'T');
            int is_inf = (e[8] == 'I' && e[9] == 'N' && e[10] == 'F');
            int is_bin = (e[8] == 'B' && e[9] == 'I' && e[10] == 'N');
            int is_bmp = (e[8] == 'B' && e[9] == 'M' && e[10] == 'P');

            /* File icon */
            int ix = px + 2, iy = fy + 2;
            gui_rect(ix, iy, 12, 12, 0xFFFFFF);
            gui_hline(ix, iy, 12, 0x808080); gui_vline(ix, iy, 12, 0x808080);
            gui_hline(ix, iy + 11, 12, 0x808080); gui_vline(ix + 11, iy, 12, 0x808080);
            gui_rect(ix + 8, iy, 4, 4, 0xC0C0C0);
            gui_pixel(ix + 8, iy, 0x808080); gui_pixel(ix + 11, iy + 3, 0x808080);

            if (is_txt) {
                gui_hline(ix + 2, iy + 4, 7, 0x0000AA);
                gui_hline(ix + 2, iy + 6, 7, 0x0000AA);
                gui_hline(ix + 2, iy + 8, 5, 0x0000AA);
            } else if (is_inf) {
                gui_rect(ix + 3, iy + 3, 6, 7, 0xFF8000);
            } else if (is_bin) {
                gui_rect(ix + 2, iy + 3, 8, 6, 0x404040);
                gui_print("EXE", ix + 1, iy + 3, 0x00FF00);
            } else if (is_bmp) {
                gui_rect(ix + 2, iy + 3, 8, 7, 0xFFFF00);
                gui_pixel(ix + 4, iy + 5, 0xFF0000);
                gui_pixel(ix + 7, iy + 5, 0x00AA00);
                gui_pixel(ix + 6, iy + 7, 0x0000FF);
            }

            char nm[14];
            int ni = 0;
            for (int j = 0; j < 8 && e[j] != ' '; j++) nm[ni++] = e[j];
            nm[ni++] = '.';
            for (int j = 8; j < 11 && e[j] != ' '; j++) nm[ni++] = e[j];
            nm[ni] = 0;

            u32 txt_col = sel ? 0xFFFFFF : 0x000000;
            gui_print(nm, col_nm, fy + 1, txt_col);

            u32 sz = e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24);
            char szb[16];
            format_size(szb, sz);
            gui_print(szb, col_sz, fy + 1, txt_col);

            const char* ty = "Data";
            if (is_txt) ty = "Text File";
            else if (is_inf) ty = "INF Driver";
            else if (is_bin) ty = "Binary";
            else if (is_bmp) ty = "Bitmap";
            gui_print(ty, col_ty, fy + 1, txt_col);

            fy += 16;
            count++;
            if (fy > py0 + ch - 20) break;
        }

        /* Status bar */
        gui_rect(px, py0 + ch - 15, cw, 15, 0xC0C0C0);
        gui_hline(px, py0 + ch - 16, cw, 0x808080);
        char cb[6];
        itoa(cb, (u32)count);
        gui_print(cb, px + 4, py0 + ch - 14, 0x000000);
        gui_print(" files", px + 4 + gui_str_width(cb), py0 + ch - 14, 0x000000);
    }
}

void explorer_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w);
    if (w->app_state == 0) {
        int dx = px + 30, dy = py0 + 38;
        if (mx >= dx && mx < dx + 50 && my >= dy && my < dy + 60) {
            w->app_state = 1;
        }
    } else {
        if (mx >= px && mx < px + 32 && my >= py0 && my < py0 + 16) {
            w->app_state = 0;
            expl_sel_idx = -1;
            return;
        }
        int hy = py0 + 18;
        if (my >= hy + 16) {
            int f_idx = (my - (hy + 16)) / 16;
            u8* root = ramdisk + root_sect * 512;
            int cnt = 0;
            for (int i = 0; i < root_cnt; i++) {
                u8* e = root + i * 32;
                if (e[0] == 0) break;
                if (e[0] == 0xE5 || e[11] == 0x0F) continue;

                if (cnt == f_idx) {
                    char nm[14];
                    int ni = 0;
                    for (int j = 0; j < 8 && e[j] != ' '; j++) nm[ni++] = e[j];
                    nm[ni++] = '.';
                    for (int j = 8; j < 11 && e[j] != ' '; j++) nm[ni++] = e[j];
                    nm[ni] = 0;

                    int is_bin = (e[8] == 'B' && e[9] == 'I' && e[10] == 'N');
                    int is_bmp = (e[8] == 'B' && e[9] == 'M' && e[10] == 'P');

                    if (f_idx == expl_last_idx && ticks - expl_last_t < 80) {
                        if (is_bmp) {
                            open_bmp_viewer(nm);
                        } else if (!is_bin) {
                            open_file_in_notepad(nm);
                        }
                        expl_sel_idx = -1;
                    } else {
                        expl_sel_idx = f_idx;
                    }
                    expl_last_idx = f_idx;
                    expl_last_t = ticks;
                    return;
                }
                cnt++;
            }
        }
    }
}

void explorer_key(char c, struct Window* w) {
    if (w->app_state == 0) {
        if (c == '\n') w->app_state = 1;
        return;
    }

    if (c == '\b') {
        w->app_state = 0;
        expl_sel_idx = -1;
        return;
    }

    if (c == KEY_UP) {
        if (expl_sel_idx > 0) expl_sel_idx--;
        return;
    }

    if (c == KEY_DOWN) {
        expl_sel_idx++;
        return;
    }

    if (c == '\n' && expl_sel_idx >= 0) {
        u8* root = ramdisk + root_sect * 512;
        int cnt = 0;
        for (int i = 0; i < root_cnt; i++) {
            u8* e = root + i * 32;
            if (e[0] == 0) break;
            if (e[0] == 0xE5 || e[11] == 0x0F) continue;

            if (cnt == expl_sel_idx) {
                char nm[14];
                int ni = 0;
                for (int j = 0; j < 8 && e[j] != ' '; j++) nm[ni++] = e[j];
                nm[ni++] = '.';
                for (int j = 8; j < 11 && e[j] != ' '; j++) nm[ni++] = e[j];
                nm[ni] = 0;

                int is_bin = (e[8] == 'B' && e[9] == 'I' && e[10] == 'N');
                int is_bmp = (e[8] == 'B' && e[9] == 'M' && e[10] == 'P');

                if (is_bmp) {
                    open_bmp_viewer(nm);
                } else if (!is_bin) {
                    open_file_in_notepad(nm);
                }
                return;
            }
            cnt++;
        }
    }
}

int explorer_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w);
    if (w->app_state == 0) {
        int dx = px + 30, dy = py0 + 38;
        if (mx >= dx && mx < dx + 50 && my >= dy && my < dy + 60) return 1;
    } else {
        if (mx >= px && mx < px + 32 && my >= py0 && my < py0 + 16) return 1;
        int hy = py0 + 18;
        if (my >= hy + 16 && my < hy + 16 + 16 * 12 && mx >= px && mx < px + cw) return 1;
    }
    return 0;
}
