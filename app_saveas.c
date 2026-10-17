#include "app_saveas.h"
#include "app_paint.h"
#include "app_notepad.h"

static char saveas_buf[16] = "FILE.TXT";
static int  saveas_target = 0; /* 0 = Notepad, 1 = Paint */

void open_saveas_dialog(int for_paint) {
    int ex = win_find(APP_SAVEAS);
    if (ex >= 0) {
        win_raise(ex);
        return;
    }
    saveas_target = for_paint;
    if (for_paint) {
        my_strcpy(saveas_buf, paint_filename[0] ? paint_filename : "UNTITLED.BMP");
    } else {
        my_strcpy(saveas_buf, current_note_file[0] ? current_note_file : "NOTE.TXT");
    }
    int dw = 300, dh = 120;
    int dx = (fw - dw) / 2, dy = (fh - dh) / 2;
    win_new(APP_SAVEAS, dx, dy, dw, dh, for_paint ? "Save Image As..." : "Save Text As...");
}

void saveas_close(void) {
    for (int i = 0; i < nwins; i++) {
        if (wins[i].app_id == APP_SAVEAS) {
            for (int j = i; j < nwins - 1; j++) wins[j] = wins[j + 1];
            nwins--;
            break;
        }
    }
}

void saveas_confirm(void) {
    if (!saveas_buf[0]) return;
    if (saveas_target == 1) {
        my_strcpy(paint_filename, saveas_buf);
        save_paint_bmp();
        for (int i = 0; i < nwins; i++) {
            if (wins[i].app_id == APP_PAINT) {
                char nt[32] = "Paint - ";
                my_strcat(nt, paint_filename);
                my_strcpy(wins[i].title, nt);
                break;
            }
        }
    } else {
        my_strcpy(current_note_file, saveas_buf);
        note_save();
        for (int i = 0; i < nwins; i++) {
            if (wins[i].app_id == APP_NOTEPAD) {
                char nt[32] = "Notepad - ";
                my_strcat(nt, current_note_file);
                my_strcpy(wins[i].title, nt);
                break;
            }
        }
    }
    saveas_close();
}

void draw_saveas(struct Window* w) {
    int px = win_client_x(w), py = win_client_y(w), cw = win_client_w(w);
    gui_rect(px, py, cw, win_client_h(w), 0xC0C0C0);
    gui_print("Enter filename (8.3 format):", px + 10, py + 8, 0x000000);

    int ix = px + 10, iy = py + 28, iw = cw - 20, ih = 20;
    gui_rect(ix, iy, iw, ih, 0xFFFFFF);
    gui_hline(ix, iy, iw, 0x808080); gui_vline(ix, iy, ih, 0x808080);
    gui_hline(ix, iy + ih - 1, iw, 0xFFFFFF); gui_vline(ix + iw - 1, iy, ih, 0xFFFFFF);
    gui_print(saveas_buf, ix + 6, iy + 2, 0x000000);

    if ((ticks / 40) & 1) {
        int sl = gui_str_width(saveas_buf);
        gui_rect(ix + 6 + sl, iy + 3, 2, 14, 0x000000);
    }

    int by = py + 60;
    gui_btn(px + cw - 140, by, 60, 22, "Save", 0);
    gui_btn(px + cw - 70, by, 60, 22, "Cancel", 0);
}

void saveas_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py = win_client_y(w), cw = win_client_w(w);
    int by = py + 60;
    if (mx >= px + cw - 140 && mx < px + cw - 80 && my >= by && my < by + 22) {
        saveas_confirm();
        return;
    }
    if (mx >= px + cw - 70 && mx < px + cw - 10 && my >= by && my < by + 22) {
        saveas_close();
        return;
    }
}

void saveas_key(char c) {
    int len = my_strlen(saveas_buf);
    if (c == '\n') {
        saveas_confirm();
    } else if (c == 27) {
        saveas_close();
    } else if (c == '\b') {
        if (len > 0) saveas_buf[len - 1] = 0;
    } else if (len < 12) {
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            saveas_buf[len] = c;
            saveas_buf[len + 1] = 0;
        }
    }
}

int saveas_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py = win_client_y(w), cw = win_client_w(w);
    int by = py + 60;
    if (mx >= px + cw - 140 && mx < px + cw - 10 && my >= by && my < by + 22) return 1;
    return 0;
}
