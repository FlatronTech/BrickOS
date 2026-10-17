#include "app_ctrlpanel.h"

int ctrlpanel_tab = 0;
u32 desktop_color = 0x1F5C8B;
int pending_res = 1; /* 1 = 800x600, 2 = 640x480 */

static int ctrlpanel_rebooting = 0;
static u32 ctrlpanel_reboot_tick = 0;

static const u32 desk_pal[] = {
    0x1F5C8B, 0x004080, 0x205020, 0x602020,
    0x402060, 0x104060, 0x505050, 0x204840
};
#define N_DESK_PAL 8

void ctrlpanel_init(void) {
    ctrlpanel_tab = 0;
    ctrlpanel_rebooting = 0;
    ctrlpanel_reboot_tick = 0;
    pending_res = (fw == 640) ? 2 : 1;
}

static int get_display_buttons_y(int py0) {
    /* Fixed base coordinate matching draw, click and hover */
    return py0 + 150;
}

void draw_ctrlpanel(struct Window* w) {
    int px = win_client_x(w), py0 = win_client_y(w), cw = win_client_w(w), ch = win_client_h(w);

    /* Background */
    gui_rect(px, py0, cw, ch, 0xC0C0C0);

    /* Tabs */
    const char* tabs[] = {"System", "Display", "Personalize"};
    int tx = px + 4;
    for (int i = 0; i < 3; i++) {
        int tw = gui_str_width(tabs[i]) + 20;
        u32 tbg = (ctrlpanel_tab == i) ? 0xE0E0E0 : 0xB0B0B0;
        gui_rect(tx, py0 + 2, tw, 20, tbg);
        gui_hline(tx, py0 + 2, tw, 0xFFFFFF);
        gui_vline(tx, py0 + 2, 20, 0xFFFFFF);
        gui_vline(tx + tw - 1, py0 + 2, 20, 0x808080);
        if (ctrlpanel_tab != i) {
            gui_hline(tx, py0 + 21, tw, 0x808080);
        }
        gui_print(tabs[i], tx + 10, py0 + 4, (ctrlpanel_tab == i) ? 0x000080 : 0x000000);
        tx += tw + 4;
    }

    /* Content Area Frame */
    int cy0 = py0 + 22;
    int c_w = cw - 8, c_h = ch - 26;
    gui_rect(px + 4, cy0, c_w, c_h, 0xFFFFFF);
    gui_hline(px + 4, cy0, c_w, 0x808080);
    gui_vline(px + 4, cy0, c_h, 0x808080);
    gui_hline(px + 4, cy0 + c_h - 1, c_w, 0xFFFFFF);
    gui_vline(px + 4 + c_w - 1, cy0, c_h, 0xFFFFFF);

    int content_x = px + 14;
    int cy2 = cy0 + 10;
    int lx = content_x + 130;

    if (ctrlpanel_tab == 0) {
        gui_print("BrickOS System Information", content_x, cy2, 0x000080); cy2 += 22;
        gui_print("OS Version:", content_x, cy2, 0x000000); gui_print("BrickOS 2.0", lx, cy2, 0x000080); cy2 += 18;
        gui_print("Architecture:", content_x, cy2, 0x000000); gui_print("x86 32-bit Protected Mode", lx, cy2, 0x000080); cy2 += 18;
        gui_print("Vendor:", content_x, cy2, 0x000000); gui_print("FlatronTech", lx, cy2, 0x000080); cy2 += 18;
        gui_print("Build:", content_x, cy2, 0x000000); gui_print("2026-10 Release", lx, cy2, 0x000080); cy2 += 18;

        char cpubrand[50];
        get_cpu_brand(cpubrand, 50);
        gui_print("CPU Model:", content_x, cy2, 0x000000); cy2 += 18;
        gui_print(cpubrand, content_x + 12, cy2, 0x000080); cy2 += 22;

        u32 cnt = *(u32*)0x71C;
        if (cnt > 32) cnt = 32;
        u32 tkb = 0;
        for (u32 i = 0; i < cnt; i++) {
            u8* e = (u8*)(0x720 + i * 20);
            u32 ty = e[16] | (e[17] << 8) | (e[18] << 16) | (e[19] << 24);
            if (ty == 1) {
                u32 ll = e[8] | (e[9] << 8) | (e[10] << 16) | (e[11] << 24);
                tkb += ll / 1024;
            }
        }
        char mb[12];
        itoa(mb, tkb / 1024);
        gui_print("RAM Memory:", content_x, cy2, 0x000000);
        gui_print(mb, lx, cy2, 0x000080);
        gui_print(" MB", lx + gui_str_width(mb), cy2, 0x000080); cy2 += 18;

        char rtc[10];
        get_rtc_time(rtc);
        gui_print("System Time:", content_x, cy2, 0x000000);
        gui_print(rtc, lx, cy2, 0x000080);
    } else if (ctrlpanel_tab == 1) {
        gui_print("Display Settings", content_x, cy2, 0x000080); cy2 += 22;

        char resb[24];
        itoa(resb, (u32)fw);
        int ri = my_strlen(resb);
        resb[ri++] = ' '; resb[ri++] = 'x'; resb[ri++] = ' ';
        itoa(resb + ri, (u32)fh);
        gui_print("Current Mode:", content_x, cy2, 0x000000);
        gui_print(resb, lx, cy2, 0x000080); cy2 += 18;

        gui_print("Video Driver:", content_x, cy2, 0x000000);
        gui_print("VESA Linear Framebuffer", lx, cy2, 0x000080); cy2 += 18;

        gui_print("Refresh Rate:", content_x, cy2, 0x000000);
        gui_print("60 Hz", lx, cy2, 0x000080); cy2 += 22;

        gui_print("Select New Resolution:", content_x, cy2, 0x000000); cy2 += 20;

        int by = get_display_buttons_y(py0);
        int sel800 = (pending_res == 1);
        int sel640 = (pending_res == 2);

        if (ctrlpanel_rebooting) {
            gui_rect(content_x, by, 250, 70, 0xC0C0C0);
            gui_print("Restarting system...", content_x + 10, by + 10, 0x800000);
            gui_print("Please wait.", content_x + 10, by + 30, 0x000000);
            /* Reboot when timer fires */
            if (ticks >= ctrlpanel_reboot_tick) {
                outb(0x64, 0xFE);
            }
        } else {
            gui_btn(content_x, by, 120, 24, "800 x 600", sel800);
            gui_btn(content_x + 130, by, 120, 24, "640 x 480", sel640);
            gui_btn(content_x + 40, by + 34, 170, 26, "Apply & Reboot", 0);
            if (pending_res == 1) {
                gui_print("(Selected: 800x600)", content_x + 45, by + 68, 0x008000);
            } else {
                gui_print("(Selected: 640x480)", content_x + 45, by + 68, 0x008000);
            }
        }
    } else {
        gui_print("Desktop Appearance", content_x, cy2, 0x000080); cy2 += 22;
        gui_print("Select Desktop Color:", content_x, cy2, 0x000000); cy2 += 20;

        for (int i = 0; i < N_DESK_PAL; i++) {
            int sw = 42, sh = 28;
            int bx = content_x + (i % 4) * (sw + 12);
            int by2 = cy2 + (i / 4) * (sh + 12);

            gui_rect(bx, by2, sw, sh, desk_pal[i]);
            gui_hline(bx, by2, sw, 0x808080); gui_vline(bx, by2, sh, 0x808080);
            gui_hline(bx, by2 + sh - 1, sw, 0x808080); gui_vline(bx + sw - 1, by2, sh, 0x808080);

            if (desk_pal[i] == desktop_color) {
                gui_hline(bx - 2, by2 - 2, sw + 4, 0x000080);
                gui_hline(bx - 2, by2 + sh + 1, sw + 4, 0x000080);
                gui_vline(bx - 2, by2 - 2, sh + 4, 0x000080);
                gui_vline(bx + sw + 1, by2 - 2, sh + 4, 0x000080);
            }
        }
        cy2 += 86;
        gui_print("Window Style: Classic Win98 3D", content_x, cy2, 0x000000); cy2 += 18;
        gui_print("System Font: VCR OSD Mono / BIOS 8x16", content_x, cy2, 0x000000);
    }
}

