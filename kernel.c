#include "kernel.h"
#include "app_paint.h"
#include "app_notepad.h"
#include "app_terminal.h"
#include "app_ctrlpanel.h"
#include "app_calc.h"
#include "app_explorer.h"
#include "app_taskmgr.h"
#include "app_sysinfo.h"
#include "app_bmpview.h"
#include "app_saveas.h"
#include "app_welcome.h"
#include "app_fileprops.h"
#include "drv_display.h"
#include "vesa_trampoline.h"

#define VGA_MEMORY 0xB8000

/* String & memory utilities */
int my_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int my_strlen(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

void my_memcpy(void* d, const void* s, int n) {
    u8* dd = (u8*)d;
    const u8* ss = (const u8*)s;
    while (n--) *dd++ = *ss++;
}

void my_memset(void* d, u8 v, int n) {
    u8* dd = (u8*)d;
    while (n--) *dd++ = v;
}

void my_strcpy(char* d, const char* s) {
    while ((*d++ = *s++));
}

void itoa(char* buf, u32 v) {
    if (!v) { buf[0] = '0'; buf[1] = 0; return; }
    char t[12];
    int i = 0;
    while (v) { t[i++] = '0' + (v % 10); v /= 10; }
    int j = 0;
    for (int k = i - 1; k >= 0; k--) buf[j++] = t[k];
    buf[j] = 0;
}

void itoa_signed(char* buf, int v) {
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return; }
    int j = 0;
    if (v < 0) {
        buf[j++] = '-';
        v = -v;
    }
    char t[12];
    int i = 0;
    u32 uv = (u32)v;
    while (uv) { t[i++] = '0' + (uv % 10); uv /= 10; }
    for (int k = i - 1; k >= 0; k--) buf[j++] = t[k];
    buf[j] = 0;
}

void my_strcat(char* d, const char* s) {
    while (*d) d++;
    while ((*d++ = *s++));
}

void format_size(char* buf, u32 sz) {
    if (sz >= 1048576) {
        char t[12]; itoa(t, sz / 1048576);
        my_strcpy(buf, t); my_strcat(buf, " MB");
    } else if (sz >= 1024) {
        char t[12]; itoa(t, sz / 1024);
        my_strcpy(buf, t); my_strcat(buf, " KB");
    } else {
        char t[12]; itoa(t, sz);
        my_strcpy(buf, t); my_strcat(buf, " B");
    }
}

/* System font */
struct LoadedFont* sys_font = (struct LoadedFont*)0x50000;
int font_loaded = 0;

/* BSS zero + entry */
extern u32 __bss_start, __bss_end;
void kmain(void);
void early_idt(void);

void __attribute__((section(".text.start"), noreturn)) _start(void) {
    u32* p = &__bss_start;
    while (p < &__bss_end) *p++ = 0;
    early_idt();
    kmain();
    for (;;) __asm__ volatile("hlt");
}

/* Global framebuffer info */
u32 bi_fb;
u16 bi_pitch;
u8  bi_bpp, bi_vesa, bi_font_dummy;
u32 bi_font;
int fw = 800, fh = 600;
u8* ramdisk = (u8*)0x800000;

/* IDT */
struct idt_e { u16 lo; u16 sel; u8 zero; u8 type; u16 hi; } __attribute__((packed));
struct idt_e idt[48];
struct { u16 lim; u32 base; } __attribute__((packed)) idtr;

volatile u32 ticks = 0;
#define KBUF 256
volatile char kbuf[KBUF];
volatile int khead = 0, ktail = 0;
volatile int esc_pressed = 0;

void exc_handler(int v) {
    __asm__ volatile("cli");
    u8* m = (u8*)VGA_MEMORY;
    const char* h = "0123456789ABCDEF";
    m[0] = 'E'; m[1] = 0x4F; m[2] = 'X'; m[3] = 0x4F;
    m[4] = 'C'; m[5] = 0x4F; m[6] = ' '; m[7] = 0x4F;
    m[8] = h[(v >> 4) & 15]; m[9] = 0x4F;
    m[10] = h[v & 15]; m[11] = 0x4F;
    for (;;) __asm__ volatile("hlt");
}

#define EXC(n) void __attribute__((naked)) exc##n(void){__asm__ volatile("pushl $" #n "\n movl %0,%%eax\n call *%%eax\n"::"i"(exc_handler));}
EXC(0)EXC(1)EXC(2)EXC(3)EXC(4)EXC(5)EXC(6)EXC(7)
EXC(8)EXC(9)EXC(10)EXC(11)EXC(12)EXC(13)EXC(14)EXC(15)
EXC(16)EXC(17)EXC(18)EXC(19)EXC(20)EXC(21)EXC(22)EXC(23)
EXC(24)EXC(25)EXC(26)EXC(27)EXC(28)EXC(29)EXC(30)EXC(31)

static u32 exctab[32] = {
    (u32)(unsigned long)exc0,  (u32)(unsigned long)exc1,  (u32)(unsigned long)exc2,  (u32)(unsigned long)exc3,
    (u32)(unsigned long)exc4,  (u32)(unsigned long)exc5,  (u32)(unsigned long)exc6,  (u32)(unsigned long)exc7,
    (u32)(unsigned long)exc8,  (u32)(unsigned long)exc9,  (u32)(unsigned long)exc10, (u32)(unsigned long)exc11,
    (u32)(unsigned long)exc12, (u32)(unsigned long)exc13, (u32)(unsigned long)exc14, (u32)(unsigned long)exc15,
    (u32)(unsigned long)exc16, (u32)(unsigned long)exc17, (u32)(unsigned long)exc18, (u32)(unsigned long)exc19,
    (u32)(unsigned long)exc20, (u32)(unsigned long)exc21, (u32)(unsigned long)exc22, (u32)(unsigned long)exc23,
    (u32)(unsigned long)exc24, (u32)(unsigned long)exc25, (u32)(unsigned long)exc26, (u32)(unsigned long)exc27,
    (u32)(unsigned long)exc28, (u32)(unsigned long)exc29, (u32)(unsigned long)exc30, (u32)(unsigned long)exc31
};

void idt_set(int n, u32 addr) {
    idt[n].lo = addr & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].zero = 0;
    idt[n].type = 0x8E;
    idt[n].hi = addr >> 16;
}

void __attribute__((naked)) irq_ignore(void) {
    __asm__ volatile("push %eax\n mov $0x20,%al\n out %al,$0x20\n out %al,$0xA0\n pop %eax\n iret\n");
}

void early_idt(void) {
    for (int i = 0; i < 48; i++) {
        idt_set(i, (i < 32) ? exctab[i] : (u32)(unsigned long)irq_ignore);
    }
    idtr.lim = sizeof(idt) - 1;
    idtr.base = (u32)(unsigned long)idt;
    __asm__ volatile("lidt %0" :: "m"(idtr));
}

/* Keyboard driver */
void tick_handler(void) { ticks++; }

static int shift_held = 0;

static char sc2asc(int sc) {
    static const char lo[] = "1234567890-=";
    static const char hi[] = "!@#$%^&*()_+";

    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return 0; }
    if (sc == 0x1C) return '\n';
    if (sc == 0x0E) return '\b';
    if (sc == 0x39) return ' ';
    if (sc == 0x0F) return '\t';
    if (sc == 0x34) return shift_held ? '>' : '.';
    if (sc == 0x33) return shift_held ? '<' : ',';
    if (sc == 0x35) return shift_held ? '?' : '/';
    if (sc == 0x27) return shift_held ? ':' : ';';
    if (sc == 0x28) return shift_held ? '"' : '\'';

    if (sc >= 2 && sc <= 0x0D) return shift_held ? hi[sc - 2] : lo[sc - 2];
    if (sc >= 0x10 && sc <= 0x19) { char c = "qwertyuiop"[sc - 0x10]; return shift_held ? c - 32 : c; }
    if (sc >= 0x1E && sc <= 0x26) { char c = "asdfghjkl"[sc - 0x1E];  return shift_held ? c - 32 : c; }
    if (sc >= 0x2C && sc <= 0x32) { char c = "zxcvbnm"[sc - 0x2C];    return shift_held ? c - 32 : c; }

    /* Numpad math keys */
    if (sc == 0x4A) return '-';
    if (sc == 0x4E) return '+';
    if (sc == 0x37) return '*';

    /* Navigation keys */
    if (sc == 0x48) return KEY_UP;
    if (sc == 0x50) return KEY_DOWN;
    if (sc == 0x4B) return KEY_LEFT;
    if (sc == 0x4D) return KEY_RIGHT;
    if (sc == 0x47) return KEY_HOME;
    if (sc == 0x4F) return KEY_END;

    return 0;
}

void kbd_handler(void) {
    u8 sc = inb(0x60);
    if (sc == 0xE0) return; /* ignore extended scan code prefix */
    if (sc & 0x80) {
        int s = sc & 0x7F;
        if (s == 0x2A || s == 0x36) shift_held = 0;
        return;
    }
    char c = sc2asc(sc);
    if (c && ((khead + 1) & (KBUF - 1)) != ktail) {
        kbuf[khead] = c;
        khead = (khead + 1) & (KBUF - 1);
    }
    if (sc == 1) esc_pressed = 1;
}

void __attribute__((naked)) irq0_stub(void) {
    __asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%eax\n out %%al,$0x20\n popa\n iret\n" :: "i"(tick_handler));
}

void __attribute__((naked)) irq1_stub(void) {
    __asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%eax\n out %%al,$0x20\n popa\n iret\n" :: "i"(kbd_handler));
}

/* Mouse driver */
volatile int mouse_x = 320, mouse_y = 240, mouse_btn = 0;
static int mouse_cycle = 0;
static u8 mouse_bytes[3];
static u32 mouse_last_tick = 0;

void mouse_handler(void) {
    u8 status = inb(0x64);
    while (status & 1) {
        u8 data = inb(0x60);
        if (status & 0x20) {
            if (mouse_cycle != 0 && (ticks - mouse_last_tick > 5)) {
                mouse_cycle = 0;
            }
            mouse_last_tick = ticks;
            if (mouse_cycle == 0) {
                if ((data & 0xC8) == 0x08) {
                    mouse_bytes[0] = data;
                    mouse_cycle = 1;
                }
            } else if (mouse_cycle == 1) {
                mouse_bytes[1] = data;
                mouse_cycle = 2;
            } else {
                mouse_bytes[2] = data;
                mouse_cycle = 0;
                int rx = (int)mouse_bytes[1];
                int ry = (int)mouse_bytes[2];
                if (mouse_bytes[0] & 0x10) rx -= 256;
                if (mouse_bytes[0] & 0x20) ry -= 256;
                if (rx >= -100 && rx <= 100 && ry >= -100 && ry <= 100) {
                    mouse_x += rx;
                    mouse_y -= ry;
                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_x >= fw) mouse_x = fw - 1;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_y >= fh) mouse_y = fh - 1;
                    mouse_btn = mouse_bytes[0] & 0x07;
                }
            }
        }
        status = inb(0x64);
    }
}

void __attribute__((naked)) irq12_stub(void) {
    __asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%al\n out %%al,$0xA0\n out %%al,$0x20\n popa\n iret\n" :: "i"(mouse_handler));
}

void mouse_wait(u8 t) {
    u32 to = 100000;
    if (t == 0) {
        while (to--) { if (inb(0x64) & 1) return; }
    } else {
        while (to--) { if (!(inb(0x64) & 2)) return; }
    }
}

void mouse_write(u8 v) {
    mouse_wait(1); outb(0x64, 0xD4);
    mouse_wait(1); outb(0x60, v);
}

u8 mouse_read(void) {
    u32 to = 100000;
    while (to--) {
        u8 st = inb(0x64);
        if ((st & 0x21) == 0x21) return inb(0x60);
    }
    return 0;
}

void mouse_init(void) {
    u32 to = 10000; while (to--) { if (inb(0x64) & 1) inb(0x60); }
    mouse_wait(1); outb(0x64, 0xA8);
    mouse_wait(1); outb(0x64, 0x20); mouse_wait(0); u8 s = inb(0x60);
    s |= 2; s &= ~0x20; s |= 1;
    mouse_wait(1); outb(0x64, 0x60); mouse_wait(1); outb(0x60, s);
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();
    to = 10000; while (to--) { if (inb(0x64) & 1) inb(0x60); }
    mouse_cycle = 0; mouse_btn = 0;
    mouse_x = fw / 2; mouse_y = fh / 2;
}

/* Interrupts init */
void interrupts_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);

    idt_set(32, (u32)(unsigned long)irq0_stub);
    idt_set(33, (u32)(unsigned long)irq1_stub);
    idt_set(44, (u32)(unsigned long)irq12_stub);

    outb(0x43, 0x36);
    outb(0x40, 11932 & 0xFF);
    outb(0x40, 11932 >> 8);

    mouse_init();
    outb(0x21, ~((1 << 0) | (1 << 1) | (1 << 2)));
    outb(0xA1, ~(1 << 4));

    __asm__ volatile("sti");
}

/* Console (fallback text mode) */
u8* fb;
int fpitch, fbpp;
int cols = 80, rows = 25, cx = 0, cy = 0, lastcx = -1, lastcy = -1;
u8* tvga = (u8*)VGA_MEMORY;
u32 COL_BG, COL_FG, COL_BAR;
char shadow[160 * 80];
int use_fb = 0;

void con_char(char c);
void con_print(const char* s);
void con_dec(u32 v);

