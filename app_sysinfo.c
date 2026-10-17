#include "app_sysinfo.h"

void draw_sysinfo(struct Window* w) {
    int px = win_client_x(w), py = win_client_y(w);

    gui_print("BrickOS 32-bit", px, py, 0x000080); py += 20;
    gui_print("OS Version: BrickOS 2.0", px, py, 0x000000); py += 16;
    gui_print("Vendor:     FlatronTech", px, py, 0x000000); py += 16;

    char cpubrand[50];
    get_cpu_brand(cpubrand, 50);
    gui_print("CPU Model:", px, py, 0x000000); py += 16;
    gui_print(cpubrand, px + 12, py, 0x000080); py += 18;

    char resb[24];
    itoa(resb, (u32)fw);
    int ri = my_strlen(resb);
    resb[ri++] = ' '; resb[ri++] = 'x'; resb[ri++] = ' ';
    itoa(resb + ri, (u32)fh);
    int ri2 = my_strlen(resb);
    resb[ri2++] = ' '; resb[ri2++] = 'V'; resb[ri2++] = 'E'; resb[ri2++] = 'S'; resb[ri2++] = 'A';
    resb[ri2] = 0;
    int lx = px + 120;
    gui_print("Resolution:", px, py, 0x000000);
    gui_print(resb, lx, py, 0x000080); py += 16;

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
    gui_print("RAM Memory:", px, py, 0x000000);
    gui_print(mb, lx, py, 0x000080);
    gui_print(" MB", lx + gui_str_width(mb), py, 0x000080); py += 16;

    gui_print("Drivers:", px, py, 0x000000);
    gui_print("INF loaded", lx, py, 0x000080); py += 16;
    gui_print("Input:", px, py, 0x000000);
    gui_print("PS/2 Mouse & Keyboard", lx, py, 0x000080); py += 18;
    gui_print("System is operational!", px, py, 0x008000);
}