void ctrlpanel_click(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w);

    /* Tabs click */
    const char* tabs[] = {"System", "Display", "Personalize"};
    int tx = px + 4;
    for (int i = 0; i < 3; i++) {
        int tw = gui_str_width(tabs[i]) + 20;
        if (mx >= tx && mx < tx + tw && my >= py0 + 2 && my < py0 + 22) {
            ctrlpanel_tab = i;
            return;
        }
        tx += tw + 4;
    }

    int content_x = px + 14;

    if (ctrlpanel_tab == 1) {
        int by = get_display_buttons_y(py0);
        /* 800x600 button */
        if (mx >= content_x && mx < content_x + 120 && my >= by && my < by + 24) {
            pending_res = 1;
            outb(0x70, 0x38);
            outb(0x71, 1);
            return;
        }
        /* 640x480 button */
        if (mx >= content_x + 130 && mx < content_x + 250 && my >= by && my < by + 24) {
            pending_res = 2;
            outb(0x70, 0x38);
            outb(0x71, 2);
            return;
        }
        /* Apply & Reboot */
        if (mx >= content_x + 40 && mx < content_x + 210 && my >= by + 34 && my < by + 60) {
            /* Save resolution to CMOS, show countdown, then reboot */
            outb(0x70, 0x38);
            outb(0x71, (u8)pending_res);
            ctrlpanel_rebooting = 1;
            ctrlpanel_reboot_tick = ticks + 150; /* ~1.5 sec */
            return;
        }
    } else if (ctrlpanel_tab == 2) {
        int cy0 = py0 + 22;
        int cy2 = cy0 + 10 + 22 + 20;
        for (int i = 0; i < N_DESK_PAL; i++) {
            int sw = 42, sh = 28;
            int bx = content_x + (i % 4) * (sw + 12);
            int by2 = cy2 + (i / 4) * (sh + 12);
            if (mx >= bx && mx < bx + sw && my >= by2 && my < by2 + sh) {
                desktop_color = desk_pal[i];
                return;
            }
        }
    }
}