void fb_pixel(int x, int y, u32 c) {
    u8* p = fb + y * fpitch + x * (fbpp / 8);
    p[0] = c & 0xFF; p[1] = (c >> 8) & 0xFF; p[2] = (c >> 16) & 0xFF;
    if (fbpp == 32) p[3] = 0;
}

void fb_rect(int x, int y, int w, int h, u32 c) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) fb_pixel(x + i, y + j, c);
    }
}

void fb_glyph(char ch, int gx, int gy, u32 fg) {
    u8 uc = (u8)ch;
    if (font_loaded) {
        int w = sys_font->width[uc]; if (w == 0) w = 8;
        int h = sys_font->height;
        for (int r = 0; r < h && r < 16; r++) {
            u16 b = sys_font->bitmap[uc][r];
            for (int cbit = 0; cbit < 8 && cbit < w; cbit++) {
                if (b & (0x8000 >> cbit)) fb_pixel(gx * 8 + cbit, gy * 16 + r, fg);
            }
        }
    } else if (bi_font) {
        const u8* f = (const u8*)(unsigned long)bi_font + uc * 16;
        for (int r = 0; r < 16; r++) {
            u8 b = f[r];
            for (int cbit = 0; cbit < 8; cbit++) {
                if (b & (0x80 >> cbit)) fb_pixel(gx * 8 + cbit, gy * 16 + r, fg);
            }
        }
    }
}

void fb_putcell(int x, int r, char ch) {
    fb_rect(x * 8, (r + 1) * 16, 8, 16, COL_BG);
    if (ch != ' ') fb_glyph(ch, x, r + 1, COL_FG);
}

void hw_cursor(void) {
    int pos = cy * cols + cx;
    outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
}

void fb_cursor(void) {
    if (lastcx >= 0) fb_putcell(lastcx, lastcy, shadow[lastcy * cols + lastcx]);
    fb_rect(cx * 8, (cy + 1) * 16 + 13, 8, 3, COL_FG);
    lastcx = cx; lastcy = cy;
}

void con_cursor(void) {
    if (use_fb) fb_cursor(); else hw_cursor();
}

void render_all(void) {
    if (use_fb) {
        fb_rect(0, 16, fw, fh - 16, COL_BG);
        for (int r = 0; r < rows; r++) {
            for (int x = 0; x < cols; x++) {
                if (shadow[r * cols + x] != ' ') fb_putcell(x, r, shadow[r * cols + x]);
            }
        }
    } else {
        for (int i = 0; i < cols * rows; i++) {
            tvga[i * 2] = shadow[i];
            tvga[i * 2 + 1] = 0x0F;
        }
    }
}

void con_scroll(void) {
    for (int i = 0; i < cols * (rows - 1); i++) shadow[i] = shadow[i + cols];
    for (int i = cols * (rows - 1); i < cols * rows; i++) shadow[i] = ' ';
    cy = rows - 1;
    render_all();
}

void con_clear(void) {
    for (int i = 0; i < cols * rows; i++) shadow[i] = ' ';
    cx = 0; cy = 0; lastcx = -1; lastcy = -1;
    if (use_fb) {
        fb_rect(0, 0, fw, fh, COL_BG);
        fb_rect(0, 0, fw, 16, COL_BAR);
        const char* t = "BrickOS 1.0";
        for (int i = 0; t[i]; i++) fb_glyph(t[i], 2 + i, 0, 0x000000);
    }
    render_all();
    con_cursor();
}

void con_char(char c) {
    if (c == '\n') {
        cx = 0; cy++;
    } else if (c == '\b') {
        if (cx > 0) {
            cx--;
            shadow[cy * cols + cx] = ' ';
            if (use_fb) fb_putcell(cx, cy, ' ');
            else tvga[(cy * cols + cx) * 2] = ' ';
        }
    } else {
        shadow[cy * cols + cx] = c;
        if (use_fb) fb_putcell(cx, cy, c);
        else {
            tvga[(cy * cols + cx) * 2] = c;
            tvga[(cy * cols + cx) * 2 + 1] = 0x0F;
        }
        cx++;
    }
    if (cx >= cols) { cx = 0; cy++; }
    if (cy >= rows) con_scroll();
    con_cursor();
}

void con_print(const char* s) { while (*s) con_char(*s++); }
void con_dec(u32 v) { char b[12]; itoa(b, v); con_print(b); }
void con_hex(u32 v) {
    const char* h = "0123456789ABCDEF";
    for (int q = 28; q >= 0; q -= 4) con_char(h[(v >> q) & 15]);
}
void con_hex16(u16 v) {
    const char* h = "0123456789ABCDEF";
    for (int q = 12; q >= 0; q -= 4) con_char(h[(v >> q) & 15]);
}

char kbd_get(void) {
    while (khead == ktail) __asm__ volatile("hlt");
    char c = kbuf[ktail];
    ktail = (ktail + 1) & (KBUF - 1);
    return c;
}

char kbd_try(void) {
    if (khead == ktail) return 0;
    char c = kbuf[ktail];
    ktail = (ktail + 1) & (KBUF - 1);
    return c;
}

/* FAT12 & FAT16 Driver */
static u16 rd16(int o) { return ramdisk[o] | (ramdisk[o + 1] << 8); }
int fat_sect, root_sect, data_sect, root_cnt, total_sectors = 2880, is_fat16 = 0;
u8* fatp;

void fat_init(void) {
    int res = rd16(14);
    int nf = ramdisk[16];
    root_cnt = rd16(17);
    fat_sect = res;
    int spf = rd16(22);
    if (spf == 0) spf = rd16(36);
    root_sect = res + nf * spf;
    data_sect = root_sect + (root_cnt * 32 + 511) / 512;
    fatp = ramdisk + fat_sect * 512;
    total_sectors = rd16(19);
    if (!total_sectors) total_sectors = *(u32*)&ramdisk[32];
    if (!total_sectors) total_sectors = 2880;
    if (ramdisk[54] == 'F' && ramdisk[57] == '1' && ramdisk[58] == '6') is_fat16 = 1;
    else is_fat16 = 0;
}

int fat_get(int e) {
    if (is_fat16) return ((u16*)fatp)[e];
    int i = e + e / 2;
    int w = fatp[i] | (fatp[i + 1] << 8);
    return (e & 1) ? (w >> 4) : (w & 0xFFF);
}

void fat_name83(const char* in, char* out) {
    while (*in == ' ') in++;
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int i;
    for (i = 0; in[i] && in[i] != '.' && i < 8; i++) {
        out[i] = (in[i] >= 'a' && in[i] <= 'z') ? (in[i] - 32) : in[i];
    }
    const char* d = in;
    while (*d && *d != '.') d++;
    if (*d == '.') {
        for (i = 0; d[1 + i] && i < 3; i++) {
            out[8 + i] = (d[1 + i] >= 'a' && d[1 + i] <= 'z') ? (d[1 + i] - 32) : d[1 + i];
        }
    }
}

int fat_read_file(const char* name, u8* dest, int max_sz) {
    char n83[11];
    fat_name83(name, n83);
    u8* root = ramdisk + root_sect * 512;
    int end_mark = is_fat16 ? 0xFFF8 : 0xFF8;

    for (int i = 0; i < root_cnt; i++) {
        u8* e = root + i * 32;
        if (e[0] == 0) break;
        int ok = 1;
        for (int j = 0; j < 11; j++) {
            if (e[j] != n83[j]) ok = 0;
        }
        if (!ok) continue;

        int sz = e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24);
        int cl = e[26] | (e[27] << 8);
        int total = 0;

        while (cl < end_mark && total < sz && total < max_sz) {
            int s = data_sect + cl - 2;
            if (s >= total_sectors) break;
            u8* p = ramdisk + s * 512;
            int n = sz - total;
            if (n > 512) n = 512;
            if (total + n > max_sz) n = max_sz - total;
            for (int k = 0; k < n; k++) dest[total + k] = p[k];
            total += n;
            cl = fat_get(cl);
        }
        return total;
    }
    return -1;
}

static void fat_set_entry(int e, int val) {
    int spf = rd16(22);
    if (spf == 0) spf = rd16(36);
    int nf = ramdisk[16];
    if (nf < 1) nf = 1;

    for (int f = 0; f < nf; f++) {
        u8* fp = ramdisk + (fat_sect + f * spf) * 512;
        if (is_fat16) {
            ((u16*)fp)[e] = (u16)val;
        } else {
            int i = e + e / 2;
            int w = fp[i] | (fp[i + 1] << 8);
            if (e & 1) w = (w & 0x000F) | (val << 4);
            else w = (w & 0xF000) | (val & 0xFFF);
            fp[i] = w & 0xFF;
            fp[i + 1] = (w >> 8) & 0xFF;
        }
    }
}

static int fat_alloc_cluster(void) {
    int max_cl = is_fat16 ? 65000 : (total_sectors - data_sect + 2);
    for (int i = 2; i < max_cl; i++) {
        if (fat_get(i) == 0) return i;
    }
    return -1;
}

int fat_write_file(const char* name, const u8* data, int sz) {
    char n83[11];
    fat_name83(name, n83);
    u8* root = ramdisk + root_sect * 512;
    int slot = -1;

    for (int i = 0; i < root_cnt; i++) {
        u8* e = root + i * 32;
        if (e[0] == 0 || (u8)e[0] == 0xE5) {
            if (slot < 0) slot = i;
            if (e[0] == 0) break;
        } else {
            int ok = 1;
            for (int j = 0; j < 11; j++) if (e[j] != n83[j]) { ok = 0; break; }
            if (ok) { slot = i; break; }
        }
    }
    if (slot < 0) return -1;

    /* If this slot was an existing file with the same name, free its old cluster chain first */
    u8* e = root + slot * 32;
    int ok_same = 1;
    for (int j = 0; j < 11; j++) {
        if (e[j] != n83[j]) { ok_same = 0; break; }
    }
    if (ok_same && e[0] != 0 && (u8)e[0] != 0xE5) {
        int old_cl = e[26] | (e[27] << 8);
        int end_mark = is_fat16 ? 0xFFF8 : 0xFF8;
        while (old_cl >= 2 && old_cl < end_mark) {
            int next = fat_get(old_cl);
            fat_set_entry(old_cl, 0);
            old_cl = next;
        }
    }

    int clusters = (sz + 511) / 512;
    if (clusters == 0) clusters = 1;
    int first_cl = -1, prev_cl = -1;
    int eoc_val = is_fat16 ? 0xFFFF : 0xFFF;

    for (int i = 0; i < clusters; i++) {
        int cl = fat_alloc_cluster();
        if (cl < 0) {
            /* Rollback allocated clusters on failure */
            int curr = first_cl;
            int end_mark = is_fat16 ? 0xFFF8 : 0xFF8;
            while (curr >= 2 && curr < end_mark) {
                int next = fat_get(curr);
                fat_set_entry(curr, 0);
                curr = next;
            }
            return -1;
        }
        fat_set_entry(cl, eoc_val);
        if (prev_cl >= 0) fat_set_entry(prev_cl, cl);
        else first_cl = cl;
        prev_cl = cl;

        int s = data_sect + cl - 2;
        if (s < total_sectors) {
            u8* p = ramdisk + s * 512;
            int n = sz - i * 512;
            if (n > 512) n = 512;
            if (n < 0) n = 0;
            for (int k = 0; k < n; k++) p[k] = data[i * 512 + k];
            for (int k = n; k < 512; k++) p[k] = 0;
        }
    }

    for (int j = 0; j < 11; j++) e[j] = n83[j];
    e[11] = 0x20;
    for (int j = 12; j < 26; j++) e[j] = 0;
    e[26] = first_cl & 0xFF; e[27] = (first_cl >> 8) & 0xFF;
    e[28] = sz & 0xFF; e[29] = (sz >> 8) & 0xFF; e[30] = (sz >> 16) & 0xFF; e[31] = (sz >> 24) & 0xFF;
    return 0;
}

/* Hardware detection & system shutdown */
void cpuid(u32 ax, u32* a, u32* b, u32* c, u32* d) {
    __asm__ volatile("push %%ebx\ncpuid\nmov %%ebx,%%esi\npop %%ebx"
                     : "=a"(*a), "=S"(*b), "=c"(*c), "=d"(*d)
                     : "a"(ax));
}

void get_cpu_brand(char* out, int max_len) {
    u32 max_ext, b, c, d;
    cpuid(0x80000000, &max_ext, &b, &c, &d);
    if (max_ext >= 0x80000004) {
        char raw[49];
        u32* p = (u32*)raw;
        cpuid(0x80000002, &p[0], &p[1], &p[2], &p[3]);
        cpuid(0x80000003, &p[4], &p[5], &p[6], &p[7]);
        cpuid(0x80000004, &p[8], &p[9], &p[10], &p[11]);
        raw[48] = 0;
        char* s = raw;
        while (*s == ' ') s++;
        int i = 0;
        while (s[i] && i < max_len - 1) { out[i] = s[i]; i++; }
        out[i] = 0;
    } else {
        u32 a;
        cpuid(0, &a, &b, &d, &c);
        char ven[13];
        *(u32*)&ven[0] = b; *(u32*)&ven[4] = c; *(u32*)&ven[8] = d; ven[12] = 0;
        int i = 0;
        while (ven[i] && i < max_len - 1) { out[i] = ven[i]; i++; }
        out[i] = 0;
    }
}

