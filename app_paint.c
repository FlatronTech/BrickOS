#include "app_paint.h"

/* Access to kernel backbuffer for direct pixel blit */
extern u8* backbuf;
extern int fpitch, fbpp;

int paint_dirty = 0;
char paint_filename[16] = "UNTITLED.BMP";

static u32 paint_color = 0x000000;
static const u32 paint_palette[] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00CC00,
    0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
    0x808080, 0xFF8000, 0x008060, 0x800080
};
#define PAINT_NC 12

static int is_drawing = 0;
static int prev_draw_x = -1, prev_draw_y = -1;
static u32 paint_saved_t = 0;

void paint_init(void) {
    paint_dirty = 0;
    is_drawing = 0;
    prev_draw_x = -1;
    prev_draw_y = -1;
    paint_saved_t = 0;
    my_strcpy(paint_filename, "UNTITLED.BMP");
    paint_clear();
}

void paint_clear(void) {
    u32* c = PAINT_CANVAS;
    for (int i = 0; i < MAX_CANVAS_W * MAX_CANVAS_H; i++) {
        c[i] = 0xFFFFFF;
    }
}

static void draw_brush_point(int bx, int by) {
    u32* cv = PAINT_CANVAS;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = bx + dx, ny = by + dy;
            if (nx >= 0 && nx < MAX_CANVAS_W && ny >= 0 && ny < MAX_CANVAS_H) {
                cv[ny * MAX_CANVAS_W + nx] = paint_color;
            }
        }
    }
}

