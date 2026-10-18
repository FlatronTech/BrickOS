#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* I/O port helpers */
static inline u8   inb(u16 p){u8 r;__asm__ volatile("inb %w1,%0":"=a"(r):"Nd"(p));return r;}
static inline u16  inw(u16 p){u16 r;__asm__ volatile("inw %w1,%0":"=a"(r):"Nd"(p));return r;}
static inline u32  inl(u16 p){u32 r;__asm__ volatile("inl %w1,%0":"=a"(r):"Nd"(p));return r;}
static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%w1"::"a"(v),"Nd"(p));}
static inline void outw(u16 p,u16 v){__asm__ volatile("outw %0,%w1"::"a"(v),"Nd"(p));}
static inline void outl(u16 p,u32 v){__asm__ volatile("outl %0,%w1"::"a"(v),"Nd"(p));}

/* String & memory utilities */
int  my_strcmp(const char* a, const char* b);
int  my_strlen(const char* s);
void my_memcpy(void* d, const void* s, int n);
void my_memset(void* d, u8 v, int n);
void my_strcpy(char* d, const char* s);
void my_strcat(char* d, const char* s);
void itoa(char* buf, u32 v);
void itoa_signed(char* buf, int v);
void format_size(char* buf, u32 sz);

/* System font */
struct LoadedFont {
    u8 height;
    u8 first_char;
    u8 last_char;
    u8 def_width;
    u8 width[256];
    u16 bitmap[256][16];
} __attribute__((packed));

extern struct LoadedFont* sys_font;
extern int font_loaded;
extern volatile u32 ticks;
extern int fw, fh;
extern u8* ramdisk;
extern int root_sect, root_cnt, is_fat16;

/* Special Key Constants for keyboard dispatch */
#define KEY_UP    0x11
#define KEY_DOWN  0x12
#define KEY_LEFT  0x13
#define KEY_RIGHT 0x14
#define KEY_HOME  0x15
#define KEY_END   0x16

/* Window manager constants */
#define MAX_WINS   16
#define TITLEBAR_H 20
#define BORDER      2
#define TASKBAR_H  26

/* App IDs */
#define APP_NONE      0
#define APP_SYSINFO   1
#define APP_EXPLORER  2
#define APP_WELCOME   3
#define APP_PAINT     4
#define APP_NOTEPAD   5
#define APP_TASKMGR   6
#define APP_TERMINAL  7
#define APP_CTRLPANEL 8
#define APP_FILEPROPS 9
#define APP_BMPVIEW   10
#define APP_SAVEAS    11
#define APP_CALC      12

struct Window {
    int x, y, w, h;
    int visible, minimized, maximized;
    int prev_x, prev_y, prev_w, prev_h;
    int app_id, app_state;
    char title[32];
};

extern struct Window wins[MAX_WINS];
extern int nwins;

/* Window management functions */
void win_raise(int i);
int  win_new(int app_id, int x, int y, int w, int h, const char* title);
int  win_find(int app_id);
int  win_client_x(struct Window* w);
int  win_client_y(struct Window* w);
int  win_client_w(struct Window* w);
int  win_client_h(struct Window* w);
void open_app(int app_id);

/* GUI rendering primitives */
void gui_pixel(int x, int y, u32 c);
void gui_rect(int x, int y, int w, int h, u32 c);
void gui_hline(int x, int y, int w, u32 c);
void gui_vline(int x, int y, int h, u32 c);
int  gui_char(char ch, int px, int py, u32 fg, u32 bg);
int  gui_str_width(const char* s);
void gui_print(const char* s, int px, int py, u32 fg);
void gui_print_bg(const char* s, int px, int py, u32 fg, u32 bg);
void gui_print_clipped(const char* s, int px, int py, u32 fg, int max_x);
void gui_btn(int x, int y, int w, int h, const char* label, int pressed);
void gui_flip(void);

/* FAT filesystem */
int  fat_read_file(const char* name, u8* dest, int max_sz);
int  fat_write_file(const char* name, const u8* data, int sz);
void fat_name83(const char* in, char* out);
int  fat_get(int e);

/* Hardware and power */
void cpuid(u32 ax, u32* a, u32* b, u32* c, u32* d);
void get_cpu_brand(char* out, int max_len);
void get_rtc_time(char* out);
void system_shutdown(void);
void vesa_live_switch(u16 mode);

/* VESA mode numbers */
#define VESA_800x600  0x4115
#define VESA_640x480  0x4112


/* Shared dialogs */
void open_saveas_dialog(int for_paint);
void open_bmp_viewer(const char* filename);
void open_file_props(const char* nm, u32 sz, const char* ty, int clust);
void open_file_in_notepad(const char* filename);

#endif
