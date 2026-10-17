#include "app_notepad.h"

int note_active = 0;
int note_dirty = 0;
char current_note_file[16] = "NOTE.TXT";

static char note_buf[NOTE_LINES][NOTE_COLS + 1];
static int  note_cx = 0, note_cy = 0;
static u32  note_saved_t = 0;

void note_init(void) {
    note_clear();
    note_active = 0;
    my_strcpy(current_note_file, "NOTE.TXT");
}

void note_clear(void) {
    for (int i = 0; i < NOTE_LINES; i++) {
        for (int j = 0; j <= NOTE_COLS; j++) {
            note_buf[i][j] = 0;
        }
    }
    note_cx = 0;
    note_cy = 0;
    note_saved_t = 0;
    note_dirty = 0;
}

void note_load_file(const char* filename) {
    note_clear();
    int fi = 0;
    while (filename[fi] && fi < 15) {
        current_note_file[fi] = filename[fi];
        fi++;
    }
    current_note_file[fi] = 0;

    static u8 temp_buf[2048];
    int n = fat_read_file(filename, temp_buf, sizeof(temp_buf) - 1);
    if (n > 0) {
        temp_buf[n] = 0;
        int line = 0, col = 0;
        for (int i = 0; i < n && line < NOTE_LINES; i++) {
            char ch = (char)temp_buf[i];
            if (ch == '\r') continue;
            if (ch == '\n') {
                note_buf[line][col] = 0;
                line++;
                col = 0;
            } else {
                if (col < NOTE_COLS - 1) {
                    note_buf[line][col++] = ch;
                    note_buf[line][col] = 0;
                }
            }
        }
        note_cx = 0;
        note_cy = 0;
    }
    note_dirty = 0;
}

void note_save(void) {
    char fbuf[NOTE_LINES * (NOTE_COLS + 2) + 1];
    int fi = 0;
    for (int l = 0; l < NOTE_LINES; l++) {
        int len = my_strlen(note_buf[l]);
        for (int c = 0; c < len; c++) {
            fbuf[fi++] = note_buf[l][c];
        }
        fbuf[fi++] = '\r';
        fbuf[fi++] = '\n';
    }
    fat_write_file(current_note_file[0] ? current_note_file : "NOTE.TXT", (u8*)fbuf, fi);
    note_saved_t = ticks + 200;
    note_dirty = 0;
}

void draw_notepad(struct Window* w) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);

    /* Toolbar */
    gui_rect(px, py0, cw, 22, 0xC0C0C0);
    const char* note_title = note_dirty ? "[*] Notepad" : w->title;
    gui_print_clipped(note_title, px + 6, py0 + 4, 0x000000, px + cw - 140);

    int sbx = px + cw - 120, sby = py0 + 2;
    gui_btn(sbx, sby, 54, 18, note_dirty ? "*Save" : "Save", 0);
    int sabx = px + cw - 62, saby = py0 + 2;
    gui_btn(sabx, saby, 58, 18, "Save As", 0);

    if (note_saved_t && ticks < note_saved_t) {
        gui_print("Saved!", px + cw - 180, py0 + 4, 0x008000);
    }

    /* Text editor area */
    int ta_x = px, ta_y = py0 + 24;
    int c_adv = font_loaded ? 9 : 8;
    int max_vis_lines = (ch - 26) / 16;
    if (max_vis_lines > NOTE_LINES) max_vis_lines = NOTE_LINES;
    int max_vis_cols = (cw - 36) / c_adv;
    if (max_vis_cols > NOTE_COLS) max_vis_cols = NOTE_COLS;

    gui_rect(ta_x, ta_y, cw, ch - 24, 0x1E1E1E);

    for (int line = 0; line < max_vis_lines; line++) {
        int lx = ta_x + 2, ly = ta_y + 2 + line * 16;

        /* Line number */
        char ln[6];
        itoa(ln, (u32)(line + 1));
        gui_print_bg(ln, lx, ly, 0x606060, 0x1E1E1E);
        lx += 24;

        /* Text content */
        int len = my_strlen(note_buf[line]);
        for (int col = 0; col < max_vis_cols && col < len; col++) {
            gui_char(note_buf[line][col], lx + col * c_adv, ly, 0xD4D4D4, 0x1E1E1E);
        }

        /* Blinking cursor */
        if (note_active && line == note_cy && ((ticks / 40) & 1)) {
            if (note_cx <= max_vis_cols) {
                gui_rect(lx + note_cx * c_adv, ly, 2, 14, 0xFFFFFF);
            }
        }
    }
}

void notepad_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);
    int sbx = px + cw - 120, sby = py0 + 2;
    int sabx = px + cw - 62, saby = py0 + 2;

    /* Toolbar buttons */
    if (mx >= sbx && mx < sbx + 54 && my >= sby && my < sby + 18) {
        note_save();
        return;
    }
    if (mx >= sabx && mx < sabx + 58 && my >= saby && my < saby + 18) {
        open_saveas_dialog(0);
        return;
    }

    /* Text area click -> position cursor */
    int ta_x = px, ta_y = py0 + 24;
    int c_adv = font_loaded ? 9 : 8;
    int max_vis_lines = (ch - 26) / 16;
    if (max_vis_lines > NOTE_LINES) max_vis_lines = NOTE_LINES;

    if (mx >= ta_x + 26 && mx < px + cw && my >= ta_y + 2 && my < ta_y + 2 + max_vis_lines * 16) {
        int target_line = (my - (ta_y + 2)) / 16;
        int target_col = (mx - (ta_x + 26)) / c_adv;

        if (target_line >= 0 && target_line < NOTE_LINES) {
            note_cy = target_line;
            int line_len = my_strlen(note_buf[note_cy]);
            if (target_col < 0) target_col = 0;
            if (target_col > line_len) target_col = line_len;
            if (target_col >= NOTE_COLS) target_col = NOTE_COLS - 1;
            note_cx = target_col;
            note_active = 1;
        }
    }
}

