#include "drv_display.h"

int ctrlpanel_tab = 0;
u32 desktop_color = 0x1F5C8B;
int pending_res = 0; /* 0 = 1280x960, 1 = 1024x768, 2 = 800x600, 3 = 640x480 */

static int ctrlpanel_switched = 0;    /* 1 = success, 2 = error */
static u32 ctrlpanel_switch_tick = 0; /* ticks when confirmation expires */

static const u32 desk_pal[] = {
    0x1F5C8B, 0x004080, 0x205020, 0x602020,
    0x402060, 0x104060, 0x505050, 0x204840
};
#define N_DESK_PAL 8

void ctrlpanel_init(void) {
    ctrlpanel_tab = 0;
    ctrlpanel_switched = 0;
    ctrlpanel_switch_tick = 0;
    if (fw >= 1200) pending_res = 0;
    else if (fw >= 1000) pending_res = 1;
    else if (fw >= 800) pending_res = 2;
    else pending_res = 3;
}

static int get_display_buttons_y(int py0) {
    return py0 + 125;
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
        gui_print("OS Version:", content_x, cy2, 0x000000); gui_print("BrickOS 1.0", lx, cy2, 0x000080); cy2 += 18;
        gui_print("Architecture:", content_x, cy2, 0x000000); gui_print("x86 32-bit", lx, cy2, 0x000080); cy2 += 18;
        gui_print("Vendor:", content_x, cy2, 0x000000); gui_print("FlatronTech", lx, cy2, 0x000080); cy2 += 18;
        gui_print("Build:", content_x, cy2, 0x000000); gui_print("202610181", lx, cy2, 0x000080); cy2 += 18;

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
        gui_print(drv_display_get_hw_name(), lx, cy2, 0x000080); cy2 += 18;

        gui_print("Refresh Rate:", content_x, cy2, 0x000000);
        gui_print("60 Hz", lx, cy2, 0x000080); cy2 += 22;

        gui_print("Select Resolution:", content_x, cy2, 0x000000); cy2 += 20;

        int by = get_display_buttons_y(py0);
        int sel1280 = (pending_res == 0);
        int sel1024 = (pending_res == 1);
        int sel800  = (pending_res == 2);
        int sel640  = (pending_res == 3);

        gui_btn(content_x,       by,      120, 24, "1280 x 960", sel1280);
        gui_btn(content_x + 130, by,      120, 24, "1024 x 768", sel1024);
        gui_btn(content_x,       by + 28, 120, 24, "800 x 600",  sel800);
        gui_btn(content_x + 130, by + 28, 120, 24, "640 x 480",  sel640);

        /* Live switch button — zero reboots */
        gui_btn(content_x + 40, by + 58, 170, 26, "Apply", 0);

        if (pending_res == 0) {
            gui_print("(1280x960 selected)", content_x + 45, by + 90, 0x008000);
        } else if (pending_res == 1) {
            gui_print("(1024x768 selected)", content_x + 45, by + 90, 0x008000);
        } else if (pending_res == 2) {
            gui_print("(800x600 selected)", content_x + 45, by + 90, 0x008000);
        } else {
            gui_print("(640x480 selected)", content_x + 45, by + 90, 0x008000);
        }

        /* Show feedback */
        if (ctrlpanel_switched == 1 && ticks < ctrlpanel_switch_tick) {
            gui_print("Resolution changed live!", content_x + 40, by + 108, 0x008000);
        } else if (ctrlpanel_switched == 2 && ticks < ctrlpanel_switch_tick) {
            gui_print("Switch not supported by VM!", content_x + 30, by + 108, 0x800000);
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

        /* 1280x960 selector */
        if (mx >= content_x && mx < content_x + 120 && my >= by && my < by + 24) {
            pending_res = 0;
            return;
        }
        /* 1024x768 selector */
        if (mx >= content_x + 130 && mx < content_x + 250 && my >= by && my < by + 24) {
            pending_res = 1;
            return;
        }
        /* 800x600 selector */
        if (mx >= content_x && mx < content_x + 120 && my >= by + 28 && my < by + 52) {
            pending_res = 2;
            return;
        }
        /* 640x480 selector */
        if (mx >= content_x + 130 && mx < content_x + 250 && my >= by + 28 && my < by + 52) {
            pending_res = 3;
            return;
        }
        /* Apply — LIVE mode switch, zero reboots */
        if (mx >= content_x + 40 && mx < content_x + 210 && my >= by + 58 && my < by + 84) {
            int tw = 800, th = 600;
            if (pending_res == 0) { tw = 1280; th = 960; }
            else if (pending_res == 1) { tw = 1024; th = 768; }
            else if (pending_res == 2) { tw = 800;  th = 600; }
            else if (pending_res == 3) { tw = 640;  th = 480; }

            int ok = drv_display_set_resolution(tw, th);
            if (ok) {
                ctrlpanel_switched = 1;
                ctrlpanel_switch_tick = ticks + 300;
            } else {
                ctrlpanel_switched = 2;
                ctrlpanel_switch_tick = ticks + 300;
            }
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
        if (mx >= content_x && mx < content_x + 120 && my >= by + 28 && my < by + 52) return 1;
        if (mx >= content_x + 130 && mx < content_x + 250 && my >= by + 28 && my < by + 52) return 1;
        if (mx >= content_x + 40 && mx < content_x + 210 && my >= by + 58 && my < by + 84) return 1;
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
