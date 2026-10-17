#include "app_terminal.h"

int term_active = 0;

static char term_screen[TERM_LINES][TERM_COLS + 1];
static int  term_cx_p = 0, term_cy_p = 0;
static char term_cmd[64];
static int  term_cmdlen = 0;

void term_init(void) {
    term_clear_screen();
    term_active = 0;
    term_cmdlen = 0;
    term_cmd[0] = 0;
    term_print("BrickOS Terminal v2.0\nType 'help' for commands\n");
}

void term_clear_screen(void) {
    for (int i = 0; i < TERM_LINES; i++) {
        for (int j = 0; j <= TERM_COLS; j++) {
            term_screen[i][j] = 0;
        }
    }
    term_cx_p = 0;
    term_cy_p = 0;
}

void term_scroll_up(void) {
    for (int i = 0; i < TERM_LINES - 1; i++) {
        my_memcpy(term_screen[i], term_screen[i + 1], TERM_COLS + 1);
    }
    for (int j = 0; j <= TERM_COLS; j++) {
        term_screen[TERM_LINES - 1][j] = 0;
    }
    term_cy_p = TERM_LINES - 1;
    term_cx_p = 0;
}

void term_putchar(char c) {
    if (term_cy_p >= TERM_LINES) {
        term_scroll_up();
    }
    if (term_cy_p < 0) term_cy_p = 0;

    if (c == '\n') {
        term_screen[term_cy_p][term_cx_p] = 0;
        term_cx_p = 0;
        term_cy_p++;
        if (term_cy_p >= TERM_LINES) {
            term_scroll_up();
        }
        return;
    }

    if (c == '\b') {
        if (term_cx_p > 0) {
            term_cx_p--;
            term_screen[term_cy_p][term_cx_p] = 0;
        }
        return;
    }

    if (term_cx_p >= TERM_COLS) {
        term_screen[term_cy_p][TERM_COLS] = 0;
        term_cx_p = 0;
        term_cy_p++;
        if (term_cy_p >= TERM_LINES) {
            term_scroll_up();
        }
    }

    term_screen[term_cy_p][term_cx_p++] = c;
    term_screen[term_cy_p][term_cx_p] = 0;
}

void term_print(const char* s) {
    if (!s) return;
    while (*s) {
        term_putchar(*s++);
    }
}

void term_dec(u32 v) {
    char b[16];
    itoa(b, v);
    term_print(b);
}

static void term_cmd_cat(const char* filename) {
    while (*filename == ' ') filename++;
    if (!*filename) {
        term_print("Usage: cat <filename>\n");
        return;
    }
    static u8 cat_buf[512];
    int n = fat_read_file(filename, cat_buf, sizeof(cat_buf) - 1);
    if (n < 0) {
        term_print("File not found: ");
        term_print(filename);
        term_putchar('\n');
        return;
    }
    cat_buf[n] = 0;
    for (int i = 0; i < n; i++) {
        char ch = (char)cat_buf[i];
        if (ch == '\r') continue;
        term_putchar(ch);
    }
    if (n > 0 && cat_buf[n - 1] != '\n') {
        term_putchar('\n');
    }
}