void notepad_key(char c, struct Window* w) {
    if (!note_active) return;

    int c_adv = font_loaded ? 9 : 8;
    int max_cols = w ? (win_client_w(w) - 36) / c_adv : 40;
    if (max_cols > NOTE_COLS - 1) max_cols = NOTE_COLS - 1;
    if (max_cols < 10) max_cols = 10;

    /* Navigation keys */
    if (c == KEY_UP) {
        if (note_cy > 0) {
            note_cy--;
            int len = my_strlen(note_buf[note_cy]);
            if (note_cx > len) note_cx = len;
        }
        return;
    }
    if (c == KEY_DOWN) {
        if (note_cy < NOTE_LINES - 1) {
            note_cy++;
            int len = my_strlen(note_buf[note_cy]);
            if (note_cx > len) note_cx = len;
        }
        return;
    }
    if (c == KEY_LEFT) {
        if (note_cx > 0) {
            note_cx--;
        } else if (note_cy > 0) {
            note_cy--;
            note_cx = my_strlen(note_buf[note_cy]);
        }
        return;
    }
    if (c == KEY_RIGHT) {
        int len = my_strlen(note_buf[note_cy]);
        if (note_cx < len) {
            note_cx++;
        } else if (note_cy < NOTE_LINES - 1) {
            note_cy++;
            note_cx = 0;
        }
        return;
    }
    if (c == KEY_HOME) {
        note_cx = 0;
        return;
    }
    if (c == KEY_END) {
        note_cx = my_strlen(note_buf[note_cy]);
        return;
    }

    /* Text editing keys */
    if (c == '\n') {
        if (note_cy < NOTE_LINES - 1) {
            /* Move remaining characters on current line to next line */
            char rest[NOTE_COLS + 1];
            int ri = 0;
            for (int i = note_cx; note_buf[note_cy][i]; i++) {
                rest[ri++] = note_buf[note_cy][i];
            }
            rest[ri] = 0;
            note_buf[note_cy][note_cx] = 0;

            /* Shift lines down */
            for (int l = NOTE_LINES - 1; l > note_cy + 1; l--) {
                my_strcpy(note_buf[l], note_buf[l - 1]);
            }
            my_strcpy(note_buf[note_cy + 1], rest);
            note_cy++;
            note_cx = 0;
            note_dirty = 1;
        }
        return;
    }

    if (c == '\b') {
        if (note_cx > 0) {
            int len = my_strlen(note_buf[note_cy]);
            for (int i = note_cx - 1; i < len; i++) {
                note_buf[note_cy][i] = note_buf[note_cy][i + 1];
            }
            note_cx--;
            note_dirty = 1;
        } else if (note_cy > 0) {
            /* Join with previous line if it fits */
            int prev_len = my_strlen(note_buf[note_cy - 1]);
            int cur_len = my_strlen(note_buf[note_cy]);
            if (prev_len + cur_len < NOTE_COLS - 1) {
                my_strcat(note_buf[note_cy - 1], note_buf[note_cy]);
                /* Shift lines up */
                for (int l = note_cy; l < NOTE_LINES - 1; l++) {
                    my_strcpy(note_buf[l], note_buf[l + 1]);
                }
                note_buf[NOTE_LINES - 1][0] = 0;
                note_cy--;
                note_cx = prev_len;
                note_dirty = 1;
            }
        }
        return;
    }

    if (c == '\t') {
        /* Insert two spaces for tab */
        for (int s = 0; s < 2; s++) {
            int len = my_strlen(note_buf[note_cy]);
            if (len < NOTE_COLS - 1 && note_cx < max_cols) {
                for (int i = len; i >= note_cx; i--) {
                    note_buf[note_cy][i + 1] = note_buf[note_cy][i];
                }
                note_buf[note_cy][note_cx++] = ' ';
                note_dirty = 1;
            }
        }
        return;
    }

    /* Regular printable character */
    if (c >= 32 && c <= 126) {
        int len = my_strlen(note_buf[note_cy]);
        if (note_cx >= max_cols && note_cy < NOTE_LINES - 1) {
            /* Auto wrap to next line */
            note_cy++;
            note_cx = 0;
            len = my_strlen(note_buf[note_cy]);
        }
        if (len < NOTE_COLS - 1 && note_cx < NOTE_COLS - 1) {
            for (int i = len; i >= note_cx; i--) {
                note_buf[note_cy][i + 1] = note_buf[note_cy][i];
            }
            note_buf[note_cy][note_cx++] = c;
            note_dirty = 1;
        }
    }
}

int notepad_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w);
    int sbx = px + cw - 120, sby = py0 + 2;
    int sabx = px + cw - 62, saby = py0 + 2;

    if (mx >= sbx && mx < sbx + 54 && my >= sby && my < sby + 18) return 1;
    if (mx >= sabx && mx < sabx + 58 && my >= saby && my < saby + 18) return 1;
    return 0;
}