static void draw_brush_line(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int sx = (x0 < x1) ? 1 : -1;
    int dy = y1 - y0; if (dy < 0) dy = -dy; dy = -dy;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int iter = 0;
    while (iter++ < (MAX_CANVAS_W + MAX_CANVAS_H) * 2) {
        draw_brush_point(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void save_paint_bmp(void) {
    u8* bmp_buf = PAINT_SCRATCH;
    int W = MAX_CANVAS_W, H = MAX_CANVAS_H;
    int row_stride = ((W * 3 + 3) / 4) * 4;
    int data_sz = row_stride * H;
    int file_sz = 54 + data_sz;

    bmp_buf[0] = 'B'; bmp_buf[1] = 'M';
    bmp_buf[2] = file_sz & 0xFF; bmp_buf[3] = (file_sz >> 8) & 0xFF;
    bmp_buf[4] = (file_sz >> 16) & 0xFF; bmp_buf[5] = (file_sz >> 24) & 0xFF;
    bmp_buf[6] = 0; bmp_buf[7] = 0; bmp_buf[8] = 0; bmp_buf[9] = 0;
    bmp_buf[10] = 54; bmp_buf[11] = 0; bmp_buf[12] = 0; bmp_buf[13] = 0;

    bmp_buf[14] = 40; bmp_buf[15] = 0; bmp_buf[16] = 0; bmp_buf[17] = 0;
    bmp_buf[18] = W & 0xFF; bmp_buf[19] = (W >> 8) & 0xFF;
    bmp_buf[20] = (W >> 16) & 0xFF; bmp_buf[21] = (W >> 24) & 0xFF;
    int nH = -H;
    bmp_buf[22] = nH & 0xFF; bmp_buf[23] = (nH >> 8) & 0xFF;
    bmp_buf[24] = (nH >> 16) & 0xFF; bmp_buf[25] = (nH >> 24) & 0xFF;
    bmp_buf[26] = 1; bmp_buf[27] = 0; bmp_buf[28] = 24; bmp_buf[29] = 0;
    bmp_buf[30] = 0; bmp_buf[31] = 0; bmp_buf[32] = 0; bmp_buf[33] = 0;
    bmp_buf[34] = data_sz & 0xFF; bmp_buf[35] = (data_sz >> 8) & 0xFF;
    bmp_buf[36] = (data_sz >> 16) & 0xFF; bmp_buf[37] = (data_sz >> 24) & 0xFF;
    for (int i = 38; i < 54; i++) bmp_buf[i] = 0;

    u32* cv = PAINT_CANVAS;
    for (int y = 0; y < H; y++) {
        u8* row = bmp_buf + 54 + y * row_stride;
        for (int x = 0; x < W; x++) {
            u32 c = cv[y * W + x];
            row[x * 3 + 0] = c & 0xFF;
            row[x * 3 + 1] = (c >> 8) & 0xFF;
            row[x * 3 + 2] = (c >> 16) & 0xFF;
        }
        for (int p = W * 3; p < row_stride; p++) row[p] = 0;
    }

    fat_write_file(paint_filename[0] ? paint_filename : "UNTITLED.BMP", bmp_buf, 54 + data_sz);
    paint_dirty = 0;
    paint_saved_t = ticks + 200;
}

void paint_mouse_move(struct Window* w, int mx, int my, int btn) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);
    /* canvas starts below 22px toolbar, with 1px border */
    int can_x = px + 1, can_y = py0 + 23;
    int can_w = cw - 2;
    int can_h = ch - 24;
    if (can_w > MAX_CANVAS_W) can_w = MAX_CANVAS_W;
    if (can_h > MAX_CANVAS_H) can_h = MAX_CANVAS_H;
    if (can_w <= 0 || can_h <= 0) return;

    if (btn & 1) {
        if (mx >= can_x && mx < can_x + can_w && my >= can_y && my < can_y + can_h) {
            int cur_x = mx - can_x;
            int cur_y = my - can_y;
            /* clamp to canvas */
            if (cur_x < 0) cur_x = 0;
            if (cur_x >= MAX_CANVAS_W) cur_x = MAX_CANVAS_W - 1;
            if (cur_y < 0) cur_y = 0;
            if (cur_y >= MAX_CANVAS_H) cur_y = MAX_CANVAS_H - 1;

            if (is_drawing && prev_draw_x >= 0 && prev_draw_y >= 0) {
                draw_brush_line(prev_draw_x, prev_draw_y, cur_x, cur_y);
            } else {
                draw_brush_point(cur_x, cur_y);
                is_drawing = 1;
            }
            prev_draw_x = cur_x;
            prev_draw_y = cur_y;
            paint_dirty = 1;
        } else {
            /* Mouse left canvas area — stop line from prev point */
            is_drawing = 0;
            prev_draw_x = -1;
            prev_draw_y = -1;
        }
    } else {
        /* Button released — end stroke */
        is_drawing = 0;
        prev_draw_x = -1;
        prev_draw_y = -1;
    }
}

void paint_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w);
    int cbx = px + cw - 170, cby = py0 + 2;
    int sbx = px + cw - 118, sby = py0 + 2;
    int sabx = px + cw - 60, saby = py0 + 2;

    for (int i = 0; i < PAINT_NC; i++) {
        int bx = px + 58 + i * 16, by2 = py0 + 3;
        if (mx >= bx && mx < bx + 14 && my >= by2 && my < by2 + 14) {
            paint_color = paint_palette[i];
            return;
        }
    }

    if (mx >= cbx && mx < cbx + 48 && my >= cby && my < cby + 18) {
        paint_clear();
        paint_dirty = 0;
        is_drawing = 0;
        prev_draw_x = -1;
        prev_draw_y = -1;
        return;
    }

    if (mx >= sbx && mx < sbx + 54 && my >= sby && my < sby + 18) {
        save_paint_bmp();
        return;
    }

    if (mx >= sabx && mx < sabx + 56 && my >= saby && my < saby + 18) {
        open_saveas_dialog(1);
        return;
    }
}