static u8 cmos_read(u8 reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static u8 bcd2bin(u8 val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

void get_rtc_time(char* out) {
    while (cmos_read(0x0A) & 0x80) {}
    u8 s = cmos_read(0x00);
    u8 m = cmos_read(0x02);
    u8 h = cmos_read(0x04);
    u8 reg_b = cmos_read(0x0B);
    if (!(reg_b & 0x04)) {
        s = bcd2bin(s);
        m = bcd2bin(m);
        h = ((h & 0x7F) >> 4) * 10 + (h & 0x0F);
    }
    if (!(reg_b & 0x02) && (cmos_read(0x04) & 0x80)) {
        h = (h + 12) % 24;
    }
    out[0] = '0' + (h / 10);
    out[1] = '0' + (h % 10);
    out[2] = ':';
    out[3] = '0' + (m / 10);
    out[4] = '0' + (m % 10);
    out[5] = ':';
    out[6] = '0' + (s / 10);
    out[7] = '0' + (s % 10);
    out[8] = 0;
}

void system_shutdown(void) {
    /* 1. Try QEMU ACPI poweroff port 0x604 */
    outw(0x604, 0x2000);

    /* 2. Try older QEMU / Bochs ACPI poweroff port 0xB004 */
    outw(0xB004, 0x2000);

    /* 3. Try VirtualBox poweroff port 0x4004 */
    outw(0x4004, 0x3400);

    /* 4. Try APM poweroff port 0x8900 */
    outw(0x8900, 0x8900);

    /* 5. Safe fallback screen for architectures without direct port poweroff */
    gui_rect(0, 0, fw, fh, 0x000000);
    const char* msg1 = "Mozna teraz bezpiecznie wylaczyc komputer.";
    const char* msg2 = "It is now safe to turn off your computer.";
    int w1 = gui_str_width(msg1);
    int w2 = gui_str_width(msg2);
    gui_print(msg1, (fw - w1) / 2, fh / 2 - 20, 0xFF8000);
    gui_print(msg2, (fw - w2) / 2, fh / 2 + 10, 0xC0C0C0);
    gui_flip();

    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* GUI Engine */
u8* backbuf = (u8*)0x200000;

/* BGA (Bochs/QEMU/VMware VBE Extensions) I/O ports */
#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF
#define VBE_DISPI_INDEX_ID              0x00
#define VBE_DISPI_INDEX_XRES            0x01
#define VBE_DISPI_INDEX_YRES            0x02
#define VBE_DISPI_INDEX_BPP             0x03
#define VBE_DISPI_INDEX_ENABLE          0x04
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40

static inline void bga_write(u16 index, u16 data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static inline u16 bga_read(u16 index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/* Live display mode switch — delegates to unified hardware driver */
void vesa_live_switch(u16 mode) {
    int target_w = 800, target_h = 600;
    if (mode == VESA_640x480) { target_w = 640; target_h = 480; }
    else if (mode == 0x4118 || mode == 0x118) { target_w = 1024; target_h = 768; }
    else if (mode == 0x4143 || mode == 0x143) { target_w = 1280; target_h = 960; }
    drv_display_set_resolution(target_w, target_h);
}


void gui_pixel(int x, int y, u32 c) {
    if (x < 0 || x >= fw || y < 0 || y >= fh) return;
    u8* p = backbuf + y * fpitch + x * (fbpp / 8);
    p[0] = c & 0xFF; p[1] = (c >> 8) & 0xFF; p[2] = (c >> 16) & 0xFF;
    if (fbpp == 32) p[3] = 0;
}

void gui_rect(int x, int y, int w, int h, u32 c) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) gui_pixel(x + i, y + j, c);
    }
}

void gui_hline(int x, int y, int w, u32 c) {
    for (int i = 0; i < w; i++) gui_pixel(x + i, y, c);
}

void gui_vline(int x, int y, int h, u32 c) {
    for (int j = 0; j < h; j++) gui_pixel(x, y + j, c);
}

int gui_char(char ch, int px, int py, u32 fg, u32 bg) {
    u8 uc = (u8)ch;
    if (font_loaded) {
        int w = sys_font->width[uc];
        if (w == 0) w = sys_font->def_width ? sys_font->def_width : 8;
        int h = sys_font->height;
        if (bg != 0xFFFFFFFF) gui_rect(px, py, w + 1, h, bg);
        for (int r = 0; r < h; r++) {
            u16 b = sys_font->bitmap[uc][r];
            for (int cbit = 0; cbit < w; cbit++) {
                if (b & (0x8000 >> cbit)) gui_pixel(px + cbit, py + r, fg);
            }
        }
        return w;
    } else if (bi_font) {
        const u8* f = (const u8*)(unsigned long)bi_font + uc * 16;
        if (bg != 0xFFFFFFFF) gui_rect(px, py, 9, 16, bg);
        for (int r = 0; r < 16; r++) {
            u8 b = f[r];
            for (int cbit = 0; cbit < 8; cbit++) {
                if (b & (0x80 >> cbit)) gui_pixel(px + cbit, py + r, fg);
            }
        }
        return 8;
    }
    return 8;
}

int gui_str_width(const char* s) {
    int w = 0;
    while (*s) {
        if (*s != '\n') {
            int cw = 8;
            if (font_loaded) {
                cw = sys_font->width[(u8)*s];
                if (cw == 0) cw = sys_font->def_width ? sys_font->def_width : 8;
            }
            w += cw + 1;
        }
        s++;
    }
    return w;
}

void gui_print(const char* s, int px, int py, u32 fg) {
    int cx = px;
    int lh = font_loaded ? sys_font->height + 4 : 16;
    while (*s) {
        if (*s == '\n') {
            py += lh;
            cx = px;
        } else {
            int cw = gui_char(*s, cx, py, fg, 0xFFFFFFFF);
            cx += cw + 1;
        }
        s++;
    }
}

void gui_print_bg(const char* s, int px, int py, u32 fg, u32 bg) {
    int cx = px;
    int lh = font_loaded ? sys_font->height + 4 : 16;
    while (*s) {
        if (*s == '\n') {
            py += lh;
            cx = px;
        } else {
            int cw = gui_char(*s, cx, py, fg, bg);
            cx += cw + 1;
        }
        s++;
    }
}

void gui_print_clipped(const char* s, int px, int py, u32 fg, int max_x) {
    int cx = px;
    while (*s) {
        int cw = 8;
        if (font_loaded) {
            cw = sys_font->width[(u8)*s];
            if (cw == 0) cw = sys_font->def_width ? sys_font->def_width : 8;
        }
        if (cx + cw > max_x) break;
        gui_char(*s, cx, py, fg, 0xFFFFFFFF);
        cx += cw + 1;
        s++;
    }
}

void gui_char_big(char ch, int px, int py, u32 fg, int sc) {
    u8 uc = (u8)ch;
    if (font_loaded) {
        int w = sys_font->width[uc]; if (w == 0) w = 8;
        int h = sys_font->height;
        for (int r = 0; r < h; r++) {
            u16 b = sys_font->bitmap[uc][r];
            for (int c = 0; c < w; c++) {
                if (b & (0x8000 >> c)) gui_rect(px + c * sc, py + r * sc, sc, sc, fg);
            }
        }
    } else if (bi_font) {
        const u8* f = (const u8*)(unsigned long)bi_font + uc * 16;
        for (int r = 0; r < 16; r++) {
            u8 b = f[r];
            for (int c = 0; c < 8; c++) {
                if (b & (0x80 >> c)) gui_rect(px + c * sc, py + r * sc, sc, sc, fg);
            }
        }
    }
}

void gui_str_big(const char* s, int px, int py, u32 fg, int sc) {
    int cx = px;
    while (*s) {
        gui_char_big(*s, cx, py, fg, sc);
        int cw = 8;
        if (font_loaded) {
            cw = sys_font->width[(u8)*s];
            if (cw == 0) cw = 8;
        }
        cx += (cw + 1) * sc;
        s++;
    }
}

void gui_btn(int x, int y, int w, int h, const char* label, int pressed) {
    gui_rect(x, y, w, h, 0xC0C0C0);
    if (pressed) {
        gui_hline(x, y, w, 0x808080); gui_vline(x, y, h, 0x808080);
        gui_hline(x, y + h - 1, w, 0xFFFFFF); gui_vline(x + w - 1, y, h, 0xFFFFFF);
    } else {
        gui_hline(x, y, w, 0xFFFFFF); gui_vline(x, y, h, 0xFFFFFF);
        gui_hline(x, y + h - 1, w, 0x808080); gui_vline(x + w - 1, y, h, 0x808080);
    }
    int ll = gui_str_width(label);
    int fh_b = font_loaded ? sys_font->height : 16;
    gui_print(label, x + (w - ll) / 2 + (pressed ? 1 : 0), y + (h - fh_b) / 2 + (pressed ? 1 : 0), 0x000000);
}

void gui_flip(void) {
    u32* s = (u32*)backbuf;
    u32* d = (u32*)fb;
    if (fbpp == 32 && fpitch == fw * 4) {
        int n = fw * fh;
        for (int i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (int y = 0; y < fh; y++) {
            u8* src = backbuf + y * fpitch;
            u8* dst = fb + y * fpitch;
            for (int x = 0; x < fw * (fbpp / 8); x++) dst[x] = src[x];
        }
    }
}

static const char* cursor_sprite[16] = {
    "*               ", "**              ", "*.*             ", "*..*            ",
    "*...*           ", "*....*          ", "*.....*         ", "*......*        ",
    "*.......*       ", "*.....****      ", "*..*..*         ", "*.* .*..*       ",
    "**   *..*       ", "*     *..*      ", "       **       ", "                "
};

static const char* cursor_hand[16] = {
    "   *            ", "  *.*           ", "  *.*           ", "  *.*           ",
    "  *.***         ", "  *.*..*..      ", " **.*...*..     ", "*.*..........   ",
    "*............   ", "*.............  ", "*.............  ", " *............  ",
    " *............  ", "  *...........  ", "  *..........   ", "   **********   "
};

static int cursor_is_link = 0;

void draw_cursor_normal(int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            char c = cursor_sprite[y][x];
            if (c == '*') gui_pixel(mx + x, my + y, 0x000000);
            else if (c == '.') gui_pixel(mx + x, my + y, 0xFFFFFF);
        }
    }
}

void draw_cursor_hand(int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            char c = cursor_hand[y][x];
            if (c == '*') gui_pixel(mx + x, my + y, 0x000000);
            else if (c == '.') gui_pixel(mx + x, my + y, 0xFFFFFF);
        }
    }
}

void draw_cursor(int mx, int my) {
    if (cursor_is_link) draw_cursor_hand(mx, my);
    else draw_cursor_normal(mx, my);
}

/* BrickOS XP-Style Boot Animation */
static void draw_brickos_emblem(int fx, int fy) {
    for (int y = 0; y < 16; y++) {
        int shift = y / 2;
        /* Tile 1: Cyan/Blue (top-left) */
        for (int x = 6 + shift; x <= 20 + shift; x++) {
            u32 color = (y == 0) ? 0x90E0FF : (y < 4 ? 0x38B4FF : (y < 11 ? 0x0078FF : 0x0040B8));
            gui_pixel(fx + x, fy + 4 + y, color);
        }
        /* Tile 2: Emerald Green (top-right) */
        for (int x = 25 + shift; x <= 39 + shift; x++) {
            u32 color = (y == 0) ? 0x98FFA0 : (y < 4 ? 0x50E040 : (y < 11 ? 0x20A818 : 0x107010));
            gui_pixel(fx + x, fy + 4 + y, color);
        }
    }

    for (int y = 0; y < 16; y++) {
        int shift = y / 2;
        /* Tile 3: Crimson/Orange (bottom-left) */
        for (int x = 6 + shift; x <= 20 + shift; x++) {
            u32 color = (y == 0) ? 0xFFA088 : (y < 4 ? 0xFF5538 : (y < 11 ? 0xDD1818 : 0x990808));
            gui_pixel(fx + x, fy + 24 + y, color);
        }
        /* Tile 4: Amber/Gold (bottom-right) */
        for (int x = 25 + shift; x <= 39 + shift; x++) {
            u32 color = (y == 0) ? 0xFFF090 : (y < 4 ? 0xFFD030 : (y < 11 ? 0xF09000 : 0xB86000));
            gui_pixel(fx + x, fy + 24 + y, color);
        }
    }

    /* Soft trailing light rays on right side */
    for (int x = 46; x < 58; x++) {
        int fade = (58 - x) * 14;
        gui_pixel(fx + x, fy + 12, ((fade * 2) << 16) | (fade << 8) | 0x00);
        gui_pixel(fx + x, fy + 32, (fade << 16) | ((fade * 2) << 8) | (fade * 3));
    }
}

