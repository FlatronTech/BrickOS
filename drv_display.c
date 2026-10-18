#include "drv_display.h"

/* External references to kernel framebuffer globals */
extern u8* fb;
extern u8* backbuf;
extern int fw, fh, fpitch, fbpp;
extern u32 bi_fb;
extern u16 bi_pitch;
extern volatile int mouse_x, mouse_y;
extern struct Window wins[MAX_WINS];
extern int nwins;
extern u32 desktop_color;

/* PCI Configuration helpers */
static u32 pci_read_cfg(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 addr = 0x80000000 | ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) | (offset & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static void pci_write_cfg(u8 bus, u8 slot, u8 func, u8 offset, u32 val) {
    u32 addr = 0x80000000 | ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) | (offset & 0xFC);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

/* VMware SVGA-II Hardware Constants */
#define SVGA_INDEX_PORT         0
#define SVGA_VALUE_PORT         1
#define SVGA_REG_ID             0
#define SVGA_REG_ENABLE         1
#define SVGA_REG_WIDTH          2
#define SVGA_REG_HEIGHT         3
#define SVGA_REG_MAX_WIDTH      4
#define SVGA_REG_MAX_HEIGHT     5
#define SVGA_REG_DEPTH          6
#define SVGA_REG_BITS_PER_PIXEL 7
#define SVGA_REG_BYTES_PER_LINE 12
#define SVGA_REG_FB_START       13
#define SVGA_REG_FB_OFFSET      14
#define SVGA_REG_FB_SIZE        16

#define SVGA_ID_2               0x90000002
#define SVGA_ID_1               0x90000001
#define SVGA_ID_0               0x90000000

static inline void vmware_out(u16 io_base, u32 reg, u32 val) {
    outl(io_base + SVGA_INDEX_PORT, reg);
    outl(io_base + SVGA_VALUE_PORT, val);
}

static inline u32 vmware_in(u16 io_base, u32 reg) {
    outl(io_base + SVGA_INDEX_PORT, reg);
    return inl(io_base + SVGA_VALUE_PORT);
}

/* Bochs / QEMU BGA Constants */
#define BGA_INDEX_PORT          0x1CE
#define BGA_DATA_PORT           0x1CF
#define BGA_REG_ID              0
#define BGA_REG_XRES            1
#define BGA_REG_YRES            2
#define BGA_REG_BPP             3
#define BGA_REG_ENABLE          4
#define BGA_ENABLED             0x01
#define BGA_LFB_ENABLED         0x40

static inline void bga_out(u16 reg, u16 val) {
    outw(BGA_INDEX_PORT, reg);
    outw(BGA_DATA_PORT, val);
}

static inline u16 bga_in(u16 reg) {
    outw(BGA_INDEX_PORT, reg);
    return inw(BGA_DATA_PORT);
}

/* Driver state */
static int disp_hw_type = DRV_DISP_HW_VESA;
static u16 vmware_io = 0;
static u32 vmware_fb_phys = 0;

void drv_display_init(void) {
    disp_hw_type = DRV_DISP_HW_VESA;

    /* 1. Try to find VMware SVGA-II PCI device (Vendor 0x15AD, Device 0x0405) */
    for (u8 slot = 0; slot < 32; slot++) {
        u32 id = pci_read_cfg(0, slot, 0, 0);
        if ((id & 0xFFFF) == 0x15AD && ((id >> 16) & 0xFFFF) == 0x0405) {
            /* Enable I/O, Memory, and Bus Master in PCI Command Register */
            u32 cmd = pci_read_cfg(0, slot, 0, 0x04);
            cmd |= 0x07;
            pci_write_cfg(0, slot, 0, 0x04, cmd);

            u32 bar0 = pci_read_cfg(0, slot, 0, 0x10);
            u32 bar1 = pci_read_cfg(0, slot, 0, 0x14);
            vmware_io = (u16)(bar0 & ~3);
            vmware_fb_phys = bar1 & ~0xF;

            /* Version negotiation with VMware SVGA device */
            vmware_out(vmware_io, SVGA_REG_ID, SVGA_ID_2);
            if (vmware_in(vmware_io, SVGA_REG_ID) == SVGA_ID_2) {
                disp_hw_type = DRV_DISP_HW_VMWARE;
                return;
            }
            vmware_out(vmware_io, SVGA_REG_ID, SVGA_ID_1);
            if (vmware_in(vmware_io, SVGA_REG_ID) == SVGA_ID_1) {
                disp_hw_type = DRV_DISP_HW_VMWARE;
                return;
            }
        }
    }

    /* 2. If VMware SVGA not found, check Bochs / QEMU BGA */
    u16 bga_id = bga_in(BGA_REG_ID);
    if (bga_id >= 0xB0C0 && bga_id <= 0xB0C6) {
        disp_hw_type = DRV_DISP_HW_BGA;
        return;
    }

    disp_hw_type = DRV_DISP_HW_VESA;
}

int drv_display_get_hw_type(void) {
    return disp_hw_type;
}

const char* drv_display_get_hw_name(void) {
    if (disp_hw_type == DRV_DISP_HW_VMWARE) return "VMware SVGA-II Hardware";
    if (disp_hw_type == DRV_DISP_HW_BGA)    return "Bochs / QEMU BGA";
    return "Standard VESA VBE (BIOS LFB)";
}

int drv_display_get_width(void) {
    return fw;
}

int drv_display_get_height(void) {
    return fh;
}

int drv_display_get_bpp(void) {
    return fbpp;
}

int drv_display_get_pitch(void) {
    return fpitch;
}

u32 drv_display_get_fb_base(void) {
    return bi_fb;
}

u8* drv_display_get_backbuf(void) {
    return backbuf;
}

void drv_display_flip(void) {
    gui_flip();
}

int drv_display_set_resolution(int width, int height) {
    if (disp_hw_type == DRV_DISP_HW_VMWARE && vmware_io != 0) {
        /* VMware SVGA-II live mode change — zero reboots */
        vmware_out(vmware_io, SVGA_REG_ENABLE, 0);
        vmware_out(vmware_io, SVGA_REG_WIDTH, (u32)width);
        vmware_out(vmware_io, SVGA_REG_HEIGHT, (u32)height);
        vmware_out(vmware_io, SVGA_REG_BITS_PER_PIXEL, 32);
        vmware_out(vmware_io, SVGA_REG_ENABLE, 1);

        u32 bpl = vmware_in(vmware_io, SVGA_REG_BYTES_PER_LINE);
        u32 fbo = vmware_in(vmware_io, SVGA_REG_FB_OFFSET);
        if (bpl == 0) bpl = (u32)width * 4;

        fw = width;
        fh = height;
        fbpp = 32;
        fpitch = (int)bpl;
        fb = (u8*)(unsigned long)(vmware_fb_phys + fbo);
        bi_fb = (u32)(unsigned long)fb;
        bi_pitch = (u16)fpitch;

    } else if (disp_hw_type == DRV_DISP_HW_BGA) {
        /* Bochs / QEMU BGA live mode change — zero reboots */
        bga_out(BGA_REG_ENABLE, 0);
        bga_out(BGA_REG_XRES, (u16)width);
        bga_out(BGA_REG_YRES, (u16)height);
        bga_out(BGA_REG_BPP, 32);
        bga_out(BGA_REG_ENABLE, BGA_ENABLED | BGA_LFB_ENABLED);

        fw = width;
        fh = height;
        fbpp = 32;
        fpitch = width * 4;
        bi_pitch = (u16)fpitch;

    } else {
        /* Fallback: cannot change mode live on standard unaccelerated VESA without VM extensions */
        return 0;
    }

    /* Keep BIOS data area in sync (0x700..) */
    *(volatile u16*)0x708 = (u16)fw;
    *(volatile u16*)0x70A = (u16)fh;
    *(volatile u16*)0x70C = (u16)fpitch;
    *(volatile u8* )0x70E = (u8)fbpp;

    /* Clear the backbuffer to prevent ghost pixels */
    for (int i = 0; i < fw * fh * 4; i++) {
        backbuf[i] = 0;
    }

    /* Center mouse pointer */
    mouse_x = fw / 2;
    mouse_y = fh / 2;

    /* Clamp all windows within the new screen boundaries */
    for (int i = 0; i < nwins; i++) {
        struct Window* w = &wins[i];
        if (w->maximized) {
            w->x = 0;
            w->y = 0;
            w->w = fw;
            w->h = fh - TASKBAR_H;
        } else {
            if (w->w > fw) w->w = fw;
            if (w->h > fh - TASKBAR_H) w->h = fh - TASKBAR_H;
            if (w->x + w->w > fw) w->x = fw - w->w;
            if (w->x < 0) w->x = 0;
            if (w->y + w->h > fh - TASKBAR_H) w->y = fh - TASKBAR_H - w->h;
            if (w->y < 0) w->y = 0;
        }
    }

    /* Redraw background and flip to screen */
    gui_rect(0, 0, fw, fh, desktop_color);
    gui_flip();

    return 1;
}