/* Blit canvas rows directly into backbuffer (avoids gui_pixel overhead).    */
static void paint_blit_canvas(int can_x, int can_y, int can_w, int can_h) {
    if (!backbuf || fbpp < 24) return;
    int bpp_bytes = fbpp / 8;
    u32* cv = PAINT_CANVAS;
    for (int j = 0; j < can_h; j++) {
        int sy = can_y + j;
        if (sy < 0 || sy >= fh) continue;
        u32* src_row = cv + j * MAX_CANVAS_W;
        u8*  dst_row = backbuf + sy * fpitch + can_x * bpp_bytes;
        for (int i = 0; i < can_w; i++) {
            u32 c = src_row[i];
            dst_row[i * bpp_bytes + 0] = (u8)(c & 0xFF);
            dst_row[i * bpp_bytes + 1] = (u8)((c >> 8)  & 0xFF);
            dst_row[i * bpp_bytes + 2] = (u8)((c >> 16) & 0xFF);
            if (bpp_bytes == 4) dst_row[i * bpp_bytes + 3] = 0;
        }
    }
}

void draw_paint(struct Window* w, int mx, int my, int btn) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);

    /* Toolbar background */
    gui_rect(px, py0, cw, 22, 0xC0C0C0);
    gui_print("Color:", px + 4, py0 + 3, 0x000000);

    /* Palette swatches */
    for (int i = 0; i < PAINT_NC; i++) {
        int bx = px + 58 + i * 16, by2 = py0 + 3;
        gui_rect(bx, by2, 14, 14, paint_palette[i]);
        gui_hline(bx, by2, 14, 0x000000); gui_vline(bx, by2, 14, 0x000000);
        gui_hline(bx, by2 + 13, 14, 0x000000); gui_vline(bx + 13, by2, 14, 0x000000);
        if (paint_palette[i] == paint_color) {
            gui_hline(bx - 1, by2 - 1, 16, 0xFF0000);
            gui_hline(bx - 1, by2 + 14, 16, 0xFF0000);
            gui_vline(bx - 1, by2 - 1, 16, 0xFF0000);
            gui_vline(bx + 14, by2 - 1, 16, 0xFF0000);
        }
    }

    int cbx = px + cw - 170, cby = py0 + 2;
    gui_btn(cbx, cby, 48, 18, "Clear", 0);
    int sbx = px + cw - 118, sby = py0 + 2;
    gui_btn(sbx, sby, 54, 18, paint_dirty ? "*Save" : "Save", 0);
    int sabx = px + cw - 60, saby = py0 + 2;
    gui_btn(sabx, saby, 56, 18, "Save As", 0);

    if (paint_saved_t && ticks < paint_saved_t) {
        gui_print("Saved!", px + cw - 230, py0 + 3, 0x008000);
    }

    /* Canvas area */
    int can_x = px + 1, can_y = py0 + 23;
    int can_w = cw - 2;
    int can_h = ch - 24;
    if (can_w > MAX_CANVAS_W) can_w = MAX_CANVAS_W;
    if (can_h > MAX_CANVAS_H) can_h = MAX_CANVAS_H;
    if (can_w <= 0 || can_h <= 0) return;

    /* Blit canvas pixels directly to the back buffer */
    paint_blit_canvas(can_x, can_y, can_w, can_h);

    /* Canvas border */
    gui_hline(can_x - 1, can_y - 1, can_w + 2, 0x808080);
    gui_vline(can_x - 1, can_y - 1, can_h + 2, 0x808080);
    gui_hline(can_x - 1, can_y + can_h, can_w + 2, 0x808080);
    gui_vline(can_x + can_w, can_y - 1, can_h + 2, 0x808080);
}

int paint_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w);
    int cbx = px + cw - 170, cby = py0 + 2;
    int sbx = px + cw - 118, sby = py0 + 2;
    int sabx = px + cw - 60, saby = py0 + 2;

    if (my >= py0 + 3 && my < py0 + 17 && mx >= px + 58 && mx < px + 58 + PAINT_NC * 16) return 1;
    if (mx >= cbx && mx < cbx + 48 && my >= cby && my < cby + 18) return 1;
    if (mx >= sbx && mx < sbx + 54 && my >= sby && my < sby + 18) return 1;
    if (mx >= sabx && mx < sabx + 56 && my >= saby && my < saby + 18) return 1;
    return 0;
}