void boot_anim(void) {
    gui_rect(0, 0, fw, fh, 0x000000);

    int lockup_w = 190;
    int lx = (fw - lockup_w) / 2;
    int ly = fh / 2 - 55;

    /* BrickOS custom aerodynamic glass crest */
    draw_brickos_emblem(lx, ly + 2);

    /* Text */
    int tx = lx + 58;
    gui_print("FlatronTech", tx + 2, ly - 3, 0x8E8E9E);
    gui_str_big("BrickOS", tx, ly + 12, 0xFFFFFF, 2);

    /* Progress bar track (Windows XP sunken marquee style) */
    int track_w = 160, track_h = 13;
    int bx = (fw - track_w) / 2;
    int by = ly + 72;

    gui_rect(bx - 2, by - 2, track_w + 4, track_h + 4, 0x2A3852);
    gui_rect(bx - 1, by - 1, track_w + 2, track_h + 2, 0x141C2A);
    gui_rect(bx, by, track_w, track_h, 0x000000);

    /* Rounded corners */
    gui_pixel(bx - 2, by - 2, 0x000000);
    gui_pixel(bx + track_w + 1, by - 2, 0x000000);
    gui_pixel(bx - 2, by + track_h + 1, 0x000000);
    gui_pixel(bx + track_w + 1, by + track_h + 1, 0x000000);

    /* Footer text */
    const char* foot = "Starting BrickOS...";
    int fl = gui_str_width(foot);
    gui_print(foot, (fw - fl) / 2, fh - 35, 0x555566);

    gui_flip();

    /* 3 scrolling blue marquee blocks (authentic Windows XP style) */
    int group_w = 24; /* 3 blocks of 6px with 3px gap */
    u32 t = ticks;

    for (int pass = 0; pass < 4; pass++) {
        for (int pos = -group_w; pos < track_w + 5; pos += 3) {
            while (ticks < t + 2) __asm__ volatile("hlt");
            t = ticks;

            /* Any key skips boot animation */
            if (kbd_try()) goto boot_done;

            /* Clear inside track */
            gui_rect(bx, by + 1, track_w, track_h - 2, 0x000000);

            /* Draw 3 glossy blue blocks */
            for (int k = 0; k < 3; k++) {
                int x0 = bx + pos + k * 9;
                for (int cx = x0; cx < x0 + 6; cx++) {
                    if (cx < bx || cx >= bx + track_w) continue;
                    for (int ry = 0; ry < 9; ry++) {
                        int cy = by + 2 + ry;
                        u32 color = 0x0066FF;
                        if (ry == 0) color = 0xA0E4FF;
                        else if (ry == 1) color = 0x50B4FF;
                        else if (ry >= 2 && ry <= 5) color = 0x0066FF;
                        else if (ry == 6 || ry == 7) color = 0x0040C0;
                        else color = 0x002080;

                        if (cx == x0 || cx == x0 + 5) {
                            if (ry > 1 && ry < 7) color = 0x3388FF;
                        }
                        gui_pixel(cx, cy, color);
                    }
                }
            }
            gui_flip();
        }
    }

boot_done:
    t = ticks;
    while (ticks < t + 20) __asm__ volatile("hlt");
}

/* Window Manager */
struct Window wins[MAX_WINS];
int nwins = 0;
static int drag_win = -1, drag_ox = 0, drag_oy = 0;

void win_raise(int i) {
    if (i == nwins - 1) return;
    struct Window tmp = wins[i];
    wins[i] = wins[nwins - 1];
    wins[nwins - 1] = tmp;
}

int win_find(int app_id) {
    for (int i = 0; i < nwins; i++) {
        if (wins[i].app_id == app_id) return i;
    }
    return -1;
}

int win_new(int app_id, int x, int y, int w, int h, const char* title) {
    if (nwins >= MAX_WINS) return -1;
    int i = nwins++;
    wins[i].x = x; wins[i].y = y; wins[i].w = w; wins[i].h = h;
    wins[i].visible = 1; wins[i].minimized = 0; wins[i].maximized = 0;
    wins[i].prev_x = x; wins[i].prev_y = y; wins[i].prev_w = w; wins[i].prev_h = h;
    wins[i].app_id = app_id; wins[i].app_state = 0;

    int ti = 0;
    while (title[ti] && ti < 31) {
        wins[i].title[ti] = title[ti];
        ti++;
    }
    wins[i].title[ti] = 0;
    return i;
}

int win_client_x(struct Window* w) { return w->x + BORDER + 2; }
int win_client_y(struct Window* w) { return w->y + BORDER + TITLEBAR_H + 2; }
int win_client_w(struct Window* w) { return w->w - BORDER * 2 - 4; }
int win_client_h(struct Window* w) { return w->h - BORDER - TITLEBAR_H - 4; }

void draw_win_chrome(struct Window* w, int focused) {
    gui_rect(w->x, w->y, w->w, w->h, 0xC0C0C0);
    gui_hline(w->x, w->y, w->w, 0xFFFFFF); gui_vline(w->x, w->y, w->h, 0xFFFFFF);
    gui_hline(w->x, w->y + w->h - 1, w->w, 0x000000); gui_vline(w->x + w->w - 1, w->y, w->h, 0x000000);
    gui_hline(w->x + 1, w->y + 1, w->w - 2, 0xDFDFDF); gui_vline(w->x + 1, w->y + 1, w->h - 2, 0xDFDFDF);
    gui_hline(w->x + 1, w->y + w->h - 2, w->w - 2, 0x808080); gui_vline(w->x + w->w - 2, w->y + 1, w->h - 2, 0x808080);

    /* Titlebar */
    u32 tbcol = focused ? 0x000080 : 0x808080;
    gui_rect(w->x + BORDER, w->y + BORDER, w->w - BORDER * 2, TITLEBAR_H, tbcol);
    int title_max_x = w->x + w->w - BORDER - 56;
    gui_print_clipped(w->title, w->x + BORDER + 4, w->y + BORDER + 2, 0xFFFFFF, title_max_x);

    /* Buttons: Close, Maximize, Minimize */
    int by_ = w->y + BORDER + 3;
    int cbx = w->x + w->w - BORDER - 18;
    gui_rect(cbx, by_, 14, 14, 0xC0C0C0);
    gui_hline(cbx, by_, 14, 0xFFFFFF); gui_vline(cbx, by_, 14, 0xFFFFFF);
    gui_hline(cbx, by_ + 13, 14, 0x000000); gui_vline(cbx + 13, by_, 14, 0x000000);
    gui_print("X", cbx + 3, by_, 0x000000);

    int mxbx = cbx - 17;
    gui_rect(mxbx, by_, 14, 14, 0xC0C0C0);
    gui_hline(mxbx, by_, 14, 0xFFFFFF); gui_vline(mxbx, by_, 14, 0xFFFFFF);
    gui_hline(mxbx, by_ + 13, 14, 0x000000); gui_vline(mxbx + 13, by_, 14, 0x000000);
    if (w->maximized) {
        gui_print("2", mxbx + 3, by_, 0x000000);
    } else {
        gui_hline(mxbx + 2, by_ + 3, 10, 0x000000);
        gui_hline(mxbx + 2, by_ + 4, 10, 0x000000);
        gui_hline(mxbx + 2, by_ + 11, 10, 0x000000);
        gui_vline(mxbx + 2, by_ + 3, 9, 0x000000);
        gui_vline(mxbx + 11, by_ + 3, 9, 0x000000);
    }

    int mnbx = mxbx - 17;
    gui_rect(mnbx, by_, 14, 14, 0xC0C0C0);
    gui_hline(mnbx, by_, 14, 0xFFFFFF); gui_vline(mnbx, by_, 14, 0xFFFFFF);
    gui_hline(mnbx, by_ + 13, 14, 0x000000); gui_vline(mnbx + 13, by_, 14, 0x000000);
    gui_hline(mnbx + 2, by_ + 9, 10, 0x000000);
    gui_hline(mnbx + 2, by_ + 10, 10, 0x000000);

    /* Client area */
    int cyo = w->y + BORDER + TITLEBAR_H;
    int cw = w->w - BORDER * 2, ch = w->h - BORDER - TITLEBAR_H;
    gui_rect(w->x + BORDER, cyo, cw, ch, 0xFFFFFF);
    gui_hline(w->x + BORDER, cyo, cw, 0x808080); gui_vline(w->x + BORDER, cyo, ch, 0x808080);
    gui_hline(w->x + BORDER, cyo + ch - 1, cw, 0xFFFFFF); gui_vline(w->x + BORDER + cw - 1, cyo, ch, 0xFFFFFF);

    /* Resize handle */
    if (!w->maximized) {
        int gx = w->x + w->w - 12, gy = w->y + w->h - 12;
        for (int g = 0; g < 3; g++) {
            int off = g * 3;
            gui_hline(gx + 6 - off, gy + 8, off + 2, 0xFFFFFF);
            gui_hline(gx + 6 - off, gy + 7, off + 2, 0x808080);
        }
    }
}

int win_close_hit(struct Window* w, int mx, int my) {
    int cbx = w->x + w->w - BORDER - 18, by_ = w->y + BORDER + 3;
    return mx >= cbx && mx < cbx + 14 && my >= by_ && my < by_ + 14;
}

int win_max_hit(struct Window* w, int mx, int my) {
    int cbx = w->x + w->w - BORDER - 35, by_ = w->y + BORDER + 3;
    return mx >= cbx && mx < cbx + 14 && my >= by_ && my < by_ + 14;
}

int win_min_hit(struct Window* w, int mx, int my) {
    int cbx = w->x + w->w - BORDER - 52, by_ = w->y + BORDER + 3;
    return mx >= cbx && mx < cbx + 14 && my >= by_ && my < by_ + 14;
}

void open_file_in_notepad(const char* filename) {
    int np_win = win_find(APP_NOTEPAD);
    char title[32] = "Notepad - ";
    int ti = 10;
    for (int j = 0; filename[j] && ti < 30; j++) title[ti++] = filename[j];
    title[ti] = 0;

    if (np_win < 0) {
        win_new(APP_NOTEPAD, 100, 50, 540, 390, title);
    } else {
        win_raise(np_win);
        np_win = nwins - 1;
        wins[np_win].minimized = 0;
        wins[np_win].visible = 1;
        my_strcpy(wins[np_win].title, title);
    }
    note_load_file(filename);
    note_active = 1;
    term_active = 0;
    calc_active = 0;
}

void open_app(int app_id) {
    int ex = win_find(app_id);
    if (ex >= 0) {
        win_raise(ex);
        wins[nwins - 1].minimized = 0;
        wins[nwins - 1].visible = 1;
        if (app_id == APP_NOTEPAD) { note_active = 1; term_active = 0; calc_active = 0; }
        else if (app_id == APP_TERMINAL) { term_active = 1; note_active = 0; calc_active = 0; }
        else if (app_id == APP_CALC) { calc_active = 1; term_active = 0; note_active = 0; }
        return;
    }

    switch (app_id) {
        case APP_PAINT: {
            int wi = win_new(APP_PAINT, 100, 50, 560, 400, "Paint");
            if (wi >= 0) { paint_init(); }
            break;
        }
        case APP_NOTEPAD: {
            int wi = win_new(APP_NOTEPAD, 120, 60, 540, 390, "Notepad");
            if (wi >= 0) {
                note_active = 1; term_active = 0; calc_active = 0;
            }
            break;
        }
        case APP_CALC: {
            int wi = win_new(APP_CALC, 160, 80, 220, 260, "Calculator");
            if (wi >= 0) {
                calc_init(); calc_active = 1; note_active = 0; term_active = 0;
            }
            break;
        }
        case APP_TASKMGR:
            win_new(APP_TASKMGR, 140, 70, 440, 350, "Task Manager");
            break;
        case APP_TERMINAL: {
            int wi = win_new(APP_TERMINAL, 110, 80, 480, 320, "Terminal");
            if (wi >= 0) {
                term_active = 1; note_active = 0; calc_active = 0;
            }
            break;
        }
        case APP_SYSINFO:
            win_new(APP_SYSINFO, 140, 50, 480, 330, "System Info");
            break;
        case APP_EXPLORER:
            win_new(APP_EXPLORER, 150, 60, 480, 350, "My Computer");
            break;
        case APP_CTRLPANEL:
            win_new(APP_CTRLPANEL, 140, 50, 520, 380, "Control Panel");
            break;
        case APP_WELCOME:
            win_new(APP_WELCOME, 180, 80, 420, 250, "Welcome");
            break;
        case APP_BMPVIEW:
            open_bmp_viewer("BRICK.BMP");
            break;
        default: break;
    }
}

/* Desktop icons */
#define NDESK_ICONS 7
struct DesktopIcon { int x, y, app_id; const char* label; };
static struct DesktopIcon desk_icons[NDESK_ICONS] = {
    {24,  20, APP_EXPLORER,  "My Computer"},
    {24, 105, APP_CTRLPANEL, "Control Panel"},
    {24, 190, APP_NOTEPAD,   "Notepad"},
    {24, 275, APP_PAINT,     "Paint"},
    {24, 360, APP_CALC,      "Calculator"},
    {24, 445, APP_TERMINAL,  "Terminal"},
    {110, 20, APP_WELCOME,   "Welcome"}
};

static int desk_sel = -1;
static int desk_last = -1;
static u32 desk_last_t = 0;

static void draw_desk_label(const char* label, int cx, int y, int selected) {
    char w1[16], w2[16];
    int sp = -1;
    for (int i = 0; label[i]; i++) {
        if (label[i] == ' ') { sp = i; break; }
    }
    int fh_box = font_loaded ? sys_font->height : 14;
    u32 bg_col = selected ? 0x000080 : desktop_color;
    if (sp >= 0) {
        int i;
        for (i = 0; i < sp && i < 15; i++) w1[i] = label[i];
        w1[i] = 0;
        int j = 0;
        for (i = sp + 1; label[i] && j < 15; i++) w2[j++] = label[i];
        w2[j] = 0;
        int l1 = gui_str_width(w1);
        int l2 = gui_str_width(w2);
        int x1 = cx - l1 / 2; if (x1 < 2) x1 = 2;
        int x2 = cx - l2 / 2; if (x2 < 2) x2 = 2;
        gui_rect(x1 - 2, y, l1 + 4, fh_box + 1, bg_col);
        gui_print(w1, x1, y, 0xFFFFFF);
        gui_rect(x2 - 2, y + fh_box + 2, l2 + 4, fh_box + 1, bg_col);
        gui_print(w2, x2, y + fh_box + 2, 0xFFFFFF);
    } else {
        int l = gui_str_width(label);
        int x = cx - l / 2; if (x < 2) x = 2;
        gui_rect(x - 2, y, l + 4, fh_box + 1, bg_col);
        gui_print(label, x, y, 0xFFFFFF);
    }
}

