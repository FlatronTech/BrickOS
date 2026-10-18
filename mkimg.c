#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char img[2880*512];
unsigned char* fat = img + 1*512;
unsigned char* root = img + 19*512;
unsigned char* data = img + 33*512;
int next_cluster = 2;

void fat_set(int e, int v) {
    int i = e + e/2;
    int w = fat[i] | (fat[i+1] << 8);
    if (e & 1) w = (w & 0x000F) | (v << 4);
    else w = (w & 0xF000) | v;
    fat[i] = w & 0xFF; fat[i+1] = w >> 8;
}

void make_name(const char* in, char* out) {
    int i;
    for (i = 0; i < 11; i++) out[i] = ' ';
    for (i = 0; in[i] && in[i] != '.' && i < 8; i++) out[i] = in[i];
    const char* d = strchr(in, '.');
    if (d) for (i = 0; d[1+i] && i < 3; i++) out[8+i] = d[1+i];
}

int add_file(const char* name, const unsigned char* buf, int size) {
    int slot = -1, i;
    for (i = 0; i < 224; i++) if (root[i*32] == 0 || (unsigned char)root[i*32] == 0xE5) { slot = i; break; }
    if (slot < 0) return -1;
    unsigned char* e = root + slot*32;
    make_name(name, (char*)e);
    e[11] = 0x20;
    int clusters = (size + 511) / 512;
    if (clusters == 0) clusters = 1;
    int first = next_cluster;
    for (i = 0; i < clusters; i++) {
        int n = size - i*512;
        if (n > 512) n = 512;
        if (n > 0) memcpy(data + (next_cluster-2)*512, buf + i*512, n);
        fat_set(next_cluster, (i == clusters-1) ? 0xFFF : next_cluster+1);
        next_cluster++;
    }
    e[26] = first & 0xFF; e[27] = first >> 8;
    e[28] = size & 0xFF; e[29] = (size>>8)&0xFF; e[30] = (size>>16)&0xFF; e[31] = (size>>24)&0xFF;
    return first;
}

unsigned char* load(const char* p, int* sz) {
    FILE* f = fopen(p, "rb");
    if (!f) { printf("no %s\n", p); exit(1); }
    fseek(f, 0, SEEK_END); *sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* b = malloc(*sz);
    fread(b, 1, *sz, f); fclose(f);
    return b;
}

int main(int argc, char** argv) {
    int bs, ks;
    unsigned char* boot = load(argv[1], &bs);
    unsigned char* kern = load(argv[2], &ks);
    if (bs > 512) { printf("boot too big\n"); return 1; }
    if (ks > 512*512) { printf("kernel too big\n"); return 1; }
    memcpy(img, boot, bs);
    fat_set(0, 0xFF0); fat_set(1, 0xFFF);
    add_file("BRICKOS.BIN", kern, ks);
    const char* readme =
        "Commands: help ls cat meminfo sysinfo time clear about reboot\r\n";
    add_file("README.TXT", (unsigned char*)readme, strlen(readme));
    const char* hello = "Hello from BrickOS!\r\n";
    add_file("HELLO.TXT", (unsigned char*)hello, strlen(hello));
    int inf_sz;
    unsigned char* inf_mouse = load("mouse.inf", &inf_sz);
    add_file("MOUSE.INF", inf_mouse, inf_sz);
    unsigned char* inf_kbd = load("keyboard.inf", &inf_sz);
    add_file("KEYBOARD.INF", inf_kbd, inf_sz);
    unsigned char* inf_mon = load("monitor.inf", &inf_sz);
    add_file("MONITOR.INF", inf_mon, inf_sz);
    unsigned char* inf_disp = load("display.inf", &inf_sz);
    add_file("DISPLAY.INF", inf_disp, inf_sz);
    int font_sz;
    unsigned char* font_data = load("FONT.BIN", &font_sz);
    add_file("FONT.BIN", font_data, font_sz);
    FILE* fbmp = fopen("BRICK.BMP", "rb");
    if (fbmp) {
        fclose(fbmp);
        int bmp_sz;
        unsigned char* bmp_d = load("BRICK.BMP", &bmp_sz);
        add_file("BRICK.BMP", bmp_d, bmp_sz);
    }

    FILE* f = fopen(argv[3], "wb");
    fwrite(img, 1, sizeof(img), f); fclose(f);
    printf("%s created (FAT12, 1.44MB)\n", argv[3]);
    return 0;
}