void term_exec(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (!cmd[0]) return;

    if (!my_strcmp(cmd, "help")) {
        term_print("Available commands:\n");
        term_print("  help      - show this list\n");
        term_print("  ls        - list directory files\n");
        term_print("  cat <f>   - view text file contents\n");
        term_print("  time      - system uptime and clock\n");
        term_print("  meminfo   - memory regions\n");
        term_print("  sysinfo   - CPU and system info\n");
        term_print("  clear     - clear screen\n");
        term_print("  reboot    - restart system\n");
        term_print("  shutdown  - power off system\n");
        term_print("  vesainfo  - VESA framebuffer info\n");
        return;
    }

    if (!my_strcmp(cmd, "clear")) {
        term_clear_screen();
        return;
    }

    if (!my_strcmp(cmd, "ls")) {
        u8* root = ramdisk + root_sect * 512;
        int count = 0;
        for (int i = 0; i < root_cnt; i++) {
            u8* e = root + i * 32;
            if (e[0] == 0) break;
            if (e[0] == 0xE5 || e[11] == 0x0F) continue;

            for (int j = 0; j < 8; j++) {
                if (e[j] != ' ') term_putchar(e[j]);
            }
            term_putchar('.');
            for (int j = 8; j < 11; j++) {
                if (e[j] != ' ') term_putchar(e[j]);
            }
            term_print("  ");

            u32 sz = e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24);
            char szb[16];
            format_size(szb, sz);
            term_print(szb);
            term_putchar('\n');
            count++;
        }
        term_dec(count);
        term_print(" file(s)\n");
        return;
    }

    if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && (cmd[3] == ' ' || cmd[3] == 0)) {
        term_cmd_cat(cmd + 3);
        return;
    }

    if (!my_strcmp(cmd, "time")) {
        term_print("Uptime: ");
        term_dec(ticks / 100);
        term_print("s | Clock: ");
        char rtc[10];
        get_rtc_time(rtc);
        term_print(rtc);
        term_putchar('\n');
        return;
    }

    if (!my_strcmp(cmd, "meminfo")) {
        u32 cnt = *(u32*)0x71C;
        if (cnt > 32) cnt = 32;
        term_dec(cnt);
        term_print(" E820 memory regions detected\n");
        return;
    }

    if (!my_strcmp(cmd, "sysinfo")) {
        char brand[50];
        get_cpu_brand(brand, 50);
        term_print("CPU: ");
        term_print(brand);
        term_print("\nOS: BrickOS 32-bit Protected Mode\n");
        return;
    }

    if (!my_strcmp(cmd, "vesainfo")) {
        const char* hx = "0123456789ABCDEF";
        u8  vesa_ok  = *(u8* )0x70F;
        u16 vesa_w   = *(u16*)0x708;
        u16 vesa_h   = *(u16*)0x70A;
        u16 vesa_pit = *(u16*)0x70C;
        u8  vesa_bpp = *(u8* )0x70E;
        u32 vesa_fb  = *(u32*)0x704;
        u16 vesa_mode= *(u16*)0x702;

        term_print("VESA Framebuffer Info:\n");
        term_print("  Active     : "); term_dec(vesa_ok); term_putchar('\n');
        term_print("  Mode       : 0x");
        term_putchar(hx[(vesa_mode >> 12) & 0xF]);
        term_putchar(hx[(vesa_mode >>  8) & 0xF]);
        term_putchar(hx[(vesa_mode >>  4) & 0xF]);
        term_putchar(hx[ vesa_mode        & 0xF]);
        term_putchar('\n');
        term_print("  Resolution : "); term_dec(vesa_w); term_print(" x "); term_dec(vesa_h); term_putchar('\n');
        term_print("  Depth      : "); term_dec(vesa_bpp); term_print(" bpp\n");
        term_print("  Pitch      : "); term_dec(vesa_pit); term_print(" bytes\n");
        term_print("  FB base    : 0x");
        for (int q = 28; q >= 0; q -= 4) term_putchar(hx[(vesa_fb >> q) & 0xF]);
        term_putchar('\n');
        return;
    }

    if (!my_strcmp(cmd, "reboot")) {
        outb(0x64, 0xFE);
        return;
    }

    if (!my_strcmp(cmd, "shutdown") || !my_strcmp(cmd, "poweroff")) {
        system_shutdown();
        return;
    }

    term_print("Unknown command: ");
    term_print(cmd);
    term_print("\nType 'help' for commands\n");
}

void draw_terminal(struct Window* w) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);
    gui_rect(px, py0, cw, ch, 0x0C0C0C);

    int c_adv = font_loaded ? 9 : 8;
    int max_vis_lines = ch / 16;
    if (max_vis_lines > TERM_LINES) max_vis_lines = TERM_LINES;

    for (int line = 0; line < max_vis_lines; line++) {
        int ly = py0 + line * 16;
        for (int col = 0; term_screen[line][col] && col < TERM_COLS; col++) {
            gui_char(term_screen[line][col], px + col * c_adv, ly, 0xCCCCCC, 0x0C0C0C);
        }
    }

    /* Active input line prompt */
    if (term_cy_p < max_vis_lines) {
        int pl = py0 + term_cy_p * 16;
        gui_print_bg("$ ", px, pl, 0x00FF00, 0x0C0C0C);
        int pw = gui_str_width("$ ");

        for (int i = 0; i < term_cmdlen && i < 60; i++) {
            gui_char(term_cmd[i], px + pw + i * c_adv, pl, 0xFFFFFF, 0x0C0C0C);
        }

        if (term_active && ((ticks / 40) & 1)) {
            gui_rect(px + pw + term_cmdlen * c_adv, pl + 1, 8, 14, 0x00FF00);
        }
    }
}

void terminal_key(char c) {
    if (!term_active) return;

    if (c == '\n') {
        term_cmd[term_cmdlen] = 0;
        /* Put the prompt and command into history */
        term_print("$ ");
        term_print(term_cmd);
        term_putchar('\n');

        char buf[64];
        my_strcpy(buf, term_cmd);
        term_cmdlen = 0;
        term_cmd[0] = 0;

        term_exec(buf);
    } else if (c == '\b') {
        if (term_cmdlen > 0) {
            term_cmdlen--;
            term_cmd[term_cmdlen] = 0;
        }
    } else if (c >= 32 && c <= 126) {
        if (term_cmdlen < 60) {
            term_cmd[term_cmdlen++] = c;
            term_cmd[term_cmdlen] = 0;
        }
    }
}