static void draw_icon_my_computer(int x, int y) {
    /* Monitor casing */
    gui_rect(x + 2, y, 26, 20, 0xC0C0C0);
    gui_hline(x + 2, y, 26, 0xFFFFFF); gui_vline(x + 2, y, 20, 0xFFFFFF);
    gui_hline(x + 2, y + 19, 26, 0x404040); gui_vline(x + 27, y, 20, 0x404040);
    /* Screen inner bezel */
    gui_rect(x + 5, y + 3, 20, 14, 0x808080);
    gui_hline(x + 5, y + 3, 20, 0x404040); gui_vline(x + 5, y + 3, 14, 0x404040);
    gui_hline(x + 5, y + 16, 20, 0xFFFFFF); gui_vline(x + 24, y + 3, 14, 0xFFFFFF);
    /* Screen content (teal Win95 desktop) */
    gui_rect(x + 6, y + 4, 18, 12, 0x008080);
    /* Little window on screen */
    gui_rect(x + 8, y + 6, 10, 7, 0xFFFFFF);
    gui_rect(x + 8, y + 6, 10, 2, 0x000080);
    /* Monitor neck */
    gui_rect(x + 12, y + 20, 6, 3, 0xA0A0A0);
    /* Monitor stand base */
    gui_rect(x + 8, y + 22, 14, 2, 0xD0D0D0);
    gui_hline(x + 8, y + 22, 14, 0xFFFFFF);
    gui_hline(x + 8, y + 23, 14, 0x404040);
    /* Desktop PC tower/box underneath */
    gui_rect(x + 1, y + 24, 30, 8, 0xC0C0C0);
    gui_hline(x + 1, y + 24, 30, 0xFFFFFF); gui_vline(x + 1, y + 24, 8, 0xFFFFFF);
    gui_hline(x + 1, y + 31, 30, 0x404040); gui_vline(x + 30, y + 24, 8, 0x404040);
    /* 3.5" Floppy drive slot */
    gui_rect(x + 5, y + 26, 12, 2, 0x000000);
    gui_rect(x + 18, y + 26, 2, 2, 0x808080);
    /* Power LED (green) and switch */
    gui_pixel(x + 23, y + 27, 0x00FF00);
    gui_rect(x + 25, y + 26, 3, 3, 0x808080);
}

static void draw_icon_ctrlpanel(int x, int y) {
    /* Folder back tab */
    gui_rect(x + 2, y + 4, 11, 4, 0xC89820);
    gui_hline(x + 2, y + 4, 11, 0xFFE080);
    /* Folder body */
    gui_rect(x + 2, y + 7, 28, 22, 0xD4A028);
    gui_hline(x + 2, y + 7, 28, 0xFFE890);
    gui_vline(x + 2, y + 7, 22, 0xFFE890);
    gui_hline(x + 2, y + 28, 28, 0x604000);
    gui_vline(x + 29, y + 7, 22, 0x604000);
    /* White sheet / panel inside folder */
    gui_rect(x + 5, y + 10, 22, 16, 0xFFFFFF);
    gui_hline(x + 5, y + 10, 22, 0x808080); gui_vline(x + 5, y + 10, 16, 0x808080);
    /* Three slider tracks */
    for (int s = 0; s < 3; s++) {
        int sy2 = y + 13 + s * 4;
        gui_hline(x + 7, sy2, 18, 0x808080);
        gui_pixel(x + 7, sy2 + 1, 0xC0C0C0);
    }
    /* Slider knobs (3D metallic) */
    int kx[3] = {11, 19, 14};
    for (int s = 0; s < 3; s++) {
        int sx2 = x + kx[s], sy2 = y + 12 + s * 4;
        gui_rect(sx2, sy2, 4, 3, 0xC0C0C0);
        gui_hline(sx2, sy2, 4, 0xFFFFFF);
        gui_vline(sx2, sy2, 3, 0xFFFFFF);
        gui_hline(sx2, sy2 + 2, 4, 0x000000);
        gui_vline(sx2 + 3, sy2, 3, 0x000000);
    }
}

static void draw_icon_notepad(int x, int y) {
    /* Notepad page with folded corner */
    gui_rect(x + 5, y + 2, 19, 27, 0xFFFFFF);
    /* Folded corner in top right */
    gui_rect(x + 19, y + 2, 5, 5, 0xC0C0C0);
    gui_hline(x + 19, y + 7, 5, 0x808080);
    gui_vline(x + 18, y + 2, 5, 0x808080);
    /* Outer border */
    gui_hline(x + 5, y + 2, 14, 0x808080);
    gui_vline(x + 5, y + 2, 27, 0x808080);
    gui_hline(x + 5, y + 28, 19, 0x808080);
    gui_vline(x + 23, y + 7, 22, 0x808080);
    /* Notebook spiral rings on left */
    for (int r = 0; r < 5; r++) {
        int ry = y + 5 + r * 5;
        gui_rect(x + 3, ry, 4, 2, 0x404040);
        gui_pixel(x + 4, ry, 0xFFFFFF);
    }
    /* Ruled blue lines */
    for (int l = 0; l < 4; l++) {
        gui_hline(x + 8, y + 9 + l * 4, 12, 0x5080C0);
    }
    /* Yellow HB Pencil resting diagonally */
    for (int p = 0; p < 12; p++) {
        int px2 = x + 12 + p, py2 = y + 28 - p;
        gui_pixel(px2, py2, 0xFFA500);
        gui_pixel(px2 + 1, py2, 0xFFD700);
        gui_pixel(px2, py2 - 1, 0xD48800);
    }
    /* Pencil lead tip */
    gui_pixel(x + 11, y + 29, 0x000000);
    gui_pixel(x + 12, y + 29, 0xC49A6C);
    /* Pencil eraser (pink) and metal ferrule */
    gui_rect(x + 23, y + 16, 3, 2, 0xC0C0C0);
    gui_rect(x + 24, y + 14, 3, 2, 0xFF6B81);
}

static void draw_icon_paint(int x, int y) {
    /* Artist wooden palette */
    for (int r = 0; r < 20; r++) {
        int rw = (r < 3 || r > 16) ? 18 : ((r < 7 || r > 13) ? 24 : 26);
        int rx2 = (r < 3) ? 5 : ((r > 16) ? 7 : 3);
        gui_rect(x + rx2, y + 5 + r, rw, 1, 0xDDB878);
        gui_pixel(x + rx2, y + 5 + r, 0xFFE8B0);
        gui_pixel(x + rx2 + rw - 1, y + 5 + r, 0x8A6020);
    }
    /* Thumb hole */
    gui_rect(x + 21, y + 16, 4, 5, desktop_color);
    gui_hline(x + 21, y + 15, 4, 0x8A6020);
    gui_vline(x + 20, y + 16, 5, 0x8A6020);
    gui_hline(x + 21, y + 21, 4, 0xFFE8B0);
    /* 4 Vibrant Paint Blobs with highlights */
    /* Red */
    gui_rect(x + 6, y + 8, 4, 4, 0xD01010);
    gui_pixel(x + 7, y + 9, 0xFF7070);
    /* Yellow */
    gui_rect(x + 13, y + 7, 4, 4, 0xE8C000);
    gui_pixel(x + 14, y + 8, 0xFFFF80);
    /* Blue */
    gui_rect(x + 8, y + 15, 4, 4, 0x1030D0);
    gui_pixel(x + 9, y + 16, 0x7090FF);
    /* Green */
    gui_rect(x + 13, y + 19, 4, 4, 0x10A020);
    gui_pixel(x + 14, y + 20, 0x70FF70);

    /* Paintbrush crossing diagonally */
    for (int b = 0; b < 14; b++) {
        int bx2 = x + 16 + b, by2 = y + 26 - b;
        gui_pixel(bx2, by2, 0x804010);
    }
    /* Ferrule (metal) */
    gui_rect(x + 14, y + 27, 3, 2, 0xC0C0C0);
    /* Bristles + dipped in red */
    gui_rect(x + 12, y + 28, 3, 2, 0x202020);
    gui_pixel(x + 11, y + 29, 0xE01010);
}

static void draw_icon_calc(int x, int y) {
    /* Calculator body with 3D chiseled bevel */
    gui_rect(x + 4, y + 2, 24, 28, 0xC0C0C0);
    gui_hline(x + 4, y + 2, 24, 0xFFFFFF); gui_vline(x + 4, y + 2, 28, 0xFFFFFF);
    gui_hline(x + 4, y + 29, 24, 0x404040); gui_vline(x + 27, y + 2, 28, 0x404040);
    /* LCD screen (sunken 3D frame) */
    gui_rect(x + 7, y + 5, 18, 7, 0x000000);
    gui_hline(x + 7, y + 5, 18, 0x404040); gui_vline(x + 7, y + 5, 7, 0x404040);
    gui_hline(x + 7, y + 11, 18, 0xFFFFFF); gui_vline(x + 24, y + 5, 7, 0xFFFFFF);
    /* LCD background & number 0 */
    gui_rect(x + 8, y + 6, 16, 5, 0x203028);
    gui_print("0", x + 19, y + 4, 0x00FF80);
    /* Keypad: Clear button (red) */
    gui_rect(x + 7, y + 14, 5, 3, 0xD02020);
    gui_pixel(x + 7, y + 14, 0xFFA0A0);
    /* Memory / operation buttons top row */
    gui_rect(x + 13, y + 14, 5, 3, 0x808080);
    gui_rect(x + 19, y + 14, 6, 3, 0x000080);
    /* Key grid (3x3 number pad) */
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int kx2 = x + 7 + col * 4;
            int ky2 = y + 19 + row * 3;
            gui_rect(kx2, ky2, 3, 2, 0xD0D0D0);
            gui_pixel(kx2, ky2, 0xFFFFFF);
            gui_pixel(kx2 + 2, ky2 + 1, 0x404040);
        }
        /* Rightmost operator button (blue) */
        int okx = x + 20, oky = y + 19 + row * 3;
        gui_rect(okx, oky, 5, 2, 0x000090);
        gui_pixel(okx, oky, 0x6080FF);
    }
}

static void draw_icon_terminal(int x, int y) {
    /* MS-DOS Prompt window icon */
    gui_rect(x + 3, y + 4, 26, 23, 0xC0C0C0);
    gui_hline(x + 3, y + 4, 26, 0xFFFFFF); gui_vline(x + 3, y + 4, 23, 0xFFFFFF);
    gui_hline(x + 3, y + 26, 26, 0x404040); gui_vline(x + 28, y + 4, 23, 0x404040);
    /* Title bar (blue) */
    gui_rect(x + 5, y + 6, 22, 4, 0x000080);
    gui_print("MS", x + 6, y + 4, 0xFFFFFF);
    /* Close box */
    gui_rect(x + 23, y + 7, 3, 2, 0xC0C0C0);
    /* CRT interior (black) */
    gui_rect(x + 5, y + 11, 22, 14, 0x000000);
    /* Prompt text C:\> in bright green / white */
    gui_print("C:>", x + 7, y + 11, 0x00FF00);
    /* Blinking underscore cursor */
    gui_hline(x + 20, y + 22, 4, 0x00FF00);
}

static void draw_icon_welcome(int x, int y) {
    /* Authentic Windows 95/98 Flying 4-Color Flag */
    /* Top-Left Red tile */
    gui_rect(x + 5,  y + 5,  9, 9, 0xE02020);
    gui_hline(x + 5, y + 5,  9, 0xFF7070); gui_vline(x + 5, y + 5, 9, 0xFF7070);
    gui_hline(x + 5, y + 13, 9, 0x800000); gui_vline(x + 13, y + 5, 9, 0x800000);

    /* Top-Right Green tile */
    gui_rect(x + 17, y + 5,  9, 9, 0x00A020);
    gui_hline(x + 17, y + 5,  9, 0x60F060); gui_vline(x + 17, y + 5, 9, 0x60F060);
    gui_hline(x + 17, y + 13, 9, 0x005010); gui_vline(x + 25, y + 5, 9, 0x005010);

    /* Bottom-Left Blue tile */
    gui_rect(x + 5,  y + 17, 9, 9, 0x0040D0);
    gui_hline(x + 5, y + 17, 9, 0x6090FF); gui_vline(x + 5, y + 17, 9, 0x6090FF);
    gui_hline(x + 5, y + 25, 9, 0x001060); gui_vline(x + 13, y + 17, 9, 0x001060);

    /* Bottom-Right Yellow tile */
    gui_rect(x + 17, y + 17, 9, 9, 0xF0B000);
    gui_hline(x + 17, y + 17, 9, 0xFFF070); gui_vline(x + 17, y + 17, 9, 0xFFF070);
    gui_hline(x + 17, y + 25, 9, 0x805000); gui_vline(x + 25, y + 17, 9, 0x805000);

    /* Trailing black/dark speed dots */
    gui_pixel(x + 15, y + 9, 0x000000);
    gui_pixel(x + 15, y + 21, 0x000000);
    gui_pixel(x + 9, y + 15, 0x000000);
    gui_pixel(x + 21, y + 15, 0x000000);
}