int ctrlpanel_is_hover_link(struct Window* w, int mx, int my) {
    int px = win_client_x(w), py0 = win_client_y(w);

    /* Tabs hover */
    const char* tabs[] = {"System", "Display", "Personalize"};
    int tx = px + 4;
    for (int i = 0; i < 3; i++) {
        int tw = gui_str_width(tabs[i]) + 20;
        if (mx >= tx && mx < tx + tw && my >= py0 + 2 && my < py0 + 22) return 1;
        tx += tw + 4;
    }

    int content_x = px + 14;

    if (ctrlpanel_tab == 1) {
        int by = get_display_buttons_y(py0);
        if (mx >= content_x && mx < content_x + 120 && my >= by && my < by + 24) return 1;
        if (mx >= content_x + 130 && mx < content_x + 250 && my >= by && my < by + 24) return 1;
        if (mx >= content_x + 40 && mx < content_x + 210 && my >= by + 34 && my < by + 60) return 1;
    } else if (ctrlpanel_tab == 2) {
        int cy0 = py0 + 22;
        int cy2 = cy0 + 10 + 22 + 20;
        for (int i = 0; i < N_DESK_PAL; i++) {
            int sw = 42, sh = 28;
            int bx = content_x + (i % 4) * (sw + 12);
            int by2 = cy2 + (i / 4) * (sh + 12);
            if (mx >= bx && mx < bx + sw && my >= by2 && my < by2 + sh) return 1;
        }
    }
    return 0;
}
