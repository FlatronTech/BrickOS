#include "app_calc.h"

int calc_active = 0;

static char calc_display[32] = "0";
static int  operand1 = 0;
static char pending_op = 0;
static int  start_new_number = 1;
static int  error_state = 0;

static const char* calc_btn_labels[5][4] = {
    {"C", "CE", "<-", "+/-"},
    {"7", "8",  "9",  "/"},
    {"4", "5",  "6",  "*"},
    {"1", "2",  "3",  "-"},
    {"0", "C",  "=",  "+"}
};

void calc_init(void) {
    calc_active = 0;
    operand1 = 0;
    pending_op = 0;
    start_new_number = 1;
    error_state = 0;
    my_strcpy(calc_display, "0");
}

static int parse_int(const char* s) {
    int res = 0, sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

static void calc_execute_op(void) {
    if (!pending_op || error_state) return;

    int op2 = parse_int(calc_display);
    int res = 0;

    if (pending_op == '+') {
        res = operand1 + op2;
    } else if (pending_op == '-') {
        res = operand1 - op2;
    } else if (pending_op == '*') {
        res = operand1 * op2;
    } else if (pending_op == '/') {
        if (op2 == 0) {
            error_state = 1;
            my_strcpy(calc_display, "Error: Div by 0");
            pending_op = 0;
            return;
        }
        res = operand1 / op2;
    }

    itoa_signed(calc_display, res);
    operand1 = res;
    pending_op = 0;
}

static void calc_input_digit(char d) {
    if (error_state) return;

    if (start_new_number || (calc_display[0] == '0' && calc_display[1] == 0)) {
        calc_display[0] = d;
        calc_display[1] = 0;
        start_new_number = 0;
    } else {
        int len = my_strlen(calc_display);
        if (len < 12) {
            calc_display[len] = d;
            calc_display[len + 1] = 0;
        }
    }
}

static void calc_input_op(char op) {
    if (error_state) return;

    if (pending_op && !start_new_number) {
        calc_execute_op();
        if (error_state) return;
    } else {
        operand1 = parse_int(calc_display);
    }

    pending_op = op;
    start_new_number = 1;
}

static void calc_input_equals(void) {
    if (error_state) return;
    if (pending_op) {
        calc_execute_op();
        start_new_number = 1;
    }
}

static void calc_input_clear(void) {
    calc_init();
}

static void calc_input_clear_entry(void) {
    if (error_state) {
        calc_init();
        return;
    }
    my_strcpy(calc_display, "0");
    start_new_number = 1;
}

static void calc_input_backspace(void) {
    if (error_state || start_new_number) return;
    int len = my_strlen(calc_display);
    if (len > 1) {
        calc_display[len - 1] = 0;
    } else {
        calc_display[0] = '0';
        calc_display[1] = 0;
        start_new_number = 1;
    }
}

static void calc_input_negate(void) {
    if (error_state) return;
    int val = parse_int(calc_display);
    val = -val;
    itoa_signed(calc_display, val);
}

void calc_key(char c) {
    if (c >= '0' && c <= '9') {
        calc_input_digit(c);
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_input_op(c);
    } else if (c == '=' || c == '\n') {
        calc_input_equals();
    } else if (c == '\b') {
        calc_input_backspace();
    } else if (c == 'c' || c == 'C' || c == 27) {
        calc_input_clear();
    }
}

void calc_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w);
    int grid_x = px + 8, grid_y = py0 + 44;
    int bw = 46, bh = 32, gap_x = 4, gap_y = 4;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = grid_x + c * (bw + gap_x);
            int by = grid_y + r * (bh + gap_y);
            if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
                const char* lbl = calc_btn_labels[r][c];
                if (lbl[0] >= '0' && lbl[0] <= '9') {
                    calc_input_digit(lbl[0]);
                } else if (!my_strcmp(lbl, "+")) {
                    calc_input_op('+');
                } else if (!my_strcmp(lbl, "-")) {
                    calc_input_op('-');
                } else if (!my_strcmp(lbl, "*")) {
                    calc_input_op('*');
                } else if (!my_strcmp(lbl, "/")) {
                    calc_input_op('/');
                } else if (!my_strcmp(lbl, "=")) {
                    calc_input_equals();
                } else if (!my_strcmp(lbl, "C")) {
                    calc_input_clear();
                } else if (!my_strcmp(lbl, "CE")) {
                    calc_input_clear_entry();
                } else if (!my_strcmp(lbl, "<-")) {
                    calc_input_backspace();
                } else if (!my_strcmp(lbl, "+/-")) {
                    calc_input_negate();
                }
                return;
            }
        }
    }
}

void draw_calc(struct Window* w) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);

    /* Main background */
    gui_rect(px, py0, cw, ch, 0xC0C0C0);

    /* Readout display box (sunken 3D effect) */
    int disp_x = px + 8, disp_y = py0 + 8, disp_w = cw - 16, disp_h = 28;
    gui_rect(disp_x, disp_y, disp_w, disp_h, 0xFFFFFF);
    gui_hline(disp_x, disp_y, disp_w, 0x808080);
    gui_vline(disp_x, disp_y, disp_h, 0x808080);
    gui_hline(disp_x + 1, disp_y + 1, disp_w - 2, 0x000000);
    gui_vline(disp_x + 1, disp_y + 1, disp_h - 2, 0x000000);
    gui_hline(disp_x, disp_y + disp_h - 1, disp_w, 0xFFFFFF);
    gui_vline(disp_x + disp_w - 1, disp_y, disp_h, 0xFFFFFF);

    /* Right-aligned display text */
    int txt_w = gui_str_width(calc_display);
    int txt_x = disp_x + disp_w - txt_w - 8;
    if (txt_x < disp_x + 6) txt_x = disp_x + 6;
    u32 text_col = error_state ? 0xCC0000 : 0x000000;
    gui_print(calc_display, txt_x, disp_y + 6, text_col);

    /* Buttons */
    int grid_x = px + 8, grid_y = py0 + 44;
    int bw = 46, bh = 32, gap_x = 4, gap_y = 4;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = grid_x + c * (bw + gap_x);
            int by = grid_y + r * (bh + gap_y);
            const char* lbl = calc_btn_labels[r][c];

            gui_btn(bx, by, bw, bh, lbl, 0);
        }
    }
}

int calc_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w);
    int grid_x = px + 8, grid_y = py0 + 44;
    int bw = 46, bh = 32, gap_x = 4, gap_y = 4;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = grid_x + c * (bw + gap_x);
            int by = grid_y + r * (bh + gap_y);
            if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) return 1;
        }
    }
    return 0;
}