void draw_desk_icons(void) {
    for (int i = 0; i < NDESK_ICONS; i++) {
        struct DesktopIcon* d = &desk_icons[i];
        int x = d->x, y = d->y;

        if (d->app_id == APP_EXPLORER) {
            draw_icon_my_computer(x, y);
        } else if (d->app_id == APP_CTRLPANEL) {
            draw_icon_ctrlpanel(x, y);
        } else if (d->app_id == APP_NOTEPAD) {
            draw_icon_notepad(x, y);
        } else if (d->app_id == APP_PAINT) {
            draw_icon_paint(x, y);
        } else if (d->app_id == APP_CALC) {
            draw_icon_calc(x, y);
        } else if (d->app_id == APP_TERMINAL) {
            draw_icon_terminal(x, y);
        } else {
            draw_icon_welcome(x, y);
        }
        draw_desk_label(d->label, x + 18, y + 36, desk_sel == i);
    }
}

/* Taskbar & Start Menu */
static int start_menu = 0;
static const char* menu_items[] = {
    "Paint",
    "Notepad",
    "Calculator",
    "Task Mgr",
    "Terminal",
    "My Computer",
    "Ctrl Panel",
    "----------",
    "Reboot",
    "Shut Down"
};
#define MENU_N 10
#define MENU_IH 18

void draw_taskbar(void) {
    int ty = fh - TASKBAR_H;
    gui_rect(0, ty, fw, TASKBAR_H, 0xC0C0C0);
    gui_hline(0, ty, fw, 0xFFFFFF);
    gui_hline(0, ty + 1, fw, 0xDFDFDF);
    gui_hline(0, fh - 1, fw, 0x808080);

    /* Start button */
    int sbp = (start_menu || (mouse_btn && mouse_x > 2 && mouse_x < 72 && mouse_y > ty + 2));
    gui_btn(2, ty + 2, 68, TASKBAR_H - 4, "Start", sbp);

    /* Real CMOS Clock */
    char rtc_time[10];
    get_rtc_time(rtc_time);
    int ul = gui_str_width(rtc_time);
    int tray_w = ul + 16;
    gui_rect(fw - tray_w - 4, ty + 3, tray_w, TASKBAR_H - 6, 0xC0C0C0);
    gui_hline(fw - tray_w - 4, ty + 3, tray_w, 0x808080); gui_vline(fw - tray_w - 4, ty + 3, TASKBAR_H - 6, 0x808080);
    gui_hline(fw - tray_w - 4, ty + TASKBAR_H - 4, tray_w, 0xFFFFFF); gui_vline(fw - 5, ty + 3, TASKBAR_H - 6, 0xFFFFFF);
    gui_print(rtc_time, fw - tray_w + 4, ty + 5, 0x000000);

    /* Window buttons */
    int bx = 74, btn_w = 115;
    for (int i = 0; i < nwins; i++) {
        if (bx + btn_w > fw - tray_w - 10) break;
        int foc = (i == nwins - 1);
        int is_min = wins[i].minimized;
        u32 bbg = is_min ? 0xA8A8A8 : (foc ? 0xA0A0A0 : 0xC0C0C0);
        gui_rect(bx, ty + 3, btn_w - 4, TASKBAR_H - 6, bbg);
        if (foc || is_min) {
            gui_hline(bx, ty + 3, btn_w - 4, 0x808080); gui_vline(bx, ty + 3, TASKBAR_H - 6, 0x808080);
            gui_hline(bx, ty + TASKBAR_H - 4, btn_w - 4, 0xFFFFFF); gui_vline(bx + btn_w - 5, ty + 3, TASKBAR_H - 6, 0xFFFFFF);
        } else {
            gui_hline(bx, ty + 3, btn_w - 4, 0xFFFFFF); gui_vline(bx, ty + 3, TASKBAR_H - 6, 0xFFFFFF);
            gui_hline(bx, ty + TASKBAR_H - 4, btn_w - 4, 0x808080); gui_vline(bx + btn_w - 5, ty + 3, TASKBAR_H - 6, 0x808080);
        }
        char tb[20];
        int tl = 0;
        while (wins[i].title[tl] && tl < 18) { tb[tl] = wins[i].title[tl]; tl++; }
        tb[tl] = 0;
        int max_tw = btn_w - 12;
        while (tl > 3 && gui_str_width(tb) > max_tw) { tb[--tl] = 0; }
        int tw = gui_str_width(tb);
        int tx_pos = bx + (btn_w - 4 - tw) / 2;
        int ty_pos = ty + 3 + (TASKBAR_H - 6 - (font_loaded ? sys_font->height : 16)) / 2;
        gui_print(tb, tx_pos, ty_pos, 0x000000);
        bx += btn_w;
    }

    /* Start menu */
    if (start_menu) {
        int mh = MENU_N * MENU_IH + 6, mw = 146, mmx = 2, mmy = ty - mh;
        gui_rect(mmx, mmy, mw, mh, 0xC0C0C0);
        gui_hline(mmx, mmy, mw, 0xFFFFFF); gui_vline(mmx, mmy, mh, 0xFFFFFF);
        gui_vline(mmx + mw - 1, mmy, mh, 0x808080); gui_hline(mmx, mmy + mh - 1, mw, 0x808080);

        /* Side banner */
        gui_rect(mmx + 2, mmy + 2, 18, mh - 4, 0x000080);

        for (int i = 0; i < MENU_N; i++) {
            int iy = mmy + 3 + i * MENU_IH;
            int hov = (mouse_x >= mmx + 20 && mouse_x < mmx + mw - 2 && mouse_y >= iy && mouse_y < iy + MENU_IH);
            if (hov && menu_items[i][0] != '-') {
                gui_rect(mmx + 20, iy, mw - 22, MENU_IH, 0x000080);
            }
            if (menu_items[i][0] == '-') {
                gui_hline(mmx + 20, iy + MENU_IH / 2, mw - 22, 0x808080);
            } else {
                gui_print(menu_items[i], mmx + 24, iy + 1, (hov ? 0xFFFFFF : 0x000000));
            }
        }
    }
}

/* Key dispatch */
void dispatch_key(char c) {
    for (int i = nwins - 1; i >= 0; i--) {
        if (!wins[i].visible || wins[i].minimized) continue;
        switch (wins[i].app_id) {
            case APP_SAVEAS:   saveas_key(c); return;
            case APP_NOTEPAD:  notepad_key(c, &wins[i]); note_active = 1; return;
            case APP_TERMINAL: terminal_key(c); term_active = 1; return;
            case APP_CALC:     calc_key(c); calc_active = 1; return;
            case APP_EXPLORER: explorer_key(c, &wins[i]); return;
            default: return;
        }
    }
}

/* Context menu */
#define CTX_NONE     0
#define CTX_DESKTOP  1
#define CTX_ICON     2
#define CTX_FILE     3

static int ctx_open = 0;
static int ctx_type = CTX_NONE;
static int ctx_x = 0, ctx_y = 0;
static int ctx_target = -1;
static char ctx_file[16] = "";
static int ctx_item_count = 0;
static char ctx_items[5][24];

static int resize_win = -1, resize_ox = 0, resize_oy = 0;

void run_gui(void) {
    esc_pressed = 0;
    start_menu = 0;
    ctx_open = 0;
    nwins = 0;

    paint_init();
    note_init();
    term_init();
    calc_init();
    ctrlpanel_init();

    mouse_btn = 0;
    mouse_cycle = 0;
    int last_btn = 0;

    while (!esc_pressed) {
        char kc = kbd_try();
        if (kc) dispatch_key(kc);

        /* Desktop */
        gui_rect(0, 0, fw, fh - TASKBAR_H, desktop_color);
        for (int gx = 0; gx < fw; gx += 32) {
            gui_vline(gx, 0, fh - TASKBAR_H, ((desktop_color & 0xFEFEFE) >> 1) | (desktop_color & 0x010101));
        }

        draw_desk_icons();

        /* Windows */
        for (int i = 0; i < nwins; i++) {
            if (!wins[i].visible || wins[i].minimized) continue;
            struct Window* w = &wins[i];
            int foc = (i == nwins - 1);
            draw_win_chrome(w, foc);

            switch (w->app_id) {
                case APP_SYSINFO:   draw_sysinfo(w); break;
                case APP_EXPLORER:  draw_explorer(w); break;
                case APP_WELCOME:   draw_welcome(w); break;
                case APP_PAINT:     draw_paint(w, mouse_x, mouse_y, mouse_btn); break;
                case APP_NOTEPAD:   draw_notepad(w); break;
                case APP_CALC:      draw_calc(w); break;
                case APP_TASKMGR:   draw_taskmgr(w); break;
                case APP_TERMINAL:  draw_terminal(w); break;
                case APP_CTRLPANEL: draw_ctrlpanel(w); break;
                case APP_FILEPROPS: draw_fileprops(w); break;
                case APP_BMPVIEW:   draw_bmpview(w); break;
                case APP_SAVEAS:    draw_saveas(w); break;
            }

            /* Unsaved changes modal dialog */
            if (w->app_state == 99) {
                int wdx = w->x + w->w / 2 - 110, wdy = w->y + w->h / 2 - 45;
                gui_rect(wdx, wdy, 220, 90, 0xC0C0C0);
                gui_hline(wdx, wdy, 220, 0xFFFFFF); gui_vline(wdx, wdy, 90, 0xFFFFFF);
                gui_hline(wdx, wdy + 89, 220, 0x000000); gui_vline(wdx + 219, wdy, 90, 0x000000);
                gui_print("Unsaved changes!", wdx + 10, wdy + 10, 0x000000);
                gui_print("Save before closing?", wdx + 10, wdy + 26, 0x000000);
                gui_btn(wdx + 10, wdy + 54, 60, 24, "Save", 0);
                gui_btn(wdx + 80, wdy + 54, 60, 24, "Discard", 0);
                gui_btn(wdx + 150, wdy + 54, 60, 24, "Cancel", 0);
            }
        }

        draw_taskbar();

        /* Context Menu */
        if (ctx_open) {
            int mw = 140, mh = ctx_item_count * 20 + 4;
            int cx_pos = ctx_x, cy_pos = ctx_y;
            if (cx_pos + mw > fw) cx_pos = fw - mw - 2;
            if (cy_pos + mh > fh - TASKBAR_H) cy_pos = fh - TASKBAR_H - mh - 2;

            gui_rect(cx_pos, cy_pos, mw, mh, 0xC0C0C0);
            gui_hline(cx_pos, cy_pos, mw, 0xFFFFFF); gui_vline(cx_pos, cy_pos, mh, 0xFFFFFF);
            gui_hline(cx_pos, cy_pos + mh - 1, mw, 0x000000); gui_vline(cx_pos + mw - 1, cy_pos, mh, 0x000000);
            gui_hline(cx_pos + 1, cy_pos + 1, mw - 2, 0xDFDFDF); gui_vline(cx_pos + 1, cy_pos + 1, mh - 2, 0xDFDFDF);
            gui_hline(cx_pos + 1, cy_pos + mh - 2, mw - 2, 0x808080); gui_vline(cx_pos + mw - 2, cy_pos + 1, mh - 2, 0x808080);

            for (int it = 0; it < ctx_item_count; it++) {
                int iy = cy_pos + 2 + it * 20;
                int hovered = (mouse_x >= cx_pos + 2 && mouse_x < cx_pos + mw - 2 && mouse_y >= iy && mouse_y < iy + 20);
                if (hovered) {
                    gui_rect(cx_pos + 2, iy, mw - 4, 20, 0x000080);
                    gui_print(ctx_items[it], cx_pos + 8, iy + 2, 0xFFFFFF);
                } else {
                    gui_print(ctx_items[it], cx_pos + 8, iy + 2, 0x000000);
                }
            }
        }

        int mx = mouse_x, my = mouse_y;
        int left_click = (mouse_btn & 1) && !(last_btn & 1);
        int right_click = (mouse_btn & 2) && !(last_btn & 2);

        /* Handle paint brush movement during mouse drag */
        if (nwins > 0 && wins[nwins - 1].visible && !wins[nwins - 1].minimized && wins[nwins - 1].app_id == APP_PAINT) {
            paint_mouse_move(&wins[nwins - 1], mx, my, mouse_btn);
        }

        /* Right click menu */
        if (right_click) {
            ctx_open = 0;
            int handled = 0;
            for (int i = nwins - 1; i >= 0; i--) {
                if (!wins[i].visible || wins[i].minimized) continue;
                struct Window* w = &wins[i];
                if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) {
                    win_raise(i);
                    w = &wins[nwins - 1];
                    if (w->app_id == APP_EXPLORER && w->app_state == 1) {
                        int px = win_client_x(w), py0 = win_client_y(w);
                        int hy = py0 + 18;
                        if (my >= hy + 16) {
                            int f_idx = (my - (hy + 16)) / 16;
                            u8* root = ramdisk + root_sect * 512;
                            int cnt = 0;
                            for (int k = 0; k < root_cnt; k++) {
                                u8* e = root + k * 32;
                                if (e[0] == 0) break;
                                if (e[0] == 0xE5 || e[11] == 0x0F) continue;
                                if (cnt == f_idx) {
                                    char nm[14];
                                    int ni = 0;
                                    for (int j = 0; j < 8 && e[j] != ' '; j++) nm[ni++] = e[j];
                                    nm[ni++] = '.';
                                    for (int j = 8; j < 11 && e[j] != ' '; j++) nm[ni++] = e[j];
                                    nm[ni] = 0;

                                    my_strcpy(ctx_file, nm);
                                    ctx_type = CTX_FILE; ctx_x = mx; ctx_y = my;
                                    int is_bin = (e[8] == 'B' && e[9] == 'I' && e[10] == 'N');
                                    int is_bmp2 = (e[8] == 'B' && e[9] == 'M' && e[10] == 'P');
                                    if (is_bin) {
                                        ctx_item_count = 1;
                                        my_strcpy(ctx_items[0], "Properties");
                                    } else if (is_bmp2) {
                                        ctx_item_count = 3;
                                        my_strcpy(ctx_items[0], "Open in Viewer");
                                        my_strcpy(ctx_items[1], "Open in Notepad");
                                        my_strcpy(ctx_items[2], "Properties");
                                    } else {
                                        ctx_item_count = 2;
                                        my_strcpy(ctx_items[0], "Open in Notepad");
                                        my_strcpy(ctx_items[1], "Properties");
                                    }
                                    ctx_open = 1; handled = 1; break;
                                }
                                cnt++;
                            }
                        }
                    }
                    if (handled) break;
                    handled = 1; break;
                }
            }
            if (!handled && my < fh - TASKBAR_H) {
                for (int i = 0; i < NDESK_ICONS; i++) {
                    struct DesktopIcon* d = &desk_icons[i];
                    int cx_ = d->x + 18;
                    if (mx >= cx_ - 32 && mx < cx_ + 32 && my >= d->y && my < d->y + 70) {
                        ctx_type = CTX_ICON; ctx_target = i; ctx_x = mx; ctx_y = my;
                        ctx_item_count = 2;
                        my_strcpy(ctx_items[0], "Open");
                        my_strcpy(ctx_items[1], "Properties");
                        ctx_open = 1; handled = 1; break;
                    }
                }
            }
            if (!handled && my < fh - TASKBAR_H) {
                ctx_type = CTX_DESKTOP; ctx_x = mx; ctx_y = my;
                ctx_item_count = 4;
                my_strcpy(ctx_items[0], "Personalize");
                my_strcpy(ctx_items[1], "New Note");
                my_strcpy(ctx_items[2], "Terminal");
                my_strcpy(ctx_items[3], "Calculator");
                ctx_open = 1;
            }
            goto input_done;
        }

        /* Left click on context menu */
        if (ctx_open && left_click) {
            int mw = 140, mh = ctx_item_count * 20 + 4;
            int cx_pos = ctx_x, cy_pos = ctx_y;
            if (cx_pos + mw > fw) cx_pos = fw - mw - 2;
            if (cy_pos + mh > fh - TASKBAR_H) cy_pos = fh - TASKBAR_H - mh - 2;
            if (mx >= cx_pos && mx < cx_pos + mw && my >= cy_pos && my < cy_pos + mh) {
                int item = (my - (cy_pos + 2)) / 20;
                if (item >= 0 && item < ctx_item_count) {
                    if (ctx_type == CTX_DESKTOP) {
                        if (item == 0) { open_app(APP_CTRLPANEL); ctrlpanel_tab = 2; }
                        else if (item == 1) { open_app(APP_NOTEPAD); }
                        else if (item == 2) { open_app(APP_TERMINAL); }
                        else if (item == 3) { open_app(APP_CALC); }
                    } else if (ctx_type == CTX_ICON) {
                        if (item == 0) { open_app(desk_icons[ctx_target].app_id); }
                        else if (item == 1) { open_app(APP_CTRLPANEL); ctrlpanel_tab = 0; }
                    } else if (ctx_type == CTX_FILE) {
                        if (item == 0 && ctx_items[0][0] == 'O') {
                            if (ctx_items[0][8] == 'V') open_bmp_viewer(ctx_file);
                            else open_file_in_notepad(ctx_file);
                        } else if (item == 1 && ctx_items[1][0] == 'O') {
                            open_file_in_notepad(ctx_file);
                        } else {
                            u8* root = ramdisk + root_sect * 512;
                            char n83[11];
                            fat_name83(ctx_file, n83);
                            for (int k = 0; k < root_cnt; k++) {
                                u8* e = root + k * 32;
                                if (e[0] == 0) break;
                                if (e[0] == 0xE5 || e[11] == 0x0F) continue;
                                int ok = 1;
                                for (int j = 0; j < 11; j++) if (e[j] != n83[j]) ok = 0;
                                if (!ok) continue;
                                u32 fsz = e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24);
                                int fcl = e[26] | (e[27] << 8);
                                int nclust = 0; int cl2 = fcl;
                                int end_mark = is_fat16 ? 0xFFF8 : 0xFF8;
                                while (cl2 >= 2 && cl2 < end_mark && nclust < 1000) { nclust++; cl2 = fat_get(cl2); }
                                const char* fty = "Data File";
                                if (e[8] == 'T' && e[9] == 'X' && e[10] == 'T') fty = "Text File";
                                else if (e[8] == 'I' && e[9] == 'N' && e[10] == 'F') fty = "INF Driver";
                                else if (e[8] == 'B' && e[9] == 'I' && e[10] == 'N') fty = "Binary";
                                else if (e[8] == 'B' && e[9] == 'M' && e[10] == 'P') fty = "Bitmap Image";
                                open_file_props(ctx_file, fsz, fty, nclust);
                                break;
                            }
                        }
                    }
                }
            }
            ctx_open = 0;
            goto input_done;
        }
        if (left_click) ctx_open = 0;

        /* Normal Left Click */
        if (left_click) {
            int ty = fh - TASKBAR_H;

            /* Start menu items */
            if (start_menu) {
                int mh = MENU_N * MENU_IH + 6, mw = 146, mmx = 2, mmy = ty - mh;
                if (mx >= mmx && mx < mmx + mw && my >= mmy && my < ty) {
                    int item = (my - mmy - 3) / MENU_IH;
                    start_menu = 0;
                    if (item >= 0 && item < MENU_N && menu_items[item][0] != '-') {
                        switch (item) {
                            case 0: open_app(APP_PAINT); break;
                            case 1: open_app(APP_NOTEPAD); break;
                            case 2: open_app(APP_CALC); break;
                            case 3: open_app(APP_TASKMGR); break;
                            case 4: open_app(APP_TERMINAL); break;
                            case 5: open_app(APP_EXPLORER); break;
                            case 6: open_app(APP_CTRLPANEL); break;
                            case 8: outb(0x64, 0xFE); break; /* Reboot */
                            case 9: system_shutdown(); break; /* Shut Down */
                        }
                    }
                } else {
                    start_menu = 0;
                }
                goto input_done;
            }

            /* Start button */
            if (mx >= 2 && mx < 70 && my > ty + 2) {
                start_menu = !start_menu;
                goto input_done;
            }

            /* Taskbar window buttons */
            {
                int bx = 74, btn_w = 115;
                for (int i = 0; i < nwins; i++) {
                    if (bx + btn_w > fw - 100) break;
                    if (mx >= bx && mx < bx + btn_w - 4 && my > ty + 3) {
                        if (wins[i].minimized) {
                            wins[i].minimized = 0;
                            win_raise(i);
                        } else if (i == nwins - 1) {
                            wins[i].minimized = 1;
                        } else {
                            win_raise(i);
                        }
                        goto input_done;
                    }
                    bx += btn_w;
                }
            }

            start_menu = 0;

            /* Window resize hit test */
            int resize_hit = -1;
            for (int i = nwins - 1; i >= 0; i--) {
                if (!wins[i].visible || wins[i].minimized || wins[i].maximized) continue;
                struct Window* w = &wins[i];
                int br = (mx >= w->x + w->w - 16 && mx <= w->x + w->w && my >= w->y + w->h - 16 && my <= w->y + w->h);
                int rb = (mx >= w->x + w->w - 4 && mx <= w->x + w->w && my >= w->y && my <= w->y + w->h);
                int bb = (my >= w->y + w->h - 4 && my <= w->y + w->h && mx >= w->x && mx <= w->x + w->w);
                if (br || rb || bb) { resize_hit = i; break; }
            }
            if (resize_hit >= 0) {
                win_raise(resize_hit);
                resize_win = nwins - 1;
                resize_ox = wins[resize_win].w - (mx - wins[resize_win].x);
                resize_oy = wins[resize_win].h - (my - wins[resize_win].y);
                goto input_done;
            }

            /* Desktop icon click */
            {
                int icon_hit = -1;
                for (int i = 0; i < NDESK_ICONS; i++) {
                    struct DesktopIcon* d = &desk_icons[i];
                    int cx_ = d->x + 18;
                    if (mx >= cx_ - 32 && mx < cx_ + 32 && my >= d->y && my < d->y + 70) {
                        icon_hit = i;
                        break;
                    }
                }
                if (icon_hit >= 0) {
                    if (icon_hit == desk_last && (ticks - desk_last_t < 80)) {
                        open_app(desk_icons[icon_hit].app_id);
                        desk_sel = -1;
                    } else {
                        desk_sel = icon_hit;
                    }
                    desk_last = icon_hit;
                    desk_last_t = ticks;
                    goto input_done;
                } else {
                    desk_sel = -1;
                }
            }

            /* Window hit test */
            for (int i = nwins - 1; i >= 0; i--) {
                if (!wins[i].visible || wins[i].minimized) continue;
                struct Window* w = &wins[i];
                if (mx < w->x || mx >= w->x + w->w || my < w->y || my >= w->y + w->h) continue;

                /* Close button */
                if (win_close_hit(w, mx, my)) {
                    int is_note = (w->app_id == APP_NOTEPAD);
                    int is_paint = (w->app_id == APP_PAINT);
                    int dirty_warn = (is_note && note_dirty) || (is_paint && paint_dirty);
                    if (dirty_warn && w->app_state != 99) {
                        w->app_state = 99;
                        break;
                    }
                    if (w->app_id == APP_NOTEPAD) { note_active = 0; note_dirty = 0; }
                    if (w->app_id == APP_TERMINAL) term_active = 0;
                    if (w->app_id == APP_CALC) calc_active = 0;
                    if (w->app_id == APP_PAINT) paint_dirty = 0;
                    for (int j = i; j < nwins - 1; j++) wins[j] = wins[j + 1];
                    nwins--;
                    break;
                }

                /* Dirty close warning dialog buttons */
                if (w->app_state == 99) {
                    int wdx = w->x + w->w / 2 - 110, wdy = w->y + w->h / 2 - 45;
                    if (mx >= wdx + 10 && mx < wdx + 70 && my >= wdy + 54 && my < wdy + 78) {
                        if (w->app_id == APP_NOTEPAD) { note_save(); note_active = 0; note_dirty = 0; }
                        else if (w->app_id == APP_PAINT) { save_paint_bmp(); paint_dirty = 0; }
                        for (int j = i; j < nwins - 1; j++) wins[j] = wins[j + 1];
                        nwins--;
                        break;
                    }
                    if (mx >= wdx + 80 && mx < wdx + 140 && my >= wdy + 54 && my < wdy + 78) {
                        if (w->app_id == APP_NOTEPAD) { note_active = 0; note_dirty = 0; }
                        if (w->app_id == APP_PAINT) paint_dirty = 0;
                        for (int j = i; j < nwins - 1; j++) wins[j] = wins[j + 1];
                        nwins--;
                        break;
                    }
                    if (mx >= wdx + 150 && mx < wdx + 210 && my >= wdy + 54 && my < wdy + 78) {
                        w->app_state = 0;
                        break;
                    }
                    break;
                }

                /* Maximize button */
                if (win_max_hit(w, mx, my)) {
                    if (!w->maximized) {
                        w->prev_x = w->x; w->prev_y = w->y; w->prev_w = w->w; w->prev_h = w->h;
                        w->x = 0; w->y = 0; w->w = fw; w->h = fh - TASKBAR_H;
                        w->maximized = 1;
                    } else {
                        w->x = w->prev_x; w->y = w->prev_y; w->w = w->prev_w; w->h = w->prev_h;
                        w->maximized = 0;
                    }
                    win_raise(i);
                    break;
                }

                /* Minimize button */
                if (win_min_hit(w, mx, my)) {
                    w->minimized = 1;
                    if (w->app_id == APP_NOTEPAD) note_active = 0;
                    if (w->app_id == APP_TERMINAL) term_active = 0;
                    if (w->app_id == APP_CALC) calc_active = 0;
                    break;
                }

                /* Titlebar drag */
                if (my >= w->y + BORDER && my <= w->y + BORDER + TITLEBAR_H) {
                    drag_win = i;
                    drag_ox = mx - w->x;
                    drag_oy = my - w->y;
                    win_raise(i);
                    drag_win = nwins - 1;
                    if (wins[drag_win].app_id == APP_NOTEPAD) { note_active = 1; term_active = 0; calc_active = 0; }
                    if (wins[drag_win].app_id == APP_TERMINAL) { term_active = 1; note_active = 0; calc_active = 0; }
                    if (wins[drag_win].app_id == APP_CALC) { calc_active = 1; note_active = 0; term_active = 0; }
                    break;
                }

                /* Client area click */
                win_raise(i);
                struct Window* wt = &wins[nwins - 1];
                if (wt->app_id == APP_PAINT) {
                    paint_click(wt, mx, my);
                } else if (wt->app_id == APP_NOTEPAD) {
                    note_active = 1; term_active = 0; calc_active = 0;
                    notepad_click(wt, mx, my);
                } else if (wt->app_id == APP_CALC) {
                    calc_active = 1; note_active = 0; term_active = 0;
                    calc_click(wt, mx, my);
                } else if (wt->app_id == APP_TERMINAL) {
                    term_active = 1; note_active = 0; calc_active = 0;
                } else if (wt->app_id == APP_CTRLPANEL) {
                    ctrlpanel_click(wt, mx, my);
                } else if (wt->app_id == APP_EXPLORER) {
                    explorer_click(wt, mx, my);
                } else if (wt->app_id == APP_FILEPROPS) {
                    if (fileprops_click(wt, mx, my)) nwins--;
                } else if (wt->app_id == APP_SAVEAS) {
                    saveas_click(wt, mx, my);
                }
                break;
            }
        }

        if (!(mouse_btn & 1)) {
            drag_win = -1;
            resize_win = -1;
        }

        if (resize_win >= 0 && resize_win < nwins && (mouse_btn & 1)) {
            int nw = (mouse_x - wins[resize_win].x) + resize_ox;
            int nh = (mouse_y - wins[resize_win].y) + resize_oy;
            if (nw < 220) nw = 220;
            if (nh < 140) nh = 140;
            if (wins[resize_win].x + nw > fw) nw = fw - wins[resize_win].x;
            if (wins[resize_win].y + nh > fh - TASKBAR_H) nh = (fh - TASKBAR_H) - wins[resize_win].y;
            wins[resize_win].w = nw;
            wins[resize_win].h = nh;
        }

        if (drag_win >= 0 && drag_win < nwins && (mouse_btn & 1)) {
            if (!wins[drag_win].maximized) {
                wins[drag_win].x = mouse_x - drag_ox;
                wins[drag_win].y = mouse_y - drag_oy;
                if (wins[drag_win].x < 0) wins[drag_win].x = 0;
                if (wins[drag_win].y < 0) wins[drag_win].y = 0;
                if (wins[drag_win].x + wins[drag_win].w > fw) wins[drag_win].x = fw - wins[drag_win].w;
                if (wins[drag_win].y + wins[drag_win].h > fh - TASKBAR_H) wins[drag_win].y = fh - TASKBAR_H - wins[drag_win].h;
            }
        }

    input_done:
        /* Cursor pointer detection */
        {
            int lmx = mouse_x, lmy = mouse_y;
            cursor_is_link = 0;
            int ty2 = fh - TASKBAR_H;

            /* Taskbar */
            if (lmy > ty2 && lmy < fh && lmx > 0 && lmx < fw) cursor_is_link = 1;

            /* Desktop icons */
            if (!cursor_is_link && lmy < ty2) {
                for (int ci = 0; ci < NDESK_ICONS && !cursor_is_link; ci++) {
                    struct DesktopIcon* d = &desk_icons[ci];
                    int cx_ = d->x + 18;
                    if (lmx >= cx_ - 32 && lmx < cx_ + 32 && lmy >= d->y && lmy < d->y + 70) cursor_is_link = 1;
                }
            }

            /* Context menu */
            if (!cursor_is_link && ctx_open) {
                int cmw = 140, cmh = ctx_item_count * 20 + 4;
                int cpx = ctx_x, cpy = ctx_y;
                if (cpx + cmw > fw) cpx = fw - cmw - 2;
                if (cpy + cmh > fh - TASKBAR_H) cpy = fh - TASKBAR_H - cmh - 2;
                if (lmx >= cpx && lmx < cpx + cmw && lmy >= cpy && lmy < cpy + cmh) cursor_is_link = 1;
            }

            /* Window buttons */
            if (!cursor_is_link) {
                for (int wi = 0; wi < nwins && !cursor_is_link; wi++) {
                    if (!wins[wi].visible || wins[wi].minimized) continue;
                    struct Window* ww = &wins[wi];
                    if (win_close_hit(ww, lmx, lmy) || win_max_hit(ww, lmx, lmy) || win_min_hit(ww, lmx, lmy)) cursor_is_link = 1;
                }
            }

            /* Topmost window controls */
            if (!cursor_is_link && nwins > 0) {
                struct Window* wt = &wins[nwins - 1];
                if (wt->visible && !wt->minimized) {
                    if (wt->app_state == 99) {
                        int wdx = wt->x + wt->w / 2 - 110, wdy = wt->y + wt->h / 2 - 45;
                        if (lmx >= wdx + 10 && lmx < wdx + 210 && lmy >= wdy + 54 && lmy < wdy + 78) cursor_is_link = 1;
                    } else if (wt->app_id == APP_PAINT) {
                        cursor_is_link = paint_is_hover_link(wt, lmx, lmy);
                    } else if (wt->app_id == APP_NOTEPAD) {
                        cursor_is_link = notepad_is_hover_link(wt, lmx, lmy);
                    } else if (wt->app_id == APP_CALC) {
                        cursor_is_link = calc_is_hover_link(wt, lmx, lmy);
                    } else if (wt->app_id == APP_CTRLPANEL) {
                        cursor_is_link = ctrlpanel_is_hover_link(wt, lmx, lmy);
                    } else if (wt->app_id == APP_EXPLORER) {
                        cursor_is_link = explorer_is_hover_link(wt, lmx, lmy);
                    } else if (wt->app_id == APP_FILEPROPS) {
                        cursor_is_link = fileprops_is_hover_link(wt, lmx, lmy);
                    } else if (wt->app_id == APP_SAVEAS) {
                        cursor_is_link = saveas_is_hover_link(wt, lmx, lmy);
                    }
                }
            }

            /* Start menu hover */
            if (!cursor_is_link && start_menu) {
                int ty3 = fh - TASKBAR_H;
                int smh = MENU_N * MENU_IH + 6, smw = 146, smx = 2, smy = ty3 - smh;
                if (lmx >= smx + 20 && lmx < smx + smw && lmy >= smy && lmy < ty3) cursor_is_link = 1;
            }
        }

        draw_cursor(mouse_x, mouse_y);
        gui_flip();
        last_btn = mouse_btn;
        __asm__ volatile("hlt");
    }

    con_clear();
}

/* Console initialization & CLI */
void console_init(void) {
    drv_display_init();
    bi_fb = *(u32*)0x704;
    if (*(u16*)0x708 >= 640 && *(u16*)0x70A >= 480) {
        fw = *(u16*)0x708;
        fh = *(u16*)0x70A;
    }
    bi_pitch = *(u16*)0x70C;
    bi_bpp = *(u8*)0x70E;
    bi_vesa = *(u8*)0x70F;
    bi_font = *(u32*)0x710;

    if (bi_vesa && bi_bpp >= 24 && bi_fb >= 0xA0000 && bi_pitch >= 640) {
        use_fb = 1;
        fb = (u8*)(unsigned long)bi_fb;
        fpitch = bi_pitch;
        fbpp = bi_bpp;
        cols = fw / 8;
        rows = (fh - 16) / 16;
        if (cols > 160) cols = 160;
        if (rows > 80) rows = 80;
        COL_BG = 0x401010;
        COL_FG = 0xFFFFFF;
        COL_BAR = 0xA0A0A0;
    }
    con_clear();
}

void cmd_meminfo(void) {
    u32 cnt = *(u32*)0x71C;
    u8* m = (u8*)0x720;
    if (cnt > 32) cnt = 32;
    con_print("E820 regions: ");
    con_dec(cnt);
    con_char('\n');
    for (u32 i = 0; i < cnt; i++) {
        u8* e = m + i * 20;
        u32 bl = e[0] | e[1] << 8 | e[2] << 16 | e[3] << 24;
        u32 ll = e[8] | e[9] << 8 | e[10] << 16 | e[11] << 24;
        u32 ty = e[16] | e[17] << 8 | e[18] << 16 | e[19] << 24;
        con_print("  base=");
        const char* h = "0123456789ABCDEF";
        for (int q = 28; q >= 0; q -= 4) con_char(h[(bl >> q) & 15]);
        con_print(" len=");
        for (int q = 28; q >= 0; q -= 4) con_char(h[(ll >> q) & 15]);
        con_print(" type=");
        con_dec(ty);
        con_char('\n');
    }
}

void cmd_sysinfo(void) {
    char b[50];
    get_cpu_brand(b, 50);
    con_print("CPU: ");
    con_print(b);
    con_char('\n');
}

void cmd_vesainfo(void) {
    con_print("VESA / Framebuffer Status:\n");
    con_print("  Active GUI (use_fb): "); con_dec(use_fb); con_char('\n');
    con_print("  Hardware Driver    : "); con_print(drv_display_get_hw_name()); con_char('\n');
    con_print("  VESA flag  (0x70F) : "); con_dec(bi_vesa); con_char('\n');
    con_print("  Last Mode  (0x702) : 0x"); con_hex16(*(u16*)0x702); con_char('\n');
    con_print("  Resolution         : "); con_dec(*(u16*)0x708); con_print(" x "); con_dec(*(u16*)0x70A); con_char('\n');
    con_print("  Color Depth(0x70E) : "); con_dec(bi_bpp); con_print(" bpp\n");
    con_print("  Scan Pitch (0x70C) : "); con_dec(bi_pitch); con_print(" bytes\n");
    con_print("  PhysBasePtr(0x704) : 0x"); con_hex(bi_fb); con_char('\n');
    if (!bi_vesa) {
        con_print("  Reason: BIOS Set/Get VBE mode failed in bootloader.\n");
    } else if (bi_bpp < 24) {
        con_print("  Reason: Color depth < 24 bpp (unsupported).\n");
    } else if (bi_fb < 0xA0000) {
        con_print("  Reason: Invalid framebuffer base pointer (< 0xA0000).\n");
    } else if (bi_pitch < 640) {
        con_print("  Reason: Invalid pitch (< 640).\n");
    } else {
        con_print("  Status: Framebuffer valid and ready.\n");
    }
}

void cmd_ls(void) {
    u8* root = ramdisk + root_sect * 512;
    for (int i = 0; i < root_cnt; i++) {
        u8* e = root + i * 32;
        if (e[0] == 0) break;
        if (e[0] == 0xE5 || e[11] == 0x0F) continue;
        for (int j = 0; j < 8; j++) con_char(e[j]);
        con_char('.');
        for (int j = 8; j < 11; j++) con_char(e[j]);
        con_print("  ");
        u32 sz = e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24);
        char szb[16];
        format_size(szb, sz);
        con_print(szb);
        con_char('\n');
    }
}

void cmd_cat(const char* name) {
    char n83[11];
    fat_name83(name, n83);
    u8* root = ramdisk + root_sect * 512;
    int end_mark = is_fat16 ? 0xFFF8 : 0xFF8;

    for (int i = 0; i < root_cnt; i++) {
        u8* e = root + i * 32;
        if (e[0] == 0) break;
        int ok = 1;
        for (int j = 0; j < 11; j++) if (e[j] != n83[j]) ok = 0;
        if (!ok) continue;

        int sz = e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24);
        int cl = e[26] | (e[27] << 8);

        while (cl < end_mark && sz > 0) {
            int s = data_sect + cl - 2;
            if (s >= total_sectors) { con_print("(oor)\n"); return; }
            u8* p = ramdisk + s * 512;
            int n = sz > 512 ? 512 : sz;
            for (int k = 0; k < n; k++) con_char(p[k]);
            sz -= n;
            cl = fat_get(cl);
        }
        con_char('\n');
        return;
    }
    con_print("Not found\n");
}

void kmain(void) {
    console_init();
    interrupts_init();
    fat_init();

    if (fat_read_file("FONT.BIN", (u8*)sys_font, sizeof(struct LoadedFont)) > 0) {
        font_loaded = 1;
    }

    con_print("BrickOS booting...\n");
    if (use_fb) {
        boot_anim();
        run_gui();
    } else {
        con_print("WARNING: Falling back to text mode (VESA not active).\n");
        con_print("Type 'gui' or 'vesainfo' for details.\n");
    }

    con_print("brickos> ");
    char cmd[64];
    int len = 0;

    while (1) {
        char c = kbd_get();
        if (c == '\n') {
            cmd[len] = 0;
            con_char('\n');
            for (int ci = 0; ci < len; ci++) {
                if (cmd[ci] >= 'A' && cmd[ci] <= 'Z') cmd[ci] += 32;
            }
            if (!len) {}
            else if (!my_strcmp(cmd, "help")) con_print("help ls cat meminfo sysinfo vesainfo clear reboot shutdown gui\n");
            else if (!my_strcmp(cmd, "clear")) con_clear();
            else if (!my_strcmp(cmd, "ls")) cmd_ls();
            else if (!my_strcmp(cmd, "meminfo")) cmd_meminfo();
            else if (!my_strcmp(cmd, "sysinfo")) cmd_sysinfo();
            else if (!my_strcmp(cmd, "vesainfo")) cmd_vesainfo();
            else if (!my_strcmp(cmd, "reboot")) outb(0x64, 0xFE);
            else if (!my_strcmp(cmd, "shutdown") || !my_strcmp(cmd, "poweroff")) system_shutdown();
            else if (!my_strcmp(cmd, "gui")) {
                if (use_fb) {
                    run_gui();
                } else {
                    con_print("Error: Cannot start GUI! VESA framebuffer not active.\n");
                    cmd_vesainfo();
                }
            }
            else if (len > 4 && cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == ' ') cmd_cat(cmd + 4);
            else { con_print("Unknown: "); con_print(cmd); con_char('\n'); }
            len = 0;
            con_print("brickos> ");
        } else if (c == '\b') {
            if (len > 0) { len--; con_char('\b'); }
        } else if (len < 63) {
            cmd[len++] = c;
            con_char(c);
        }
    }
}