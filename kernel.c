#define VGA_MEMORY 0xB8000
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
static inline u8   inb(u16 p){u8 r;__asm__ volatile("inb %w1,%0":"=a"(r):"Nd"(p));return r;}
static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%w1"::"a"(v),"Nd"(p));}
static int  my_strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return(unsigned char)*a-(unsigned char)*b;}
static int  my_strlen(const char*s){int n=0;while(s[n])n++;return n;}
static void my_memcpy(void*d,const void*s,int n){u8*dd=(u8*)d;const u8*ss=(const u8*)s;while(n--)*dd++=*ss++;}
static void my_memset(void*d,u8 v,int n){u8*dd=(u8*)d;while(n--)*dd++=v;}
static void my_strcpy(char*d,const char*s){while((*d++=*s++));}
static void itoa(char*buf,u32 v){if(!v){buf[0]='0';buf[1]=0;return;}char t[12];int i=0;while(v){t[i++]='0'+v%10;v/=10;}int j=0;for(int k=i-1;k>=0;k--)buf[j++]=t[k];buf[j]=0;}
static void my_strcat(char*d,const char*s){while(*d)d++;while((*d++=*s++));}
static void format_size(char*buf,u32 sz){
    if(sz>=1048576){char t[12];itoa(t,sz/1048576);my_strcpy(buf,t);my_strcat(buf," MB");}
    else if(sz>=1024){char t[12];itoa(t,sz/1024);my_strcpy(buf,t);my_strcat(buf," KB");}
    else{char t[12];itoa(t,sz);my_strcpy(buf,t);my_strcat(buf," B");}
}
struct LoadedFont {
    u8 height;
    u8 first_char;
    u8 last_char;
    u8 def_width;
    u8 width[256];
    u16 bitmap[256][16];
} __attribute__((packed));
static struct LoadedFont* sys_font = (struct LoadedFont*)0x50000;
static int font_loaded = 0;
struct Window;
void open_bmp_viewer(const char*filename);
void open_file_props(const char*nm,u32 sz,const char*ty,int clust);
void open_file_in_notepad(const char*filename);
void open_saveas_dialog(int for_paint);
void note_save(void);
void save_paint_bmp(void);

/* ── BSS zero + entry ── */
extern u32 __bss_start,__bss_end;
void kmain(void);void early_idt(void);
void __attribute__((section(".text.start"),noreturn)) _start(void){
    u32*p=&__bss_start;while(p<&__bss_end)*p++=0;
    early_idt();kmain();for(;;)__asm__ volatile("hlt");
}

/* ── global framebuffer info ── */
u32 bi_fb;u16 bi_pitch;u8 bi_bpp,bi_vesa,bi_font_dummy;u32 bi_font;
int fw=800,fh=600;
u8*ramdisk=(u8*)0x800000;

/* ── IDT ── */
struct idt_e{u16 lo;u16 sel;u8 zero;u8 type;u16 hi;}__attribute__((packed));
struct idt_e idt[48];
struct{u16 lim;u32 base;}__attribute__((packed)) idtr;

volatile u32 ticks=0;
#define KBUF 256
volatile char kbuf[KBUF];volatile int khead=0,ktail=0;
volatile int esc_pressed=0;

void exc_handler(int v){
    __asm__ volatile("cli");
    u8*m=(u8*)VGA_MEMORY;const char*h="0123456789ABCDEF";
    m[0]='E';m[1]=0x4F;m[2]='X';m[3]=0x4F;m[4]='C';m[5]=0x4F;m[6]=' ';m[7]=0x4F;
    m[8]=h[(v>>4)&15];m[9]=0x4F;m[10]=h[v&15];m[11]=0x4F;
    for(;;)__asm__ volatile("hlt");
}
#define EXC(n) void __attribute__((naked)) exc##n(void){__asm__ volatile("pushl $" #n "\n movl %0,%%eax\n call *%%eax\n"::"i"(exc_handler));}
EXC(0)EXC(1)EXC(2)EXC(3)EXC(4)EXC(5)EXC(6)EXC(7)
EXC(8)EXC(9)EXC(10)EXC(11)EXC(12)EXC(13)EXC(14)EXC(15)
EXC(16)EXC(17)EXC(18)EXC(19)EXC(20)EXC(21)EXC(22)EXC(23)
EXC(24)EXC(25)EXC(26)EXC(27)EXC(28)EXC(29)EXC(30)EXC(31)
static u32 exctab[32]={(u32)(unsigned long)exc0,(u32)(unsigned long)exc1,(u32)(unsigned long)exc2,(u32)(unsigned long)exc3,(u32)(unsigned long)exc4,(u32)(unsigned long)exc5,(u32)(unsigned long)exc6,(u32)(unsigned long)exc7,(u32)(unsigned long)exc8,(u32)(unsigned long)exc9,(u32)(unsigned long)exc10,(u32)(unsigned long)exc11,(u32)(unsigned long)exc12,(u32)(unsigned long)exc13,(u32)(unsigned long)exc14,(u32)(unsigned long)exc15,(u32)(unsigned long)exc16,(u32)(unsigned long)exc17,(u32)(unsigned long)exc18,(u32)(unsigned long)exc19,(u32)(unsigned long)exc20,(u32)(unsigned long)exc21,(u32)(unsigned long)exc22,(u32)(unsigned long)exc23,(u32)(unsigned long)exc24,(u32)(unsigned long)exc25,(u32)(unsigned long)exc26,(u32)(unsigned long)exc27,(u32)(unsigned long)exc28,(u32)(unsigned long)exc29,(u32)(unsigned long)exc30,(u32)(unsigned long)exc31};
void idt_set(int n,u32 addr){idt[n].lo=addr&0xFFFF;idt[n].sel=0x08;idt[n].zero=0;idt[n].type=0x8E;idt[n].hi=addr>>16;}
void __attribute__((naked)) irq_ignore(void){__asm__ volatile("push %eax\n mov $0x20,%al\n out %al,$0x20\n out %al,$0xA0\n pop %eax\n iret\n");}
void early_idt(void){for(int i=0;i<48;i++)idt_set(i,(i<32)?exctab[i]:(u32)(unsigned long)irq_ignore);idtr.lim=sizeof(idt)-1;idtr.base=(u32)(unsigned long)idt;__asm__ volatile("lidt %0"::"m"(idtr));}

/* ── Keyboard ── */
void tick_handler(void){ticks++;}
static int shift_held=0;
static char sc2asc(int sc){
    static const char lo[]="1234567890-=";static const char hi[]="!@#$%^&*()_+";
    if(sc==0x2A||sc==0x36){shift_held=1;return 0;}
    if(sc==0x1C)return'\n';if(sc==0x0E)return'\b';if(sc==0x39)return' ';
    if(sc==0x34)return shift_held?'>':'.';if(sc==0x33)return shift_held?'<':',';
    if(sc==0x35)return shift_held?'?':'/';if(sc==0x27)return shift_held?':':';';
    if(sc==0x28)return shift_held?'"':'\'';
    if(sc>=2&&sc<=0x0D)return shift_held?hi[sc-2]:lo[sc-2];
    if(sc>=0x10&&sc<=0x19){char c="qwertyuiop"[sc-0x10];return shift_held?c-32:c;}
    if(sc>=0x1E&&sc<=0x26){char c="asdfghjkl"[sc-0x1E];return shift_held?c-32:c;}
    if(sc>=0x2C&&sc<=0x32){char c="zxcvbnm"[sc-0x2C];return shift_held?c-32:c;}
    return 0;
}
void kbd_handler(void){
    u8 sc=inb(0x60);
    if(sc&0x80){int s=sc&0x7F;if(s==0x2A||s==0x36)shift_held=0;return;}
    char c=sc2asc(sc);
    if(c&&((khead+1)&(KBUF-1))!=ktail){kbuf[khead]=c;khead=(khead+1)&(KBUF-1);}
    if(sc==1)esc_pressed=1;
}
void __attribute__((naked)) irq0_stub(void){__asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%eax\n out %%al,$0x20\n popa\n iret\n"::"i"(tick_handler));}
void __attribute__((naked)) irq1_stub(void){__asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%eax\n out %%al,$0x20\n popa\n iret\n"::"i"(kbd_handler));}

/* ── Mouse ── */
volatile int mouse_x=320,mouse_y=240,mouse_btn=0;
static int mouse_cycle=0;static u8 mouse_bytes[3];
static u32 mouse_last_tick=0;
void mouse_handler(void){
    u8 status=inb(0x64);
    while(status&1){
        u8 data=inb(0x60);
        if(status&0x20){
            if(mouse_cycle!=0&&(ticks-mouse_last_tick>5)){
                mouse_cycle=0;
            }
            mouse_last_tick=ticks;
            if(mouse_cycle==0){
                /* Byte 0 must have bit 3=1 and bits 6,7=0 (not overflow/error/ACK 0xFA) */
                if((data&0xC8)==0x08){
                    mouse_bytes[0]=data;
                    mouse_cycle=1;
                }
            }else if(mouse_cycle==1){
                mouse_bytes[1]=data;
                mouse_cycle=2;
            }else{
                mouse_bytes[2]=data;
                mouse_cycle=0;
                int rx=(int)mouse_bytes[1];
                int ry=(int)mouse_bytes[2];
                if(mouse_bytes[0]&0x10)rx-=256;
                if(mouse_bytes[0]&0x20)ry-=256;
                if(rx>=-100&&rx<=100&&ry>=-100&&ry<=100){
                    mouse_x+=rx;mouse_y-=ry;
                    if(mouse_x<0)mouse_x=0;if(mouse_x>=fw)mouse_x=fw-1;
                    if(mouse_y<0)mouse_y=0;if(mouse_y>=fh)mouse_y=fh-1;
                    mouse_btn=mouse_bytes[0]&0x07;
                }
            }
        }
        status=inb(0x64);
    }
}
void __attribute__((naked)) irq12_stub(void){__asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%al\n out %%al,$0xA0\n out %%al,$0x20\n popa\n iret\n"::"i"(mouse_handler));}
void mouse_wait(u8 t){u32 to=100000;if(t==0){while(to--){if(inb(0x64)&1)return;}}else{while(to--){if(!(inb(0x64)&2))return;}}}
void mouse_write(u8 v){mouse_wait(1);outb(0x64,0xD4);mouse_wait(1);outb(0x60,v);}
u8 mouse_read(void){u32 to=100000;while(to--){u8 st=inb(0x64);if((st&0x21)==0x21)return inb(0x60);}return 0;}
void mouse_init(void){
    u32 to=10000;while(to--){if(inb(0x64)&1)inb(0x60);}
    mouse_wait(1);outb(0x64,0xA8);
    mouse_wait(1);outb(0x64,0x20);mouse_wait(0);u8 s=inb(0x60);
    s|=2;s&=~0x20;s|=1;mouse_wait(1);outb(0x64,0x60);mouse_wait(1);outb(0x60,s);
    mouse_write(0xF6);mouse_read();
    mouse_write(0xF4);mouse_read();
    to=10000;while(to--){if(inb(0x64)&1)inb(0x60);}
    mouse_cycle=0;mouse_btn=0;
    mouse_x=fw/2;mouse_y=fh/2;
}

/* ── PIC/PIT init ── */
void interrupts_init(void){
    outb(0x20,0x11);outb(0xA0,0x11);outb(0x21,0x20);outb(0xA1,0x28);
    outb(0x21,0x04);outb(0xA1,0x02);outb(0x21,0x01);outb(0xA1,0x01);
    idt_set(32,(u32)(unsigned long)irq0_stub);idt_set(33,(u32)(unsigned long)irq1_stub);idt_set(44,(u32)(unsigned long)irq12_stub);
    outb(0x43,0x36);outb(0x40,11932&0xFF);outb(0x40,11932>>8);
    mouse_init();
    outb(0x21,~((1<<0)|(1<<1)|(1<<2)));outb(0xA1,~(1<<4));
    __asm__ volatile("sti");
}

/* ── Console (fallback text mode) ── */
u8*fb;int fpitch,fbpp;
int cols=80,rows=25,cx=0,cy=0,lastcx=-1,lastcy=-1;
u8*tvga=(u8*)VGA_MEMORY;
u32 COL_BG,COL_FG,COL_BAR;
char shadow[160*80];
int use_fb=0;
void con_char(char c);void con_print(const char*s);void con_dec(u32 v);
void fb_pixel(int x,int y,u32 c){u8*p=fb+y*fpitch+x*(fbpp/8);p[0]=c&0xFF;p[1]=(c>>8)&0xFF;p[2]=(c>>16)&0xFF;if(fbpp==32)p[3]=0;}
void fb_rect(int x,int y,int w,int h,u32 c){for(int j=0;j<h;j++)for(int i=0;i<w;i++)fb_pixel(x+i,y+j,c);}
void fb_glyph(char ch,int gx,int gy,u32 fg){
    u8 uc=(u8)ch;
    if(font_loaded){
        int w=sys_font->width[uc];if(w==0)w=8;int h=sys_font->height;
        for(int r=0;r<h&&r<16;r++){u16 b=sys_font->bitmap[uc][r];for(int cbit=0;cbit<8&&cbit<w;cbit++)if(b&(0x8000>>cbit))fb_pixel(gx*8+cbit,gy*16+r,fg);}
    }else if(bi_font){
        const u8*f=(const u8*)(unsigned long)bi_font+uc*16;
        for(int r=0;r<16;r++){u8 b=f[r];for(int cbit=0;cbit<8;cbit++)if(b&(0x80>>cbit))fb_pixel(gx*8+cbit,gy*16+r,fg);}
    }
}
void fb_putcell(int x,int r,char ch){fb_rect(x*8,(r+1)*16,8,16,COL_BG);if(ch!=' ')fb_glyph(ch,x,r+1,COL_FG);}
void hw_cursor(void){int pos=cy*cols+cx;outb(0x3D4,0x0E);outb(0x3D5,(pos>>8)&0xFF);outb(0x3D4,0x0F);outb(0x3D5,pos&0xFF);}
void fb_cursor(void){if(lastcx>=0)fb_putcell(lastcx,lastcy,shadow[lastcy*cols+lastcx]);fb_rect(cx*8,(cy+1)*16+13,8,3,COL_FG);lastcx=cx;lastcy=cy;}
void con_cursor(void){if(use_fb)fb_cursor();else hw_cursor();}
void render_all(void){if(use_fb){fb_rect(0,16,fw,fh-16,COL_BG);for(int r=0;r<rows;r++)for(int x=0;x<cols;x++)if(shadow[r*cols+x]!=' ')fb_putcell(x,r,shadow[r*cols+x]);}else{for(int i=0;i<cols*rows;i++){tvga[i*2]=shadow[i];tvga[i*2+1]=0x0F;}}}
void con_scroll(void){for(int i=0;i<cols*(rows-1);i++)shadow[i]=shadow[i+cols];for(int i=cols*(rows-1);i<cols*rows;i++)shadow[i]=' ';cy=rows-1;render_all();}
void con_clear(void){for(int i=0;i<cols*rows;i++)shadow[i]=' ';cx=0;cy=0;lastcx=-1;lastcy=-1;if(use_fb){fb_rect(0,0,fw,fh,COL_BG);fb_rect(0,0,fw,16,COL_BAR);const char*t="BrickOS 1.0";for(int i=0;t[i];i++)fb_glyph(t[i],2+i,0,0x000000);}render_all();con_cursor();}
void con_char(char c){if(c=='\n'){cx=0;cy++;}else if(c=='\b'){if(cx>0){cx--;shadow[cy*cols+cx]=' ';if(use_fb)fb_putcell(cx,cy,' ');else tvga[(cy*cols+cx)*2]=' ';}}else{shadow[cy*cols+cx]=c;if(use_fb)fb_putcell(cx,cy,c);else{tvga[(cy*cols+cx)*2]=c;tvga[(cy*cols+cx)*2+1]=0x0F;}cx++;}if(cx>=cols){cx=0;cy++;}if(cy>=rows)con_scroll();con_cursor();}
void con_print(const char*s){while(*s)con_char(*s++);}
void con_dec(u32 v){char b[12];int i=0;if(!v){con_char('0');return;}while(v){b[i++]='0'+v%10;v/=10;}while(i--)con_char(b[i]);}
void con_hex(u32 v){const char*h="0123456789ABCDEF";for(int q=28;q>=0;q-=4)con_char(h[(v>>q)&15]);}
void con_hex16(u16 v){const char*h="0123456789ABCDEF";for(int q=12;q>=0;q-=4)con_char(h[(v>>q)&15]);}
char kbd_get(void){while(khead==ktail)__asm__ volatile("hlt");char c=kbuf[ktail];ktail=(ktail+1)&(KBUF-1);return c;}
char kbd_try(void){if(khead==ktail)return 0;char c=kbuf[ktail];ktail=(ktail+1)&(KBUF-1);return c;}

/* ── FAT12 & FAT16 Driver ── */
u16 rd16(int o){return ramdisk[o]|(ramdisk[o+1]<<8);}
int fat_sect,root_sect,data_sect,root_cnt,total_sectors=2880,is_fat16=0;
u8*fatp;
void fat_init(void){
    int res=rd16(14),nf=ramdisk[16];
    root_cnt=rd16(17);
    fat_sect=res;
    int spf=rd16(22);if(spf==0)spf=rd16(36);
    root_sect=res+nf*spf;
    data_sect=root_sect+(root_cnt*32+511)/512;
    fatp=ramdisk+fat_sect*512;
    total_sectors=rd16(19);
    if(!total_sectors)total_sectors=*(u32*)&ramdisk[32];
    if(!total_sectors)total_sectors=2880;
    if(ramdisk[54]=='F'&&ramdisk[57]=='1'&&ramdisk[58]=='6')is_fat16=1;
    else is_fat16=0;
}
int fat_get(int e){
    if(is_fat16)return ((u16*)fatp)[e];
    int i=e+e/2;int w=fatp[i]|(fatp[i+1]<<8);return(e&1)?w>>4:w&0xFFF;
}
void fat_name83(const char*in,char*out){
    for(int i=0;i<11;i++)out[i]=' ';
    int i;for(i=0;in[i]&&in[i]!='.'&&i<8;i++)out[i]=(in[i]>='a'&&in[i]<='z')?in[i]-32:in[i];
    const char*d=in;while(*d&&*d!='.')d++;
    if(*d=='.')for(i=0;d[1+i]&&i<3;i++)out[8+i]=(d[1+i]>='a'&&d[1+i]<='z')?d[1+i]-32:d[1+i];
}
int fat_read_file(const char*name,u8*dest,int max_sz){
    char n83[11];fat_name83(name,n83);
    u8*root=ramdisk+root_sect*512;
    int end_mark=is_fat16?0xFFF8:0xFF8;
    for(int i=0;i<root_cnt;i++){
        u8*e=root+i*32;if(e[0]==0)break;
        int ok=1;for(int j=0;j<11;j++)if(e[j]!=n83[j])ok=0;if(!ok)continue;
        int sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
        int cl=e[26]|(e[27]<<8);
        int total=0;
        while(cl<end_mark&&total<sz&&total<max_sz){
            int s=data_sect+cl-2;if(s>=total_sectors)break;
            u8*p=ramdisk+s*512;int n=sz-total;if(n>512)n=512;
            if(total+n>max_sz)n=max_sz-total;
            for(int k=0;k<n;k++)dest[total+k]=p[k];
            total+=n;cl=fat_get(cl);
        }
        return total;
    }
    return -1;
}
void fat_set_entry(int e, int val) {
    if(is_fat16){((u16*)fatp)[e]=(u16)val;return;}
    int i = e + e/2;
    int w = fatp[i] | (fatp[i+1] << 8);
    if (e & 1) w = (w & 0x000F) | (val << 4);
    else w = (w & 0xF000) | (val & 0xFFF);
    fatp[i] = w & 0xFF;
    fatp[i+1] = (w >> 8) & 0xFF;
}
int fat_alloc_cluster(void) {
    int max_cl=is_fat16?65000:(total_sectors-data_sect+2);
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
        if (e[0] == 0 || e[0] == 0xE5) {
            if (slot < 0) slot = i;
            if (e[0] == 0) break;
        } else {
            int ok = 1;
            for (int j = 0; j < 11; j++) if (e[j] != n83[j]) ok = 0;
            if (ok) { slot = i; break; }
        }
    }
    if (slot < 0) return -1;
    int clusters = (sz + 511) / 512;
    if (clusters == 0) clusters = 1;
    int first_cl = -1, prev_cl = -1;
    int eoc_val = is_fat16 ? 0xFFFF : 0xFFF;
    for (int i = 0; i < clusters; i++) {
        int cl = fat_alloc_cluster();
        if (cl < 0) return -1;
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
    u8* e = root + slot * 32;
    for (int j = 0; j < 11; j++) e[j] = n83[j];
    e[11] = 0x20;
    e[26] = first_cl & 0xFF; e[27] = (first_cl >> 8) & 0xFF;
    e[28] = sz & 0xFF; e[29] = (sz >> 8) & 0xFF; e[30] = (sz >> 16) & 0xFF; e[31] = (sz >> 24) & 0xFF;
    return 0;
}
void cmd_cat(const char*name){
    char n83[11];fat_name83(name,n83);u8*root=ramdisk+root_sect*512;
    int end_mark=is_fat16?0xFFF8:0xFF8;
    for(int i=0;i<root_cnt;i++){
        u8*e=root+i*32;if(e[0]==0)break;
        int ok=1;for(int j=0;j<11;j++)if(e[j]!=n83[j])ok=0;if(!ok)continue;
        int sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
        int cl=e[26]|(e[27]<<8);
        while(cl<end_mark&&sz>0){
            int s=data_sect+cl-2;
            if(s>=total_sectors){con_print("(oor)\n");return;}
            u8*p=ramdisk+s*512;int n=sz>512?512:sz;
            for(int k=0;k<n;k++)con_char(p[k]);
            sz-=n;cl=fat_get(cl);
        }
        con_char('\n');return;
    }
    con_print("Not found\n");
}
void cmd_ls(void){
    u8*root=ramdisk+root_sect*512;
    for(int i=0;i<root_cnt;i++){
        u8*e=root+i*32;if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;
        for(int j=0;j<8;j++)con_char(e[j]);
        con_char('.');
        for(int j=8;j<11;j++)con_char(e[j]);
        con_print("  ");
        u32 sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
        char szb[16];format_size(szb,sz);
        con_print(szb);con_char('\n');
    }
}

/* ── CPUID & Hardware Info ── */
void cpuid(u32 ax,u32*a,u32*b,u32*c,u32*d){__asm__ volatile("push %%ebx\ncpuid\nmov %%ebx,%%esi\npop %%ebx":"=a"(*a),"=S"(*b),"=c"(*c),"=d"(*d):"a"(ax));}
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

/* ══════════════════════════════════════
   GUI ENGINE
   ══════════════════════════════════════ */

u8*backbuf=(u8*)0x200000;
void gui_pixel(int x,int y,u32 c){if(x<0||x>=fw||y<0||y>=fh)return;u8*p=backbuf+y*fpitch+x*(fbpp/8);p[0]=c&0xFF;p[1]=(c>>8)&0xFF;p[2]=(c>>16)&0xFF;if(fbpp==32)p[3]=0;}
void gui_rect(int x,int y,int w,int h,u32 c){for(int j=0;j<h;j++)for(int i=0;i<w;i++)gui_pixel(x+i,y+j,c);}
void gui_hline(int x,int y,int w,u32 c){for(int i=0;i<w;i++)gui_pixel(x+i,y,c);}
void gui_vline(int x,int y,int h,u32 c){for(int j=0;j<h;j++)gui_pixel(x,y+j,c);}

int gui_char(char ch,int px,int py,u32 fg,u32 bg){
    u8 uc=(u8)ch;
    if(font_loaded){
        int w=sys_font->width[uc];if(w==0)w=sys_font->def_width?sys_font->def_width:8;
        int h=sys_font->height;
        if(bg!=0xFFFFFFFF)gui_rect(px,py,w+1,h,bg);
        for(int r=0;r<h;r++){u16 b=sys_font->bitmap[uc][r];for(int cbit=0;cbit<w;cbit++)if(b&(0x8000>>cbit))gui_pixel(px+cbit,py+r,fg);}
        return w;
    }else if(bi_font){
        const u8*f=(const u8*)(unsigned long)bi_font+uc*16;
        if(bg!=0xFFFFFFFF)gui_rect(px,py,9,16,bg);
        for(int r=0;r<16;r++){u8 b=f[r];for(int cbit=0;cbit<8;cbit++)if(b&(0x80>>cbit))gui_pixel(px+cbit,py+r,fg);}
        return 8;
    }
    return 8;
}
int gui_str_width(const char*s){
    int w=0;
    while(*s){
        if(*s!='\n'){
            int cw=8;
            if(font_loaded){cw=sys_font->width[(u8)*s];if(cw==0)cw=sys_font->def_width?sys_font->def_width:8;}
            w+=cw+1;
        }
        s++;
    }
    return w;
}
void gui_print(const char*s,int px,int py,u32 fg){
    int cx=px;int lh=font_loaded?sys_font->height+4:16;
    while(*s){if(*s=='\n'){py+=lh;cx=px;}else{int cw=gui_char(*s,cx,py,fg,0xFFFFFFFF);cx+=cw+1;}s++;}
}
void gui_print_bg(const char*s,int px,int py,u32 fg,u32 bg){
    int cx=px;int lh=font_loaded?sys_font->height+4:16;
    while(*s){if(*s=='\n'){py+=lh;cx=px;}else{int cw=gui_char(*s,cx,py,fg,bg);cx+=cw+1;}s++;}
}
/* Print string clipped to max_x (won't draw past that x coordinate) */
void gui_print_clipped(const char*s,int px,int py,u32 fg,int max_x){
    int cx=px;
    while(*s){
        int cw=8;
        if(font_loaded){cw=sys_font->width[(u8)*s];if(cw==0)cw=sys_font->def_width?sys_font->def_width:8;}
        if(cx+cw>max_x)break;
        gui_char(*s,cx,py,fg,0xFFFFFFFF);
        cx+=cw+1;
        s++;
    }
}

void gui_char_big(char ch,int px,int py,u32 fg,int sc){
    u8 uc=(u8)ch;
    if(font_loaded){
        int w=sys_font->width[uc];if(w==0)w=8;int h=sys_font->height;
        for(int r=0;r<h;r++){u16 b=sys_font->bitmap[uc][r];for(int c=0;c<w;c++)if(b&(0x8000>>c))gui_rect(px+c*sc,py+r*sc,sc,sc,fg);}
    }else if(bi_font){
        const u8*f=(const u8*)(unsigned long)bi_font+uc*16;
        for(int r=0;r<16;r++){u8 b=f[r];for(int c=0;c<8;c++)if(b&(0x80>>c))gui_rect(px+c*sc,py+r*sc,sc,sc,fg);}
    }
}
void gui_str_big(const char*s,int px,int py,u32 fg,int sc){
    int cx=px;
    while(*s){gui_char_big(*s,cx,py,fg,sc);int cw=8;if(font_loaded){cw=sys_font->width[(u8)*s];if(cw==0)cw=8;}cx+=(cw+1)*sc;s++;}
}

void gui_flip(void){u32*s=(u32*)backbuf;u32*d=(u32*)fb;if(fbpp==32&&fpitch==fw*4){int n=fw*fh;for(int i=0;i<n;i++)d[i]=s[i];}else{for(int y=0;y<fh;y++){u8*src=backbuf+y*fpitch;u8*dst=fb+y*fpitch;for(int x=0;x<fw*(fbpp/8);x++)dst[x]=src[x];}}}

static const char*cursor_sprite[16]={"*               ","**              ","*.*             ","*..*            ","*...*           ","*....*          ","*.....*         ","*......*        ","*.......*       ","*.....****      ","*..*..*         ","*.* .*..*       ","**   *..*       ","*     *..*      ","       **       ","                "};
/* Hand/pointer cursor for links (Win98 style pointing hand) */
static const char*cursor_hand[16]={"   *            ","  *.*           ","  *.*           ","  *.*           ","  *.***         ","  *.*..*..      "," **.*...*..     ","*.*..........   ","*............   ","*.............  ","*.............  "," *............  "," *............  ","  *...........  ","  *..........   ","   **********   "};
void draw_cursor_normal(int mx,int my){for(int y=0;y<16;y++)for(int x=0;x<16;x++){char c=cursor_sprite[y][x];if(c=='*')gui_pixel(mx+x,my+y,0x000000);else if(c=='.')gui_pixel(mx+x,my+y,0xFFFFFF);}}
void draw_cursor_hand(int mx,int my){for(int y=0;y<16;y++)for(int x=0;x<16;x++){char c=cursor_hand[y][x];if(c=='*')gui_pixel(mx+x,my+y,0x000000);else if(c=='.')gui_pixel(mx+x,my+y,0xFFFFFF);}}
/* Global: is cursor on clickable item this frame? */
static int cursor_is_link=0;
void draw_cursor(int mx,int my){if(cursor_is_link)draw_cursor_hand(mx,my);else draw_cursor_normal(mx,my);}

/* ══════════════════════════════════════
   BOOT ANIMATION
   ══════════════════════════════════════ */
void boot_anim(void){
    gui_rect(0,0,fw,fh,0x000000);
    /* Stars */
    u32 seed=12345;
    for(int i=0;i<80;i++){seed=seed*1664525+1013904223;int sx=(seed>>16)%fw;int sy=(seed>>8)%((fh*3)/4);u8 br=((seed>>24)&3)*60+60;gui_pixel(sx,sy,(br<<16)|(br<<8)|br);}
    /* Logo "BrickOS" at 3x */
    const char*logo="BrickOS";int lw=gui_str_width(logo)*3;int lx=(fw-lw)/2,ly=fh/2-80;
    gui_str_big(logo,lx,ly,0x4488FF,3);
    /* Subtitle */
    const char*sub="Operating System 1.0";int sl=gui_str_width(sub);
    gui_print(sub,(fw-sl)/2,ly+55,0x8899CC);
    const char*ft="FlatronTech - 2026";int fl=gui_str_width(ft);
    gui_print(ft,(fw-fl)/2,ly+73,0x505070);
    /* Progress bar frame */
    int bx=(fw-280)/2,by=ly+100,bh=12;
    gui_rect(bx-2,by-2,284,16,0x303050);
    gui_rect(bx,by,280,bh,0x101020);
    gui_flip();
    u32 t=ticks;
    for(int pct=0;pct<=100;pct+=2){
        while(ticks<t+2)__asm__ volatile("hlt");t=ticks;
        u32 r=0x20+(pct*2);u32 g=0x60+(pct);u32 bc=(r<<16)|(g<<8)|0xFF;
        gui_rect(bx,by,280*pct/100,bh,bc);
        /* pct text */
        char pb[5];itoa(pb,(u32)pct);int pl=my_strlen(pb);
        gui_rect((fw-32)/2,by+16,32,14,0x000000);
        gui_print(pb,(fw-pl*8)/2,by+16,0x8888AA);
        gui_flip();
    }
    /* Hold 0.6s */
    t=ticks;while(ticks<t+60)__asm__ volatile("hlt");
}

/* ══════════════════════════════════════
   WINDOW MANAGER
   ══════════════════════════════════════ */
#define MAX_WINS   12
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

struct Window{
    int x,y,w,h;
    int visible,minimized,maximized;
    int prev_x,prev_y,prev_w,prev_h;
    int app_id,app_state;
    char title[32];
};
struct Window wins[MAX_WINS];
int nwins=0;
int drag_win=-1,drag_ox=0,drag_oy=0;

void win_raise(int i){if(i==nwins-1)return;struct Window tmp=wins[i];wins[i]=wins[nwins-1];wins[nwins-1]=tmp;}
int win_new(int app_id,int x,int y,int w,int h,const char*title){
    if(nwins>=MAX_WINS)return -1;
    int i=nwins++;
    wins[i].x=x;wins[i].y=y;wins[i].w=w;wins[i].h=h;
    wins[i].visible=1;wins[i].minimized=0;wins[i].maximized=0;
    wins[i].prev_x=x;wins[i].prev_y=y;wins[i].prev_w=w;wins[i].prev_h=h;
    wins[i].app_id=app_id;wins[i].app_state=0;
    int ti=0;while(title[ti]&&ti<31){wins[i].title[ti]=title[ti];ti++;}wins[i].title[ti]=0;
    return i;
}
int win_client_x(struct Window*w){return w->x+BORDER+2;}
int win_client_y(struct Window*w){return w->y+BORDER+TITLEBAR_H+2;}
int win_client_w(struct Window*w){return w->w-BORDER*2-4;}
int win_client_h(struct Window*w){return w->h-BORDER-TITLEBAR_H-4;}

/* ── 3D button helper ── */
void gui_btn(int x,int y,int w,int h,const char*label,int pressed){
    gui_rect(x,y,w,h,0xC0C0C0);
    if(pressed){gui_hline(x,y,w,0x808080);gui_vline(x,y,h,0x808080);gui_hline(x,y+h-1,w,0xFFFFFF);gui_vline(x+w-1,y,h,0xFFFFFF);}
    else{gui_hline(x,y,w,0xFFFFFF);gui_vline(x,y,h,0xFFFFFF);gui_hline(x,y+h-1,w,0x808080);gui_vline(x+w-1,y,h,0x808080);}
    int ll=gui_str_width(label);int fh_b=font_loaded?sys_font->height:16;gui_print(label,x+(w-ll)/2,y+(h-fh_b)/2,0x000000);
}

/* ── Window chrome ── */
void draw_win_chrome(struct Window*w,int focused){
    gui_rect(w->x,w->y,w->w,w->h,0xC0C0C0);
    gui_hline(w->x,w->y,w->w,0xFFFFFF);gui_vline(w->x,w->y,w->h,0xFFFFFF);
    gui_hline(w->x,w->y+w->h-1,w->w,0x000000);gui_vline(w->x+w->w-1,w->y,w->h,0x000000);
    gui_hline(w->x+1,w->y+1,w->w-2,0xDFDFDF);gui_vline(w->x+1,w->y+1,w->h-2,0xDFDFDF);
    gui_hline(w->x+1,w->y+w->h-2,w->w-2,0x808080);gui_vline(w->x+w->w-2,w->y+1,w->h-2,0x808080);
    /* Titlebar */
    u32 tbcol=focused?0x000080:0x808080;
    gui_rect(w->x+BORDER,w->y+BORDER,w->w-BORDER*2,TITLEBAR_H,tbcol);
    /* Title clipped so it doesn't overrun the buttons area (3 buttons * 17px + 4px padding = 55px from right) */
    int title_max_x=w->x+w->w-BORDER-56;
    gui_print_clipped(w->title,w->x+BORDER+4,w->y+BORDER+2,0xFFFFFF,title_max_x);
    /* Buttons: X, Max, Min  (14px wide each, 3px gap) */
    int by_=w->y+BORDER+3;
    /* Close */
    int cbx=w->x+w->w-BORDER-18;
    gui_rect(cbx,by_,14,14,0xC0C0C0);
    gui_hline(cbx,by_,14,0xFFFFFF);gui_vline(cbx,by_,14,0xFFFFFF);
    gui_hline(cbx,by_+13,14,0x000000);gui_vline(cbx+13,by_,14,0x000000);
    gui_print("X",cbx+3,by_,0x000000);
    /* Maximize */
    int mxbx=cbx-17;
    gui_rect(mxbx,by_,14,14,0xC0C0C0);
    gui_hline(mxbx,by_,14,0xFFFFFF);gui_vline(mxbx,by_,14,0xFFFFFF);
    gui_hline(mxbx,by_+13,14,0x000000);gui_vline(mxbx+13,by_,14,0x000000);
    if(w->maximized){gui_print("2",mxbx+3,by_,0x000000);}
    else{gui_hline(mxbx+2,by_+3,10,0x000000);gui_hline(mxbx+2,by_+4,10,0x000000);gui_hline(mxbx+2,by_+11,10,0x000000);gui_vline(mxbx+2,by_+3,9,0x000000);gui_vline(mxbx+11,by_+3,9,0x000000);}
    /* Minimize */
    int mnbx=mxbx-17;
    gui_rect(mnbx,by_,14,14,0xC0C0C0);
    gui_hline(mnbx,by_,14,0xFFFFFF);gui_vline(mnbx,by_,14,0xFFFFFF);
    gui_hline(mnbx,by_+13,14,0x000000);gui_vline(mnbx+13,by_,14,0x000000);
    gui_hline(mnbx+2,by_+9,10,0x000000);gui_hline(mnbx+2,by_+10,10,0x000000);
    /* Client area */
    int cyo=w->y+BORDER+TITLEBAR_H,cw=w->w-BORDER*2,ch=w->h-BORDER-TITLEBAR_H;
    gui_rect(w->x+BORDER,cyo,cw,ch,0xFFFFFF);
    gui_hline(w->x+BORDER,cyo,cw,0x808080);gui_vline(w->x+BORDER,cyo,ch,0x808080);
    gui_hline(w->x+BORDER,cyo+ch-1,cw,0xFFFFFF);gui_vline(w->x+BORDER+cw-1,cyo,ch,0xFFFFFF);
    /* Resize handle */
    if(!w->maximized){
        int gx=w->x+w->w-12,gy=w->y+w->h-12;
        for(int g=0;g<3;g++){
            int off=g*3;
            gui_hline(gx+6-off,gy+8,off+2,0xFFFFFF);
            gui_hline(gx+6-off,gy+7,off+2,0x808080);
        }
    }
    /* Modal warning dialog for unsaved changes */
    if(w->app_state==99){
        int wdx=w->x+w->w/2-110,wdy=w->y+w->h/2-45;
        gui_rect(wdx,wdy,220,90,0xC0C0C0);
        gui_hline(wdx,wdy,220,0xFFFFFF);gui_vline(wdx,wdy,90,0xFFFFFF);
        gui_hline(wdx,wdy+89,220,0x000000);gui_vline(wdx+219,wdy,90,0x000000);
        gui_hline(wdx+1,wdy+1,218,0xDFDFDF);gui_vline(wdx+1,wdy+1,88,0xDFDFDF);
        gui_hline(wdx+1,wdy+88,218,0x808080);gui_vline(wdx+218,wdy+1,88,0x808080);
        gui_rect(wdx+2,wdy+2,216,18,0x000080);
        gui_print("Warning",wdx+6,wdy+3,0xFFFFFF);
        gui_print("Save changes before closing?",wdx+12,wdy+26,0x000000);
        gui_btn(wdx+12,wdy+54,58,22,"Save",0);
        gui_btn(wdx+78,wdy+54,62,22,"Discard",0);
        gui_btn(wdx+148,wdy+54,58,22,"Cancel",0);
    }
}
/* hit-test helpers for buttons */
int win_close_hit(struct Window*w,int mx,int my){int cbx=w->x+w->w-BORDER-18,by_=w->y+BORDER+3;return mx>=cbx&&mx<cbx+14&&my>=by_&&my<by_+14;}
int win_max_hit(struct Window*w,int mx,int my){int cbx=w->x+w->w-BORDER-35,by_=w->y+BORDER+3;return mx>=cbx&&mx<cbx+14&&my>=by_&&my<by_+14;}
int win_min_hit(struct Window*w,int mx,int my){int cbx=w->x+w->w-BORDER-52,by_=w->y+BORDER+3;return mx>=cbx&&mx<cbx+14&&my>=by_&&my<by_+14;}

/* ══════════════════════════════════════
   PAINT APP (Dynamic full-window canvas)
   ══════════════════════════════════════ */
#define MAX_CANVAS_W 800
#define MAX_CANVAS_H 600
#define PAINT_CANVAS ((u32*)0x600000)
static u32 paint_color=0x000000;
static int paint_dirty=0; /* unsaved changes flag */
static char paint_filename[16]="UNTITLED.BMP";
static const u32 paint_palette[]={0x000000,0xFFFFFF,0xFF0000,0x00CC00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0x808080,0xFF8000,0x008060,0x800080};
#define PAINT_NC 12
void paint_clear(void){u32*c=PAINT_CANVAS;for(int i=0;i<MAX_CANVAS_W*MAX_CANVAS_H;i++)c[i]=0xFFFFFF;}
void save_paint_bmp(void){
    /* Write a minimal 24-bit BMP file */
    static u8 bmp_buf[54+MAX_CANVAS_W*MAX_CANVAS_H*3];
    int W=MAX_CANVAS_W,H=MAX_CANVAS_H;
    int row_stride=((W*3+3)/4)*4;
    int data_sz=row_stride*H;
    int file_sz=54+data_sz;
    /* BMP header */
    bmp_buf[0]='B';bmp_buf[1]='M';
    bmp_buf[2]=file_sz&0xFF;bmp_buf[3]=(file_sz>>8)&0xFF;bmp_buf[4]=(file_sz>>16)&0xFF;bmp_buf[5]=(file_sz>>24)&0xFF;
    bmp_buf[6]=0;bmp_buf[7]=0;bmp_buf[8]=0;bmp_buf[9]=0;
    bmp_buf[10]=54;bmp_buf[11]=0;bmp_buf[12]=0;bmp_buf[13]=0;
    /* DIB header */
    bmp_buf[14]=40;bmp_buf[15]=0;bmp_buf[16]=0;bmp_buf[17]=0;
    bmp_buf[18]=W&0xFF;bmp_buf[19]=(W>>8)&0xFF;bmp_buf[20]=(W>>16)&0xFF;bmp_buf[21]=(W>>24)&0xFF;
    int nH=-H;
    bmp_buf[22]=nH&0xFF;bmp_buf[23]=(nH>>8)&0xFF;bmp_buf[24]=(nH>>16)&0xFF;bmp_buf[25]=(nH>>24)&0xFF;
    bmp_buf[26]=1;bmp_buf[27]=0;bmp_buf[28]=24;bmp_buf[29]=0;
    bmp_buf[30]=0;bmp_buf[31]=0;bmp_buf[32]=0;bmp_buf[33]=0;
    bmp_buf[34]=data_sz&0xFF;bmp_buf[35]=(data_sz>>8)&0xFF;bmp_buf[36]=(data_sz>>16)&0xFF;bmp_buf[37]=(data_sz>>24)&0xFF;
    for(int i=38;i<54;i++)bmp_buf[i]=0;
    /* Pixel data */
    u32*cv=PAINT_CANVAS;
    for(int y=0;y<H;y++){
        u8*row=bmp_buf+54+y*row_stride;
        for(int x=0;x<W;x++){
            u32 c=cv[y*W+x];
            row[x*3+0]=c&0xFF;      /* B */
            row[x*3+1]=(c>>8)&0xFF; /* G */
            row[x*3+2]=(c>>16)&0xFF;/* R */
        }
        for(int p=W*3;p<row_stride;p++)row[p]=0;
    }
    fat_write_file(paint_filename,bmp_buf,54+data_sz);
    paint_dirty=0;
}
void draw_paint(struct Window*w,int mx,int my,int btn){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w),ch=win_client_h(w);
    /* Toolbar */
    gui_rect(px,py0,cw,22,0xC0C0C0);
    gui_print("Color:",px+4,py0+3,0x000000);
    for(int i=0;i<PAINT_NC;i++){
        int bx=px+58+i*16,by2=py0+3;
        gui_rect(bx,by2,14,14,paint_palette[i]);
        gui_hline(bx,by2,14,0x000000);gui_vline(bx,by2,14,0x000000);
        gui_hline(bx,by2+13,14,0x000000);gui_vline(bx+13,by2,14,0x000000);
        if(paint_palette[i]==paint_color){gui_hline(bx-1,by2-1,16,0xFF0000);gui_hline(bx-1,by2+14,16,0xFF0000);gui_vline(bx-1,by2-1,16,0xFF0000);gui_vline(bx+14,by2-1,16,0xFF0000);}
    }
    int cbx=px+cw-170,cby=py0+2;
    gui_btn(cbx,cby,48,18,"Clear",0);
    int sbx=px+cw-118,sby=py0+2;
    gui_btn(sbx,sby,54,18,paint_dirty?"*Save":"Save",0);
    int sabx=px+cw-60,saby=py0+2;
    gui_btn(sabx,saby,56,18,"Save As",0);

    /* Canvas fills full window client area below toolbar */
    int can_x=px+1,can_y=py0+23;
    int can_w=cw-2,can_h=ch-24;
    if(can_w>MAX_CANVAS_W)can_w=MAX_CANVAS_W;
    if(can_h>MAX_CANVAS_H)can_h=MAX_CANVAS_H;
    if(can_w<=0||can_h<=0)return;

    u32*cv=PAINT_CANVAS;
    for(int j=0;j<can_h;j++){
        int sy=can_y+j;
        u32*row=cv+j*MAX_CANVAS_W;
        for(int i=0;i<can_w;i++)gui_pixel(can_x+i,sy,row[i]);
    }
    gui_hline(can_x-1,can_y-1,can_w+2,0x808080);gui_vline(can_x-1,can_y-1,can_h+2,0x808080);
    gui_hline(can_x-1,can_y+can_h,can_w+2,0x808080);gui_vline(can_x+can_w,can_y-1,can_h+2,0x808080);

    if(mx>=can_x&&mx<can_x+can_w&&my>=can_y&&my<can_y+can_h&&(btn&1)){
        int bx=mx-can_x,by2=my-can_y;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
            int nx=bx+dx,ny=by2+dy;
            if(nx>=0&&nx<MAX_CANVAS_W&&ny>=0&&ny<MAX_CANVAS_H){cv[ny*MAX_CANVAS_W+nx]=paint_color;paint_dirty=1;}
        }
    }
    if(btn&1){
        for(int i=0;i<PAINT_NC;i++){int bx=px+58+i*16,by2=py0+3;if(mx>=bx&&mx<bx+14&&my>=by2&&my<by2+14)paint_color=paint_palette[i];}
        if(mx>=cbx&&mx<cbx+48&&my>=cby&&my<cby+18){paint_clear();paint_dirty=0;}
        if(mx>=sbx&&mx<sbx+54&&my>=sby&&my<sby+18)save_paint_bmp();
        if(mx>=sabx&&mx<sabx+56&&my>=saby&&my<saby+18)open_saveas_dialog(1);
    }
}

/* ══════════════════════════════════════
   NOTEPAD APP
   ══════════════════════════════════════ */
#define NOTE_LINES 25
#define NOTE_COLS  64
static char note_buf[NOTE_LINES][NOTE_COLS+1];
static int note_cx=0,note_cy=0,note_active=0;
static int term_active=0; /* forward declared here; also initialized in Terminal section */
static u32 note_saved_t=0;
static int note_dirty=0;
static char current_note_file[16]="NOTE.TXT";
void note_clear(void){for(int i=0;i<NOTE_LINES;i++){for(int j=0;j<=NOTE_COLS;j++)note_buf[i][j]=0;}note_cx=0;note_cy=0;note_saved_t=0;note_dirty=0;}
void note_load_file(const char*filename){
    note_clear();
    int fi=0;while(filename[fi]&&fi<15){current_note_file[fi]=filename[fi];fi++;}current_note_file[fi]=0;
    static u8 temp_buf[4096];
    int n=fat_read_file(filename,temp_buf,sizeof(temp_buf)-1);
    if(n>0){
        temp_buf[n]=0;
        int line=0,col=0;
        for(int i=0;i<n&&line<NOTE_LINES;i++){
            char ch=(char)temp_buf[i];
            if(ch=='\r')continue;
            if(ch=='\n'){note_buf[line][col]=0;line++;col=0;}
            else{
                if(col<NOTE_COLS-1){note_buf[line][col++]=ch;note_buf[line][col]=0;}
            }
        }
        note_cx=0;note_cy=0;
    }
    note_dirty=0;
}
void open_file_in_notepad(const char*filename){
    int np_win=-1;
    for(int i=0;i<nwins;i++){if(wins[i].app_id==APP_NOTEPAD){np_win=i;break;}}
    char title[32]="Notepad - ";
    int ti=10;for(int j=0;filename[j]&&ti<30;j++)title[ti++]=filename[j];title[ti]=0;
    if(np_win<0){
        win_new(APP_NOTEPAD,100,50,540,390,title);
    }else{
        win_raise(np_win);np_win=nwins-1;
        wins[np_win].minimized=0;wins[np_win].visible=1;
        my_strcpy(wins[np_win].title,title);
    }
    note_load_file(filename);
    note_active=1;term_active=0;
}
void note_save(void){
    char fbuf[NOTE_LINES*(NOTE_COLS+2)+1];
    int fi=0;
    for(int l=0;l<NOTE_LINES;l++){
        int len=my_strlen(note_buf[l]);
        for(int c=0;c<len;c++)fbuf[fi++]=note_buf[l][c];
        fbuf[fi++]='\r';fbuf[fi++]='\n';
    }
    fat_write_file(current_note_file[0]?current_note_file:"NOTE.TXT",(u8*)fbuf,fi);
    note_saved_t=ticks+200;
    note_dirty=0;
}
void draw_notepad(struct Window*w){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w),ch=win_client_h(w);
    gui_rect(px,py0,cw,20,0xC0C0C0);
    /* Show dirty marker in toolbar */
    const char*note_title=note_dirty?"[*] Notepad":w->title;
    gui_print_clipped(note_title,px+4,py0+2,0x000000,px+cw-130);
    int sbx=px+cw-116,sby=py0+2;
    gui_btn(sbx,sby,50,16,note_dirty?"*Save":"Save",0);
    int sabx=px+cw-62,saby=py0+2;
    gui_btn(sabx,saby,58,16,"Save As",0);
    if(note_saved_t&&ticks<note_saved_t){
        gui_print("Saved!",px+cw-170,py0+2,0x008000);
    }
    int ta_x=px,ta_y=py0+22;
    int c_adv=font_loaded?10:8;
    int max_vis_lines=(ch-26)/16;
    if(max_vis_lines>NOTE_LINES)max_vis_lines=NOTE_LINES;
    int max_vis_cols=(cw-36)/c_adv;
    if(max_vis_cols>NOTE_COLS)max_vis_cols=NOTE_COLS;
    gui_rect(ta_x,ta_y,cw,ch-22,0x1E1E1E);
    for(int line=0;line<max_vis_lines;line++){
        int lx=ta_x+2,ly=ta_y+2+line*16;
        char ln[4];itoa(ln,(u32)(line+1));gui_print_bg(ln,lx,ly,0x606060,0x1E1E1E);lx+=24;
        for(int col=0;col<max_vis_cols&&note_buf[line][col];col++){
            gui_char(note_buf[line][col],lx+col*c_adv,ly,0xD4D4D4,0x1E1E1E);
        }
        if(note_active&&line==note_cy&&(ticks/50)&1){
            if(note_cx<=max_vis_cols)gui_rect(lx+note_cx*c_adv,ly,2,15,0xFFFFFF);
        }
    }
}
void notepad_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w);
    int sbx=px+cw-116,sby=py0+2;
    int sabx=px+cw-62,saby=py0+2;
    if(mx>=sbx&&mx<sbx+50&&my>=sby&&my<sby+16){
        note_save();
    }
    if(mx>=sabx&&mx<sabx+58&&my>=saby&&my<saby+16){
        open_saveas_dialog(0);
    }
}
void notepad_key(char c,struct Window*w){
    if(!note_active)return;
    int c_adv=font_loaded?10:8;
    int max_cols=w?(win_client_w(w)-36)/c_adv:34;
    if(max_cols>NOTE_COLS-1)max_cols=NOTE_COLS-1;
    if(max_cols<10)max_cols=10;

    if(c=='\n'){
        if(note_cy<NOTE_LINES-1){note_cy++;note_cx=0;}
    }else if(c=='\b'){
        if(note_cx>0){note_cx--;note_buf[note_cy][note_cx]=0;}
        else if(note_cy>0){note_cy--;note_cx=my_strlen(note_buf[note_cy]);}
    }else{
        if(note_cx>=max_cols){
            if(note_cy<NOTE_LINES-1){note_cy++;note_cx=0;}
        }
        if(note_cx<NOTE_COLS-1){
            note_buf[note_cy][note_cx++]=c;
            note_buf[note_cy][note_cx]=0;
        }
    }
    note_dirty=1;
}

/* ══════════════════════════════════════
   SAVE AS DIALOG (for Notepad & Paint)
   ══════════════════════════════════════ */
static char saveas_buf[16]="FILE.TXT";
static int saveas_target=0; /* 0 = Notepad, 1 = Paint */

void open_saveas_dialog(int for_paint){
    for(int i=0;i<nwins;i++){
        if(wins[i].app_id==APP_SAVEAS){
            win_raise(i);
            return;
        }
    }
    saveas_target=for_paint;
    if(for_paint){
        my_strcpy(saveas_buf,paint_filename[0]?paint_filename:"UNTITLED.BMP");
    }else{
        my_strcpy(saveas_buf,current_note_file[0]?current_note_file:"NOTE.TXT");
    }
    int dw=300,dh=120;
    int dx=(fw-dw)/2,dy=(fh-dh)/2;
    win_new(APP_SAVEAS,dx,dy,dw,dh,for_paint?"Save Image As...":"Save Text As...");
}
void saveas_close(void){
    for(int i=0;i<nwins;i++){
        if(wins[i].app_id==APP_SAVEAS){
            for(int j=i;j<nwins-1;j++)wins[j]=wins[j+1];
            nwins--;break;
        }
    }
}
void saveas_confirm(void){
    if(!saveas_buf[0])return;
    if(saveas_target==1){
        my_strcpy(paint_filename,saveas_buf);
        save_paint_bmp();
        for(int i=0;i<nwins;i++){
            if(wins[i].app_id==APP_PAINT){
                char nt[32]="Paint - ";my_strcat(nt,paint_filename);
                my_strcpy(wins[i].title,nt);break;
            }
        }
    }else{
        my_strcpy(current_note_file,saveas_buf);
        note_save();
        for(int i=0;i<nwins;i++){
            if(wins[i].app_id==APP_NOTEPAD){
                char nt[32]="Notepad - ";my_strcat(nt,current_note_file);
                my_strcpy(wins[i].title,nt);break;
            }
        }
    }
    saveas_close();
}
void draw_saveas(struct Window*w){
    int px=win_client_x(w),py=win_client_y(w),cw=win_client_w(w);
    gui_rect(px,py,cw,win_client_h(w),0xC0C0C0);
    gui_print("Enter filename (8.3 format):",px+10,py+8,0x000000);
    int ix=px+10,iy=py+28,iw=cw-20,ih=20;
    gui_rect(ix,iy,iw,ih,0xFFFFFF);
    gui_hline(ix,iy,iw,0x808080);gui_vline(ix,iy,ih,0x808080);
    gui_hline(ix,iy+ih-1,iw,0xFFFFFF);gui_vline(ix+iw-1,iy,ih,0xFFFFFF);
    gui_print(saveas_buf,ix+6,iy+2,0x000000);
    if((ticks/40)&1){
        int sl=gui_str_width(saveas_buf);
        gui_rect(ix+6+sl,iy+3,2,14,0x000000);
    }
    int by=py+60;
    gui_btn(px+cw-140,by,60,22,"Save",0);
    gui_btn(px+cw-70,by,60,22,"Cancel",0);
}
void saveas_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),py=win_client_y(w),cw=win_client_w(w);
    int by=py+60;
    if(mx>=px+cw-140&&mx<px+cw-80&&my>=by&&my<by+22){saveas_confirm();}
    if(mx>=px+cw-70&&mx<px+cw-10&&my>=by&&my<by+22){saveas_close();}
}
void saveas_key(char c){
    int len=my_strlen(saveas_buf);
    if(c=='\n'){saveas_confirm();}
    else if(c==27){saveas_close();}
    else if(c=='\b'){if(len>0)saveas_buf[len-1]=0;}
    else if(len<12){
        if(c>='a'&&c<='z')c-=32;
        if((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'){
            saveas_buf[len]=c;saveas_buf[len+1]=0;
        }
    }
}

/* ══════════════════════════════════════
   TERMINAL APP
   ══════════════════════════════════════ */
#define TERM_LINES 10
#define TERM_COLS  38
static char term_screen[TERM_LINES][TERM_COLS+1];
static int  term_cx_p=0,term_cy_p=0;
static char term_cmd[64];static int term_cmdlen=0;
void term_clear_screen(void){for(int i=0;i<TERM_LINES;i++)for(int j=0;j<=TERM_COLS;j++)term_screen[i][j]=0;term_cx_p=0;term_cy_p=0;}
void term_scroll_up(void){for(int i=0;i<TERM_LINES-1;i++)my_memcpy(term_screen[i],term_screen[i+1],TERM_COLS+1);for(int j=0;j<=TERM_COLS;j++)term_screen[TERM_LINES-1][j]=0;term_cy_p=TERM_LINES-1;term_cx_p=0;}
void term_putchar(char c){if(c=='\n'){term_cx_p=0;term_cy_p++;if(term_cy_p>=TERM_LINES)term_scroll_up();return;}if(c=='\b'){if(term_cx_p>0){term_cx_p--;term_screen[term_cy_p][term_cx_p]=0;}return;}if(term_cx_p>=TERM_COLS){term_cx_p=0;term_cy_p++;if(term_cy_p>=TERM_LINES)term_scroll_up();}term_screen[term_cy_p][term_cx_p++]=c;}
void term_print(const char*s){while(*s)term_putchar(*s++);}
void term_dec(u32 v){char b[12];itoa(b,v);term_print(b);}
void term_exec(const char*cmd){
    if(!cmd[0])return;
    if(!my_strcmp(cmd,"help")){term_print("help ls time meminfo\nsysinfo clear reboot cat\n");return;}
    if(!my_strcmp(cmd,"clear")){term_clear_screen();return;}
    if(!my_strcmp(cmd,"ls")){u8*root=ramdisk+root_sect*512;for(int i=0;i<root_cnt;i++){u8*e=root+i*32;if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;for(int j=0;j<8;j++)if(e[j]!=' ')term_putchar(e[j]);term_putchar('.');for(int j=8;j<11;j++)if(e[j]!=' ')term_putchar(e[j]);term_putchar('\n');}return;}
    if(!my_strcmp(cmd,"time")){term_print("Uptime: ");term_dec(ticks/100);term_print("s\n");return;}
    if(!my_strcmp(cmd,"reboot")){outb(0x64,0xFE);return;}
    if(!my_strcmp(cmd,"meminfo")){u32 cnt=*(u32*)0x71C;if(cnt>8)cnt=8;term_dec(cnt);term_print(" E820 regions\n");return;}
    if(!my_strcmp(cmd,"sysinfo")){u32 a,b,c,d;cpuid(0,&a,&b,&c,&d);char v[13];*(u32*)&v[0]=b;*(u32*)&v[4]=d;*(u32*)&v[8]=c;v[12]=0;term_print(v);term_putchar('\n');return;}
    term_print("Unknown: ");term_print(cmd);term_putchar('\n');
}
void draw_terminal(struct Window*w){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w),ch=win_client_h(w);
    gui_rect(px,py0,cw,ch,0x0C0C0C);
    int c_adv=font_loaded?10:8;
    for(int line=0;line<TERM_LINES;line++){int ly=py0+line*16;for(int col=0;term_screen[line][col]&&col<TERM_COLS;col++)gui_char(term_screen[line][col],px+col*c_adv,ly,0xCCCCCC,0x0C0C0C);}
    int pl=py0+term_cy_p*16;
    int pw=gui_str_width("$ ");
    gui_print_bg("$ ",px,pl,0x00FF00,0x0C0C0C);
    for(int i=0;i<term_cmdlen;i++)gui_char(term_cmd[i],px+pw+i*c_adv,pl,0xFFFFFF,0x0C0C0C);
    if(term_active&&(ticks/50)&1)gui_rect(px+pw+term_cmdlen*c_adv,pl,8,13,0x808080);
}
void terminal_key(char c){if(!term_active)return;if(c=='\n'){term_print("$ ");term_print(term_cmd);term_putchar('\n');term_cmd[term_cmdlen]=0;term_exec(term_cmd);term_cmdlen=0;term_cmd[0]=0;term_print("$ ");}else if(c=='\b'){if(term_cmdlen>0){term_cmdlen--;term_cmd[term_cmdlen]=0;}}else{if(term_cmdlen<63){term_cmd[term_cmdlen++]=c;term_cmd[term_cmdlen]=0;}}}

/* ══════════════════════════════════════
   TASK MANAGER
   ══════════════════════════════════════ */
void draw_taskmgr(struct Window*w){
    int px=win_client_x(w),py=win_client_y(w),cw=win_client_w(w);
    gui_rect(px,py,cw,16,0x000080);gui_print("BrickOS Task Manager",px+4,py,0xFFFFFF);py+=18;
    gui_rect(px,py,cw,16,0xC0C0C0);gui_print("Process           PID  CPU  Status",px+2,py+1,0x000000);py+=18;
    const char*procs[]={"System Idle","Kernel","GUI Server","Mouse Drv","Kbd Driver"};
    for(int i=0;i<5;i++){if(i&1)gui_rect(px,py,cw,16,0xEEEEFF);gui_print(procs[i],px+2,py+1,0x000000);char pb[4];itoa(pb,(u32)i);gui_print(pb,px+140,py+1,0x000000);gui_print(i==0?"99%":"0%",px+180,py+1,0x000000);gui_print("Running",px+220,py+1,0x008000);py+=16;}
    for(int i=0;i<nwins;i++){if(!wins[i].visible)continue;if((5+i)&1)gui_rect(px,py,cw,16,0xEEEEFF);gui_print(wins[i].title,px+2,py+1,0x000000);char pb[4];itoa(pb,(u32)(5+i));gui_print(pb,px+140,py+1,0x000000);gui_print("0%",px+180,py+1,0x000000);gui_print(wins[i].minimized?"Minimized":"Running",px+220,py+1,wins[i].minimized?0x808080:0x008000);py+=16;if(py>win_client_y(w)+win_client_h(w)-4)break;}
    py+=4;
    gui_rect(px,py,cw,16,0x000080);gui_print("Performance",px+4,py,0xFFFFFF);py+=18;
    char upb[16];itoa(upb,ticks/100);gui_print("Uptime: ",px,py,0x000000);gui_print(upb,px+64,py,0x000000);gui_print("s",px+64+gui_str_width(upb),py,0x000000);py+=16;
    char tb[16];itoa(tb,(u32)nwins);gui_print("Windows:",px,py,0x000000);gui_print(tb,px+72,py,0x000000);
}

/* ══════════════════════════════════════
   SYSTEM INFO
   ══════════════════════════════════════ */
void draw_sysinfo(struct Window*w){
    int px=win_client_x(w),py=win_client_y(w);
    gui_print("BrickOS 32-bit",px,py,0x000080);py+=20;
    gui_print("OS Version: BrickOS 1.0",px,py,0x000000);py+=16;
    gui_print("Vendor:     FlatronTech",px,py,0x000000);py+=16;
    char cpubrand[50];
    get_cpu_brand(cpubrand,50);
    gui_print("CPU Model:",px,py,0x000000);py+=16;
    gui_print(cpubrand,px+12,py,0x000080);py+=18;

    char resb[24];
    itoa(resb,(u32)fw);int ri=my_strlen(resb);resb[ri++]=' ';resb[ri++]='x';resb[ri++]=' ';
    itoa(resb+ri,(u32)fh);int ri2=my_strlen(resb);
    resb[ri2++]=' ';resb[ri2++]='V';resb[ri2++]='E';resb[ri2++]='S';resb[ri2++]='A';resb[ri2]=0;
    int lx=px+120;
    gui_print("Resolution:",px,py,0x000000);gui_print(resb,lx,py,0x000080);py+=16;

    u32 cnt=*(u32*)0x71C;if(cnt>32)cnt=32;u32 tkb=0;
    for(u32 i=0;i<cnt;i++){u8*e=(u8*)(0x720+i*20);u32 ty=e[16]|(e[17]<<8)|(e[18]<<16)|(e[19]<<24);if(ty==1){u32 ll=e[8]|(e[9]<<8)|(e[10]<<16)|(e[11]<<24);tkb+=ll/1024;}}
    char mb[12];itoa(mb,tkb/1024);gui_print("RAM Memory:",px,py,0x000000);gui_print(mb,lx,py,0x000080);gui_print(" MB",lx+gui_str_width(mb),py,0x000000);py+=16;

    gui_print("Drivers:",px,py,0x000000);gui_print("INF loaded",lx,py,0x000080);py+=16;
    gui_print("Input:",px,py,0x000000);gui_print("PS/2 Mouse & Keyboard",lx,py,0x000080);py+=18;
    gui_print("System is operational!",px,py,0x008000);
}

/* ══════════════════════════════════════
   EXPLORER (My Computer style)
   ══════════════════════════════════════ */
static int expl_sel_idx=-1;
static int expl_last_idx=-1;
static u32 expl_last_t=0;

void draw_explorer(struct Window*w){
    int px=win_client_x(w),py0=win_client_y(w);
    int cw=win_client_w(w),ch=win_client_h(w);
    /* Toolbar / address bar */
    gui_rect(px,py0,cw,16,0xC0C0C0);
    if(w->app_state==0){
        gui_print("My Computer",px+4,py0,0x000000);
        /* Breadcrumb */
        gui_rect(px,py0+18,cw,14,0xFFFFFF);
        gui_print("My Computer",px+4,py0+18,0x000080);
        /* Drive icon */
        int dx=px+30,dy=py0+38;
        /* floppy body */
        gui_rect(dx,dy,50,44,0xC0C0C0);
        gui_rect(dx+2,dy+2,46,40,0x8080A0);
        gui_rect(dx+8,dy+2,28,14,0xD0D0FF);  /* label */
        gui_rect(dx+14,dy+28,22,14,0x606060); /* disk slot */
        gui_rect(dx+18,dy+30,14,10,0xA0A0C0);
        gui_print("A:",dx+16,dy+15,0x000000);
        gui_print("3.5 Floppy (A:)",px+4,dy+48,0x000000);
        gui_print("Click to open",px+4,dy+66,0x808080);
        /* Status bar */
        gui_rect(px,py0+ch-14,cw,14,0xC0C0C0);gui_hline(px,py0+ch-15,cw,0x808080);
        gui_print("1 drive",px+4,py0+ch-13,0x000000);
    } else {
        /* File list view */
        gui_btn(px,py0,32,16,"<-",0);
        gui_rect(px+34,py0,cw-34,16,0xFFFFFF);gui_print("A:\\",px+38,py0,0x000080);
        int hy=py0+18;
        gui_rect(px,hy,cw,14,0xC0C0C0);
        int col_nm=px+20,col_sz=px+170,col_ty=px+270;
        gui_print("Name",col_nm,hy+1,0x000000);
        gui_print("Size",col_sz,hy+1,0x000000);
        gui_print("Type",col_ty,hy+1,0x000000);
        u8*root=ramdisk+root_sect*512;int fy=hy+16,count=0;
        for(int i=0;i<root_cnt;i++){
            u8*e=root+i*32;
            if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;
            int sel=(count==expl_sel_idx);
            if(sel)gui_rect(px,fy,cw,16,0x000080);
            else if(count&1)gui_rect(px,fy,cw,16,0xEEEEFF);
            /* icon by ext - improved look */
            int is_txt=(e[8]=='T'&&e[9]=='X'&&e[10]=='T');
            int is_inf=(e[8]=='I'&&e[9]=='N'&&e[10]=='F');
            int is_bin=(e[8]=='B'&&e[9]=='I'&&e[10]=='N');
            int is_bmp=(e[8]=='B'&&e[9]=='M'&&e[10]=='P');
            /* Draw file icon */
            int ix=px+2,iy=fy+2;
            gui_rect(ix,iy,12,12,0xFFFFFF);gui_hline(ix,iy,12,0x808080);gui_vline(ix,iy,12,0x808080);gui_hline(ix,iy+11,12,0x808080);gui_vline(ix+11,iy,12,0x808080);
            /* dog-ear top-right corner */
            gui_rect(ix+8,iy,4,4,0xC0C0C0);gui_pixel(ix+8,iy,0x808080);gui_pixel(ix+11,iy+3,0x808080);
            if(is_txt){gui_hline(ix+2,iy+4,7,0x0000AA);gui_hline(ix+2,iy+6,7,0x0000AA);gui_hline(ix+2,iy+8,5,0x0000AA);}
            else if(is_inf){gui_rect(ix+3,iy+3,6,7,0xFF8000);gui_hline(ix+4,iy+2,4,0xFF8000);}
            else if(is_bin){gui_rect(ix+2,iy+3,8,6,0x404040);gui_print("EXE",ix+1,iy+3,0x00FF00);}
            else if(is_bmp){gui_rect(ix+2,iy+3,8,7,0xFFFF00);gui_pixel(ix+4,iy+5,0xFF0000);gui_pixel(ix+7,iy+5,0x00AA00);gui_pixel(ix+6,iy+7,0x0000FF);}
            /* name */
            char nm[14];int ni=0;
            for(int j=0;j<8&&e[j]!=' ';j++)nm[ni++]=e[j];
            nm[ni++]='.';for(int j=8;j<11&&e[j]!=' ';j++)nm[ni++]=e[j];nm[ni]=0;
            u32 txt_col=sel?0xFFFFFF:0x000000;
            gui_print(nm,col_nm,fy+1,txt_col);
            /* size - formatted */
            u32 sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
            char szb[16];format_size(szb,sz);gui_print(szb,col_sz,fy+1,txt_col);
            /* type */
            const char*ty="Data";
            if(is_txt)ty="Text File";
            else if(is_inf)ty="INF Driver";
            else if(is_bin)ty="Binary";
            else if(is_bmp)ty="Bitmap";
            gui_print(ty,col_ty,fy+1,txt_col);
            fy+=16;count++;if(fy>py0+ch-20)break;
        }
        /* Status bar */
        gui_rect(px,py0+ch-15,cw,15,0xC0C0C0);gui_hline(px,py0+ch-16,cw,0x808080);
        char cb[6];itoa(cb,(u32)count);gui_print(cb,px+4,py0+ch-14,0x000000);gui_print(" files",px+4+gui_str_width(cb),py0+ch-14,0x000000);
    }
}
void explorer_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),py0=win_client_y(w),ch=win_client_h(w);
    if(w->app_state==0){
        int dx=px+30,dy=py0+38;
        if(mx>=dx&&mx<dx+50&&my>=dy&&my<dy+60)w->app_state=1;
    } else {
        if(mx>=px&&mx<px+32&&my>=py0&&my<py0+16){w->app_state=0;expl_sel_idx=-1;return;}
        int hy=py0+18;
        if(my>=hy+16){
            int f_idx=(my-(hy+16))/16;
            u8*root=ramdisk+root_sect*512;int cnt=0;
            for(int i=0;i<root_cnt;i++){
                u8*e=root+i*32;
                if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;
                if(cnt==f_idx){
                    char nm[14];int ni=0;
                    for(int j=0;j<8&&e[j]!=' ';j++)nm[ni++]=e[j];
                    nm[ni++]='.';for(int j=8;j<11&&e[j]!=' ';j++)nm[ni++]=e[j];nm[ni]=0;
                    int is_bin=(e[8]=='B'&&e[9]=='I'&&e[10]=='N');
                    int is_bmp=(e[8]=='B'&&e[9]=='M'&&e[10]=='P');
                    if(f_idx==expl_last_idx&&ticks-expl_last_t<50){
                        if(is_bmp){open_bmp_viewer(nm);}
                        else if(!is_bin){open_file_in_notepad(nm);}
                        expl_sel_idx=-1;
                    }else{
                        expl_sel_idx=f_idx;
                    }
                    expl_last_idx=f_idx;expl_last_t=ticks;
                    return;
                }
                cnt++;
            }
        }
    }
}

/* ══════════════════════════════════════
   CONTROL PANEL
   ══════════════════════════════════════ */
static int ctrlpanel_tab=0;
static u32 desktop_color=0x1F5C8B;
static const u32 desk_pal[]={0x1F5C8B,0x004080,0x205020,0x602020,0x402060,0x104060,0x505050,0x204840};
#define N_DESK_PAL 8

void draw_ctrlpanel(struct Window*w){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w);
    const char*tabs[]={"System","Display","Personalize"};
    int tx=px;
    for(int i=0;i<3;i++){
        int tw=gui_str_width(tabs[i])+20;
        u32 tbg=(ctrlpanel_tab==i)?0xFFFFFF:0xD0D0D0;
        gui_rect(tx,py0,tw,18,tbg);
        if(ctrlpanel_tab==i){gui_hline(tx,py0,tw,0xFFFFFF);gui_vline(tx,py0,18,0xFFFFFF);}
        else{gui_hline(tx,py0,tw,0xFFFFFF);gui_vline(tx,py0,18,0xFFFFFF);gui_hline(tx,py0+17,tw,0x808080);gui_vline(tx+tw-1,py0,18,0x808080);}
        gui_print(tabs[i],tx+10,py0+2,0x000000);tx+=tw+3;
    }
    int cy2=py0+24;
    int lx=px+130;
    if(ctrlpanel_tab==0){
        gui_print("BrickOS System Information",px,cy2,0x000080);cy2+=20;
        gui_print("OS Version:",px,cy2,0x000000);gui_print("BrickOS 1.0",lx,cy2,0x000080);cy2+=16;
        gui_print("Architecture:",px,cy2,0x000000);gui_print("i386 32-bit Protected Mode",lx,cy2,0x000080);cy2+=16;
        gui_print("Vendor:",px,cy2,0x000000);gui_print("FlatronTech",lx,cy2,0x000080);cy2+=16;
        gui_print("Build date:",px,cy2,0x000000);gui_print("2026-10-14",lx,cy2,0x000080);cy2+=16;
        char cpubrand[50];
        get_cpu_brand(cpubrand,50);
        gui_print("CPU Model:",px,cy2,0x000000);cy2+=16;
        gui_print(cpubrand,px+12,cy2,0x000080);cy2+=18;
        u32 cnt=*(u32*)0x71C;if(cnt>32)cnt=32;u32 tkb=0;
        for(u32 i=0;i<cnt;i++){u8*e=(u8*)(0x720+i*20);u32 ty=e[16]|(e[17]<<8)|(e[18]<<16)|(e[19]<<24);if(ty==1){u32 ll=e[8]|(e[9]<<8)|(e[10]<<16)|(e[11]<<24);tkb+=ll/1024;}}
        char mb[12];itoa(mb,tkb/1024);gui_print("RAM Memory:",px,cy2,0x000000);gui_print(mb,lx,cy2,0x000080);gui_print(" MB",lx+gui_str_width(mb),cy2,0x000000);cy2+=16;
        char rtc[10];get_rtc_time(rtc);gui_print("System Time:",px,cy2,0x000000);gui_print(rtc,lx,cy2,0x000080);
    } else if(ctrlpanel_tab==1){
        gui_print("Display Settings",px,cy2,0x000080);cy2+=20;
        char resb[24];itoa(resb,(u32)fw);int ri=my_strlen(resb);resb[ri++]=' ';resb[ri++]='x';resb[ri++]=' ';itoa(resb+ri,(u32)fh);
        gui_print("Resolution:",px,cy2,0x000000);gui_print(resb,lx,cy2,0x000080);cy2+=16;
        gui_print("Video Mode:",px,cy2,0x000000);gui_print("VESA LFB",lx,cy2,0x000080);cy2+=16;
        char bppb[4];itoa(bppb,(u32)fbpp);
        gui_print("Color depth:",px,cy2,0x000000);gui_print(bppb,lx,cy2,0x000080);gui_print(" bpp",lx+gui_str_width(bppb),cy2,0x000000);cy2+=16;
        char pitb[8];itoa(pitb,(u32)fpitch);
        gui_print("Line Pitch:",px,cy2,0x000000);gui_print(pitb,lx,cy2,0x000080);gui_print(" bytes",lx+gui_str_width(pitb),cy2,0x000000);cy2+=16;
        gui_print("Target Rate:",px,cy2,0x000000);gui_print("60 Hz",lx,cy2,0x000080);cy2+=20;
        gui_print("Change Resolution:",px,cy2,0x000000);cy2+=16;
        int sel800=(fw==800),sel640=(fw==640);
        gui_btn(px+10,cy2,110,22,"800 x 600",sel800);
        gui_btn(px+130,cy2,110,22,"640 x 480",sel640);cy2+=28;
        gui_btn(px+40,cy2,160,24,"Apply & Reboot",0);
    } else {
        gui_print("Desktop Background",px,cy2,0x000080);cy2+=20;
        gui_print("Select wallpaper color:",px,cy2,0x000000);cy2+=18;
        for(int i=0;i<N_DESK_PAL;i++){
            int sw=36,sh=26;int bx=px+(i%4)*(sw+8),by2=cy2+(i/4)*(sh+8);
            gui_rect(bx,by2,sw,sh,desk_pal[i]);
            gui_hline(bx,by2,sw,0x808080);gui_vline(bx,by2,sh,0x808080);
            gui_hline(bx,by2+sh-1,sw,0x808080);gui_vline(bx+sw-1,by2,sh,0x808080);
            if(desk_pal[i]==desktop_color){gui_hline(bx-2,by2-2,sw+4,0xFFFFFF);gui_hline(bx-2,by2+sh+1,sw+4,0xFFFFFF);gui_vline(bx-2,by2-2,sh+4,0xFFFFFF);gui_vline(bx+sw+1,by2-2,sh+4,0xFFFFFF);}
        }
        cy2+=78;
        gui_print("Taskbar: Classic 3D Style",px,cy2,0x000000);cy2+=16;
        gui_print("Font:    VCR OSD Mono Bold (FONT.BIN)",px,cy2,0x000000);
    }
}
void ctrlpanel_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),py0=win_client_y(w);
    const char*tabs[]={"System","Display","Personalize"};
    int tx=px;
    for(int i=0;i<3;i++){
        int tw=gui_str_width(tabs[i])+20;
        if(mx>=tx&&mx<tx+tw&&my>=py0&&my<py0+18){ctrlpanel_tab=i;return;}
        tx+=tw+3;
    }
    if(ctrlpanel_tab==1){
        int cy2=py0+24+20+16*5+20+16;
        if(mx>=px+10&&mx<px+120&&my>=cy2&&my<cy2+22){
            outb(0x70,0x38);outb(0x71,1);
        }else if(mx>=px+130&&mx<px+240&&my>=cy2&&my<cy2+22){
            outb(0x70,0x38);outb(0x71,2);
        }
        int rby=cy2+28;
        if(mx>=px+40&&mx<px+200&&my>=rby&&my<rby+24){
            outb(0x64,0xFE);
        }
    }
    if(ctrlpanel_tab==2){
        int cy2=py0+24+20+18;
        for(int i=0;i<N_DESK_PAL;i++){
            int sw=36,sh=26;int bx=px+(i%4)*(sw+8),by2=cy2+(i/4)*(sh+8);
            if(mx>=bx&&mx<bx+sw&&my>=by2&&my<by2+sh){desktop_color=desk_pal[i];return;}
        }
    }
}

/* ══════════════════════════════════════
   FILE PROPERTIES POPUP
   ══════════════════════════════════════ */
static char fileprops_name[16]="";
static u32   fileprops_size=0;
static char  fileprops_type[20]="";
static int   fileprops_clusters=0;

void open_file_props(const char*nm,u32 sz,const char*ty,int clust){
    int i=0;while(nm[i]&&i<15){fileprops_name[i]=nm[i];i++;}fileprops_name[i]=0;
    fileprops_size=sz;
    i=0;while(ty[i]&&i<19){fileprops_type[i]=ty[i];i++;}fileprops_type[i]=0;
    fileprops_clusters=clust;
    char title[32]="Properties: ";my_strcat(title,nm);
    for(int j=0;j<nwins;j++){
        if(wins[j].app_id==APP_FILEPROPS){
            win_raise(j);
            my_strcpy(wins[nwins-1].title,title);
            wins[nwins-1].minimized=0;wins[nwins-1].visible=1;
            return;
        }
    }
    win_new(APP_FILEPROPS,200,150,300,220,title);
}
void draw_fileprops(struct Window*w){
    int px=win_client_x(w),py=win_client_y(w),cw=win_client_w(w);
    int ix=px+10,iy=py+10;
    /* file icon */
    gui_rect(ix,iy,32,32,0xFFFFFF);gui_hline(ix,iy,32,0x808080);gui_vline(ix,iy,32,0x808080);gui_hline(ix,iy+31,32,0x808080);gui_vline(ix+31,iy,32,0x808080);
    gui_rect(ix+22,iy,10,10,0xC0C0C0);gui_pixel(ix+22,iy,0x808080);gui_pixel(ix+31,iy+9,0x808080);
    gui_hline(ix+4,iy+14,20,0x0000AA);gui_hline(ix+4,iy+18,20,0x0000AA);gui_hline(ix+4,iy+22,15,0x0000AA);
    gui_print(fileprops_name,px+52,py+14,0x000000);
    gui_hline(px,py+46,cw,0x808080);
    int ly=py+54;int lx2=px+100;
    gui_print("Name:",px+8,ly,0x000000);gui_print(fileprops_name,lx2,ly,0x000080);ly+=18;
    char szb[20];format_size(szb,fileprops_size);
    gui_print("Size:",px+8,ly,0x000000);gui_print(szb,lx2,ly,0x000080);ly+=18;
    char szb2[16];itoa(szb2,fileprops_size);my_strcat(szb2," bytes");
    gui_print("",px+8,ly,0x000000);gui_print(szb2,lx2,ly,0x606060);ly+=18;
    gui_print("Type:",px+8,ly,0x000000);gui_print(fileprops_type,lx2,ly,0x000080);ly+=18;
    char cb[8];itoa(cb,(u32)fileprops_clusters);my_strcat(cb," cluster(s)");
    gui_print("Alloc:",px+8,ly,0x000000);gui_print(cb,lx2,ly,0x000080);ly+=18;
    gui_hline(px,ly+4,cw,0x808080);ly+=12;
    gui_btn(px+cw/2-30,ly,60,20,"OK",0);
}
int fileprops_ok_y(struct Window*w){return win_client_y(w)+54+18*5+12;}
int fileprops_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),cw=win_client_w(w);
    int ly=fileprops_ok_y(w);
    if(mx>=px+cw/2-30&&mx<px+cw/2+30&&my>=ly&&my<ly+20)return 1;
    return 0;
}

/* ══════════════════════════════════════
   BMP VIEWER APP
   ══════════════════════════════════════ */
static char bmpview_file[16]="";
#define BMPVIEW_BUF_SZ (54+640*480*3)
static u8 bmpview_buf[BMPVIEW_BUF_SZ];
static int bmpview_loaded=0,bmpview_w=0,bmpview_h=0;

void open_bmp_viewer(const char*filename){
    int i=0;while(filename[i]&&i<15){bmpview_file[i]=filename[i];i++;}bmpview_file[i]=0;
    bmpview_loaded=0;bmpview_w=0;bmpview_h=0;
    int n=fat_read_file(filename,bmpview_buf,BMPVIEW_BUF_SZ);
    if(n>54&&bmpview_buf[0]=='B'&&bmpview_buf[1]=='M'){
        bmpview_w=bmpview_buf[18]|(bmpview_buf[19]<<8)|(bmpview_buf[20]<<16)|(bmpview_buf[21]<<24);
        int h_raw=(int)(bmpview_buf[22]|(bmpview_buf[23]<<8)|(bmpview_buf[24]<<16)|(bmpview_buf[25]<<24));
        bmpview_h=h_raw<0?-h_raw:h_raw;
        bmpview_loaded=1;
    }
    char title[32]="View: ";my_strcat(title,filename);
    for(int j=0;j<nwins;j++){
        if(wins[j].app_id==APP_BMPVIEW){
            win_raise(j);
            my_strcpy(wins[nwins-1].title,title);
            wins[nwins-1].minimized=0;wins[nwins-1].visible=1;
            return;
        }
    }
    win_new(APP_BMPVIEW,80,60,500,380,title);
}
void draw_bmpview(struct Window*w){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w),ch=win_client_h(w);
    gui_rect(px,py0,cw,ch,0x303030);
    if(!bmpview_loaded){gui_print("Invalid or unsupported BMP",px+10,py0+20,0xFF4444);return;}
    if(bmpview_w<=0||bmpview_h<=0){gui_print("Empty image",px+10,py0+20,0xFF8800);return;}
    int bpp=bmpview_buf[28];
    if(bpp!=24&&bpp!=32){gui_print("Only 24/32-bit BMP supported",px+10,py0+20,0xFF8800);return;}
    int pxsz=bpp/8;
    int data_off=bmpview_buf[10]|(bmpview_buf[11]<<8)|(bmpview_buf[12]<<16)|(bmpview_buf[13]<<24);
    int row_stride=((bmpview_w*pxsz+3)/4)*4;
    int max_w=cw-2;if(max_w>bmpview_w)max_w=bmpview_w;
    int max_h=ch-20;if(max_h>bmpview_h)max_h=bmpview_h;
    int h_raw=(int)(bmpview_buf[22]|(bmpview_buf[23]<<8)|(bmpview_buf[24]<<16)|(bmpview_buf[25]<<24));
    int flipped=(h_raw>0);
    for(int y=0;y<max_h;y++){
        int src_y=flipped?(bmpview_h-1-y):y;
        if(src_y<0||src_y>=bmpview_h)continue;
        u8*row=bmpview_buf+data_off+src_y*row_stride;
        for(int x=0;x<max_w;x++){
            u8 b=row[x*pxsz+0],g=row[x*pxsz+1],r=row[x*pxsz+2];
            gui_pixel(px+1+x,py0+1+y,(r<<16)|(g<<8)|b);
        }
    }
    char info[40];itoa(info,(u32)bmpview_w);my_strcat(info,"x");
    char tmp[8];itoa(tmp,(u32)bmpview_h);my_strcat(info,tmp);my_strcat(info," px | ");
    char bppb[4];itoa(bppb,(u32)bpp);my_strcat(info,bppb);my_strcat(info,"bpp");
    gui_rect(px,py0+ch-18,cw,18,0x000000);
    gui_print(info,px+4,py0+ch-16,0xCCCCCC);
}

/* ══════════════════════════════════════
   WELCOME
   ══════════════════════════════════════ */
void draw_welcome(struct Window*w){
    int px=win_client_x(w),py=win_client_y(w);
    gui_rect(w->x+BORDER+2,win_client_y(w),win_client_w(w),win_client_h(w),0xFFFFCC);
    gui_print("Welcome to BrickOS!",px,py,0x000080);py+=20;
    gui_print("Click & drag titlebars to move",px,py,0x000000);py+=16;
    gui_print("Drag bottom-right corner to resize",px,py,0x000000);py+=16;
    gui_print("Right-click for context menus",px,py,0x000000);py+=16;
    gui_print("Double-click files to open in Notepad",px,py,0x000000);py+=16;
    gui_print("Click [X] to close, [_] to min, [ ] to max",px,py,0x000000);py+=16;
    py+=4;gui_print("Enjoy BrickOS!",px,py,0x008000);
}

/* ══════════════════════════════════════
   DESKTOP ICONS
   ══════════════════════════════════════ */
#define NDESK_ICONS 6
struct DesktopIcon{int x,y,app_id;u32 col1,col2;char label[16];};
static struct DesktopIcon desk_icons[NDESK_ICONS]={
    {24,  20, APP_EXPLORER,  0x3366FF, 0x1133AA, "My Computer"},
    {24, 115, APP_CTRLPANEL, 0xFF8800, 0xBB5500, "Control Panel"},
    {24, 210, APP_NOTEPAD,   0xFFFF44, 0x888800, "Notepad"},
    {24, 305, APP_PAINT,     0xFF4444, 0xAA1111, "Paint"},
    {24, 400, APP_TERMINAL,  0x004400, 0x008800, "Terminal"},
    {24, 495, APP_WELCOME,   0x44AAFF, 0x1177BB, "Welcome"},
};
static int desk_sel=-1;
static int desk_last=-1;
static u32 desk_last_t=0;

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
        int x1 = cx - l1 / 2;
        int x2 = cx - l2 / 2;
        gui_rect(x1 - 2, y, l1 + 4, fh_box + 1, bg_col);
        gui_print(w1, x1, y, 0xFFFFFF);
        gui_rect(x2 - 2, y + fh_box + 2, l2 + 4, fh_box + 1, bg_col);
        gui_print(w2, x2, y + fh_box + 2, 0xFFFFFF);
    } else {
        int l = gui_str_width(label);
        int x = cx - l / 2;
        gui_rect(x - 2, y, l + 4, fh_box + 1, bg_col);
        gui_print(label, x, y, 0xFFFFFF);
    }
}

void draw_desk_icons(void){
    for(int i=0;i<NDESK_ICONS;i++){
        struct DesktopIcon*d=&desk_icons[i];
        int x=d->x,y=d->y;

        /* Selection outline */
        if(desk_sel==i){
            for(int ds=0;ds<36;ds+=2){gui_pixel(x-2+ds,y-2,0xFFFFFF);gui_pixel(x-2+ds,y+34,0xFFFFFF);}
            for(int ds=0;ds<36;ds+=2){gui_pixel(x-2,y-2+ds,0xFFFFFF);gui_pixel(x+34,y-2+ds,0xFFFFFF);}
        }

        if(d->app_id==APP_EXPLORER){
            /* Win98 "My Computer": beige CRT monitor + desktop case */
            /* Monitor frame */
            gui_rect(x+4,y+2,24,18,0xD4D0C8);
            gui_hline(x+4,y+2,24,0xFFFFFF);gui_vline(x+4,y+2,18,0xFFFFFF);
            gui_hline(x+4,y+19,24,0x808080);gui_vline(x+27,y+2,18,0x808080);
            /* CRT screen */
            gui_rect(x+7,y+5,18,12,0x000080);
            gui_hline(x+8,y+6,5,0x4080FF);gui_vline(x+8,y+6,4,0x4080FF);
            /* Window on screen */
            gui_rect(x+11,y+8,11,7,0xC0C0C0);
            gui_hline(x+11,y+8,11,0x008080);
            gui_rect(x+12,y+10,9,4,0xFFFFFF);
            /* Power LED */
            gui_pixel(x+24,y+18,0x00FF00);
            /* Stand */
            gui_rect(x+13,y+20,6,2,0x808080);
            gui_rect(x+10,y+22,12,2,0xD4D0C8);
            gui_hline(x+10,y+22,12,0xFFFFFF);gui_hline(x+10,y+23,12,0x808080);
            /* Desktop PC case below */
            gui_rect(x+2,y+24,28,8,0xD4D0C8);
            gui_hline(x+2,y+24,28,0xFFFFFF);gui_vline(x+2,y+24,8,0xFFFFFF);
            gui_hline(x+2,y+31,28,0x404040);gui_vline(x+29,y+24,8,0x404040);
            /* Floppy drive */
            gui_rect(x+5,y+26,8,2,0x000000);gui_rect(x+13,y+26,2,2,0xD4D0C8);
            /* CD-ROM drive */
            gui_rect(x+17,y+26,10,2,0x808080);gui_pixel(x+25,y+27,0x00FF00);
            /* Power switch */
            gui_rect(x+4,y+29,2,2,0x808080);
        } else if(d->app_id==APP_CTRLPANEL){
            /* Win98 Control Panel: 3D console panel with sliders */
            gui_rect(x+3,y+3,26,26,0xC0C0C0);
            gui_hline(x+3,y+3,26,0xFFFFFF);gui_vline(x+3,y+3,26,0xFFFFFF);
            gui_hline(x+3,y+28,26,0x404040);gui_vline(x+28,y+3,26,0x404040);
            /* Blue header */
            gui_rect(x+5,y+5,22,4,0x000080);
            /* Slider 1 */
            gui_hline(x+7,y+12,18,0x808080);gui_hline(x+7,y+13,18,0xFFFFFF);
            gui_rect(x+17,y+10,4,6,0xD4D0C8);
            gui_hline(x+17,y+10,4,0xFFFFFF);gui_vline(x+17,y+10,6,0xFFFFFF);
            gui_hline(x+17,y+15,4,0x404040);gui_vline(x+20,y+10,6,0x404040);
            /* Slider 2 */
            gui_hline(x+7,y+18,18,0x808080);gui_hline(x+7,y+19,18,0xFFFFFF);
            gui_rect(x+10,y+16,4,6,0xD4D0C8);
            gui_hline(x+10,y+16,4,0xFFFFFF);gui_vline(x+10,y+16,6,0xFFFFFF);
            gui_hline(x+10,y+21,4,0x404040);gui_vline(x+13,y+16,6,0x404040);
            /* Slider 3 */
            gui_hline(x+7,y+24,18,0x808080);gui_hline(x+7,y+25,18,0xFFFFFF);
            gui_rect(x+20,y+22,4,6,0xD4D0C8);
            gui_hline(x+20,y+22,4,0xFFFFFF);gui_vline(x+20,y+22,6,0xFFFFFF);
            gui_hline(x+20,y+27,4,0x404040);gui_vline(x+23,y+22,6,0x404040);
        } else if(d->app_id==APP_NOTEPAD){
            /* Win98 Notepad: spiral paper with lines & pencil */
            gui_rect(x+7,y+4,20,27,0x000000); /* shadow */
            gui_rect(x+5,y+2,20,27,0xFFFFFF); /* paper */
            gui_hline(x+5,y+2,20,0x808080);gui_vline(x+5,y+2,27,0x808080);
            gui_hline(x+5,y+28,20,0x808080);gui_vline(x+24,y+2,27,0x808080);
            /* Spiral rings */
            for(int si=0;si<5;si++){
                int sx=x+6+si*4;
                gui_rect(sx,y+1,2,3,0x808080);gui_pixel(sx,y,0xC0C0C0);
            }
            /* Ruled lines */
            gui_hline(x+8,y+8,14,0x80A0FF);
            gui_hline(x+8,y+12,14,0x80A0FF);
            gui_hline(x+8,y+16,14,0x80A0FF);
            gui_hline(x+8,y+20,14,0x80A0FF);
            gui_hline(x+8,y+24,14,0x80A0FF);
            /* Red margin */
            gui_vline(x+9,y+4,24,0xFF8080);
            /* Pencil */
            for(int p=0;p<10;p++){gui_pixel(x+17+p,y+17+p,0xFFCC00);gui_pixel(x+18+p,y+17+p,0xFFA000);}
            gui_pixel(x+16,y+16,0xD4B080);gui_pixel(x+15,y+15,0x000000);
            gui_pixel(x+27,y+27,0xC0C0C0);gui_pixel(x+28,y+28,0xFF9999);
        } else if(d->app_id==APP_PAINT){
            /* Win98 Paint: wooden artist palette & brush */
            for(int py2=6;py2<=26;py2++){
                int wspan=(py2<10||py2>22)?18:24;
                int xoff=(py2<10||py2>22)?6:3;
                gui_rect(x+xoff,y+py2,wspan,1,0xD2A679);
            }
            /* Outline */
            gui_hline(x+6,y+5,18,0xF0D0A0);gui_vline(x+3,y+9,14,0xF0D0A0);
            gui_hline(x+6,y+27,18,0x805020);gui_vline(x+27,y+9,14,0x805020);
            /* Thumb hole */
            gui_rect(x+19,y+18,5,4,desktop_color);
            gui_hline(x+19,y+18,5,0x604010);gui_vline(x+19,y+18,4,0x604010);
            /* Paint blobs */
            gui_rect(x+6,y+9,5,4,0xFF0000);gui_pixel(x+7,y+9,0xFFAAAA);
            gui_rect(x+13,y+7,5,4,0xFFFF00);gui_pixel(x+14,y+7,0xFFFFFF);
            gui_rect(x+20,y+9,5,4,0x0055FF);gui_pixel(x+21,y+9,0x88CCFF);
            gui_rect(x+7,y+17,5,4,0x00BB00);gui_pixel(x+8,y+17,0x88FF88);
            gui_rect(x+11,y+22,5,4,0xFFFFFF);
            /* Paintbrush */
            for(int bp=0;bp<12;bp++){gui_pixel(x+27-bp,y+4+bp,0x8B4513);}
            gui_pixel(x+15,y+16,0xC0C0C0);gui_pixel(x+14,y+17,0x303030);gui_pixel(x+13,y+18,0x0055FF);
        } else if(d->app_id==APP_TERMINAL){
            /* Win98 MS-DOS prompt window */
            gui_rect(x+5,y+6,26,24,0x000000); /* shadow */
            gui_rect(x+3,y+4,26,24,0xC0C0C0);
            gui_hline(x+3,y+4,26,0xFFFFFF);gui_vline(x+3,y+4,24,0xFFFFFF);
            gui_hline(x+3,y+27,26,0x404040);gui_vline(x+28,y+4,24,0x404040);
            /* Titlebar */
            gui_rect(x+5,y+6,22,3,0x000080);
            gui_rect(x+24,y+6,2,2,0xC0C0C0);
            /* CRT screen */
            gui_rect(x+5,y+10,22,16,0x000000);
            gui_print("C:",x+7,y+12,0xFFFFFF);
            gui_print(">",x+7,y+19,0xFFFFFF);
            gui_rect(x+15,y+21,5,2,0xFFFFFF);
        } else {
            /* Welcome / Info: 3D blue Windows sphere */
            for(int ry=-12;ry<=12;ry++){
                int rx_=(int)(12-(ry<0?-ry:ry)/3);
                if(rx_<1)rx_=1;
                u32 sc = (ry<-4)?0x3090FF:(ry<4?0x1060D0:0x083890);
                gui_hline(x+16-rx_,y+16+ry,rx_*2+1,sc);
            }
            /* Specular shine */
            gui_rect(x+11,y+9,4,3,0xFFFFFF);
            /* White "i" */
            gui_rect(x+15,y+10,3,3,0xFFFFFF);
            gui_rect(x+15,y+15,3,7,0xFFFFFF);
            gui_hline(x+13,y+21,7,0xFFFFFF);
            gui_hline(x+13,y+15,3,0xFFFFFF);
        }
        draw_desk_label(d->label,x+18,y+38,desk_sel==i);
    }
}

void open_app(int app_id){
    switch(app_id){
        case APP_PAINT:{int wi=win_new(APP_PAINT,100,50,560,400,"Paint");if(wi>=0)paint_clear();break;}
        case APP_NOTEPAD:{int wi=win_new(APP_NOTEPAD,120,60,540,390,"Notepad");if(wi>=0){note_clear();note_active=1;term_active=0;}break;}
        case APP_TASKMGR:win_new(APP_TASKMGR,140,70,440,350,"Task Manager");break;
        case APP_TERMINAL:{int wi=win_new(APP_TERMINAL,110,80,480,320,"Terminal");if(wi>=0){term_clear_screen();term_print("BrickOS Terminal\n$ ");term_active=1;note_active=0;}break;}
        case APP_SYSINFO:win_new(APP_SYSINFO,140,50,480,330,"System Info");break;
        case APP_EXPLORER:win_new(APP_EXPLORER,150,60,480,350,"My Computer");break;
        case APP_CTRLPANEL:win_new(APP_CTRLPANEL,140,50,520,380,"Control Panel");break;
        case APP_WELCOME:win_new(APP_WELCOME,180,80,420,250,"Welcome");break;
        default:break;
    }
}

/* ══════════════════════════════════════
   TASKBAR + START MENU
   ══════════════════════════════════════ */
static int start_menu=0;
static const char*menu_items[]={"Paint","Notepad","Task Mgr","Terminal","My Computer","Ctrl Panel","----------","Reboot"};
#define MENU_N 8
#define MENU_IH 18

void draw_taskbar(void){
    int ty=fh-TASKBAR_H;
    gui_rect(0,ty,fw,TASKBAR_H,0xC0C0C0);
    gui_hline(0,ty,fw,0xFFFFFF);gui_hline(0,ty+1,fw,0xDFDFDF);gui_hline(0,fh-1,fw,0x808080);
    /* Start button */
    int sbp=(mouse_btn&&mouse_x>2&&mouse_x<72&&mouse_y>ty+2);
    gui_btn(2,ty+2,68,TASKBAR_H-4,"Start",sbp);
    /* Real CMOS Clock */
    char rtc_time[10];
    get_rtc_time(rtc_time);
    int ul=gui_str_width(rtc_time);
    int tray_w=ul+16;
    gui_rect(fw-tray_w-4,ty+3,tray_w,TASKBAR_H-6,0xC0C0C0);
    gui_hline(fw-tray_w-4,ty+3,tray_w,0x808080);gui_vline(fw-tray_w-4,ty+3,TASKBAR_H-6,0x808080);
    gui_hline(fw-tray_w-4,ty+TASKBAR_H-4,tray_w,0xFFFFFF);gui_vline(fw-5,ty+3,TASKBAR_H-6,0xFFFFFF);
    gui_print(rtc_time,fw-tray_w+4,ty+5,0x000000);
    /* Window buttons (115px wide so full names fit) */
    int bx=74,btn_w=115;
    for(int i=0;i<nwins;i++){
        if(bx+btn_w>fw-tray_w-10)break;
        int foc=(i==nwins-1);
        int is_min=wins[i].minimized;
        u32 bbg=is_min?0xA8A8A8:(foc?0xA0A0A0:0xC0C0C0);
        gui_rect(bx,ty+3,btn_w-4,TASKBAR_H-6,bbg);
        if(foc||is_min){gui_hline(bx,ty+3,btn_w-4,0x808080);gui_vline(bx,ty+3,TASKBAR_H-6,0x808080);gui_hline(bx,ty+TASKBAR_H-4,btn_w-4,0xFFFFFF);gui_vline(bx+btn_w-5,ty+3,TASKBAR_H-6,0xFFFFFF);}
        else{gui_hline(bx,ty+3,btn_w-4,0xFFFFFF);gui_vline(bx,ty+3,TASKBAR_H-6,0xFFFFFF);gui_hline(bx,ty+TASKBAR_H-4,btn_w-4,0x808080);gui_vline(bx+btn_w-5,ty+3,TASKBAR_H-6,0x808080);}
        char tb[20];int tl=0;
        while(wins[i].title[tl]&&tl<18){tb[tl]=wins[i].title[tl];tl++;}tb[tl]=0;
        int max_tw=btn_w-12;
        while(tl>3&&gui_str_width(tb)>max_tw){tb[--tl]=0;}
        int tw=gui_str_width(tb);
        int tx_pos=bx+(btn_w-4-tw)/2;
        int ty_pos=ty+3+(TASKBAR_H-6-(font_loaded?sys_font->height:16))/2;
        gui_print(tb,tx_pos,ty_pos,0x000000);
        bx+=btn_w;
    }
    /* Start menu */
    if(start_menu){
        int mh=MENU_N*MENU_IH+6,mw=140,mmx=2,mmy=ty-mh;
        gui_rect(mmx,mmy,mw,mh,0xC0C0C0);
        gui_hline(mmx,mmy,mw,0xFFFFFF);gui_vline(mmx,mmy,mh,0xFFFFFF);
        gui_vline(mmx+mw-1,mmy,mh,0x808080);gui_hline(mmx,mmy+mh-1,mw,0x808080);
        /* FlatronTech side strip */
        gui_rect(mmx+2,mmy+2,18,mh-4,0x000080);
        for(int i=0;i<MENU_N;i++){
            int iy=mmy+3+i*MENU_IH;
            int hov=(mouse_x>=mmx+20&&mouse_x<mmx+mw-2&&mouse_y>=iy&&mouse_y<iy+MENU_IH);
            if(hov)gui_rect(mmx+20,iy,mw-22,MENU_IH,0x000080);
            if(menu_items[i][0]=='-')gui_hline(mmx+20,iy+MENU_IH/2,mw-22,0x808080);
            else gui_print(menu_items[i],mmx+24,iy+1,hov?0xFFFFFF:0x000000);
        }
    }
}

/* ══════════════════════════════════════
   KEY DISPATCH
   ══════════════════════════════════════ */
void dispatch_key(char c){
    for(int i=nwins-1;i>=0;i--){
        if(!wins[i].visible||wins[i].minimized)continue;
        switch(wins[i].app_id){
            case APP_SAVEAS:   saveas_key(c);return;
            case APP_NOTEPAD:  notepad_key(c,&wins[i]);note_active=1;return;
            case APP_TERMINAL: terminal_key(c);term_active=1;return;
            default:return;
        }
    }
}

/* ══════════════════════════════════════
   MAIN GUI LOOP & CONTEXT MENUS
   ══════════════════════════════════════ */
#define CTX_NONE     0
#define CTX_DESKTOP  1
#define CTX_ICON     2
#define CTX_FILE     3

static int ctx_open=0;
static int ctx_type=CTX_NONE;
static int ctx_x=0,ctx_y=0;
static int ctx_target=-1;
static char ctx_file[16]="";
static int ctx_item_count=0;
static char ctx_items[5][24];

static int resize_win=-1,resize_ox=0,resize_oy=0;

void run_gui(void){
    esc_pressed=0;start_menu=0;ctx_open=0;
    nwins=0;
    open_app(APP_WELCOME);
    paint_clear();note_clear();term_clear_screen();
    term_print("BrickOS Terminal\nType 'help' for list\n");
    mouse_btn=0;mouse_cycle=0;
    int last_btn=0;

    while(!esc_pressed){
        char kc=kbd_try();if(kc)dispatch_key(kc);

        /* ── Desktop ── */
        gui_rect(0,0,fw,fh-TASKBAR_H,desktop_color);

        /* simple grid pattern on desktop */
        for(int gx=0;gx<fw;gx+=32)gui_vline(gx,0,fh-TASKBAR_H,(desktop_color&0xFEFEFE)>>1|(desktop_color&0x010101));

        draw_desk_icons();

        /* ── Windows ── */
        for(int i=0;i<nwins;i++){
            if(!wins[i].visible||wins[i].minimized)continue;
            struct Window*w=&wins[i];
            int foc=(i==nwins-1);
            draw_win_chrome(w,foc);
            switch(w->app_id){
                case APP_SYSINFO:  draw_sysinfo(w);break;
                case APP_EXPLORER: draw_explorer(w);break;
                case APP_WELCOME:  draw_welcome(w);break;
                case APP_PAINT:    draw_paint(w,mouse_x,mouse_y,mouse_btn);break;
                case APP_NOTEPAD:  draw_notepad(w);break;
                case APP_TASKMGR:  draw_taskmgr(w);break;
                case APP_TERMINAL: draw_terminal(w);break;
                case APP_CTRLPANEL:draw_ctrlpanel(w);break;
                case APP_FILEPROPS:draw_fileprops(w);break;
                case APP_BMPVIEW:  draw_bmpview(w);break;
                case APP_SAVEAS:   draw_saveas(w);break;
            }
            if(w->app_state==99){
                int wdx=w->x+w->w/2-110,wdy=w->y+w->h/2-45;
                gui_rect(wdx,wdy,220,90,0xC0C0C0);
                gui_hline(wdx,wdy,220,0xFFFFFF);gui_vline(wdx,wdy,90,0xFFFFFF);
                gui_hline(wdx,wdy+89,220,0x000000);gui_vline(wdx+219,wdy,90,0x000000);
                gui_print("Unsaved changes!",wdx+10,wdy+10,0x000000);
                gui_print("Save before closing?",wdx+10,wdy+26,0x000000);
                gui_btn(wdx+10,wdy+54,60,24,"Save",0);
                gui_btn(wdx+80,wdy+54,60,24,"Discard",0);
                gui_btn(wdx+150,wdy+54,60,24,"Cancel",0);
            }
        }

        draw_taskbar();

        /* ── Context Menu (if open) ── */
        if(ctx_open){
            int mw=140,mh=ctx_item_count*20+4;
            int cx_pos=ctx_x,cy_pos=ctx_y;
            if(cx_pos+mw>fw)cx_pos=fw-mw-2;
            if(cy_pos+mh>fh-TASKBAR_H)cy_pos=fh-TASKBAR_H-mh-2;

            gui_rect(cx_pos,cy_pos,mw,mh,0xC0C0C0);
            gui_hline(cx_pos,cy_pos,mw,0xFFFFFF);
            gui_vline(cx_pos,cy_pos,mh,0xFFFFFF);
            gui_hline(cx_pos,cy_pos+mh-1,mw,0x000000);
            gui_vline(cx_pos+mw-1,cy_pos,mh,0x000000);
            gui_hline(cx_pos+1,cy_pos+mh-2,mw-2,0x808080);
            gui_vline(cx_pos+mw-2,cy_pos+1,mh-2,0x808080);

            for(int it=0;it<ctx_item_count;it++){
                int iy=cy_pos+2+it*20;
                int hovered=(mouse_x>=cx_pos+2&&mouse_x<cx_pos+mw-2&&mouse_y>=iy&&mouse_y<iy+20);
                if(hovered){
                    gui_rect(cx_pos+2,iy,mw-4,20,0x000080);
                    gui_print(ctx_items[it],cx_pos+8,iy+2,0xFFFFFF);
                }else{
                    gui_print(ctx_items[it],cx_pos+8,iy+2,0x000000);
                }
            }
        }

        int mx=mouse_x,my=mouse_y;
        int left_click=(mouse_btn&1)&&!(last_btn&1);
        int right_click=(mouse_btn&2)&&!(last_btn&2);

        /* ── Right Click (Context Menu trigger) ── */
        if(right_click){
            ctx_open=0;
            int handled=0;
            /* Check if right-clicked inside an active Explorer window file row */
            for(int i=nwins-1;i>=0;i--){
                if(!wins[i].visible||wins[i].minimized)continue;
                struct Window*w=&wins[i];
                if(mx>=w->x&&mx<w->x+w->w&&my>=w->y&&my<w->y+w->h){
                    win_raise(i);
                    w=&wins[nwins-1];
                    if(w->app_id==APP_EXPLORER&&w->app_state==1){
                        int px=win_client_x(w),py0=win_client_y(w);
                        int hy=py0+18;
                        if(my>=hy+16){
                            int f_idx=(my-(hy+16))/16;
                            u8*root=ramdisk+root_sect*512;int cnt=0;
                            for(int k=0;k<root_cnt;k++){
                                u8*e=root+k*32;
                                if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;
                                if(cnt==f_idx){
                                    char nm[14];int ni=0;
                                    for(int j=0;j<8&&e[j]!=' ';j++)nm[ni++]=e[j];
                                    nm[ni++]='.';for(int j=8;j<11&&e[j]!=' ';j++)nm[ni++]=e[j];nm[ni]=0;
                                    my_strcpy(ctx_file,nm);
                                    ctx_type=CTX_FILE;ctx_x=mx;ctx_y=my;
                                    int is_bin=(e[8]=='B'&&e[9]=='I'&&e[10]=='N');
                                    int is_bmp2=(e[8]=='B'&&e[9]=='M'&&e[10]=='P');
                                    if(is_bin){
                                        ctx_item_count=1;
                                        my_strcpy(ctx_items[0],"Properties");
                                    }else if(is_bmp2){
                                        ctx_item_count=3;
                                        my_strcpy(ctx_items[0],"Open in Viewer");
                                        my_strcpy(ctx_items[1],"Open in Notepad");
                                        my_strcpy(ctx_items[2],"Properties");
                                    }else{
                                        ctx_item_count=2;
                                        my_strcpy(ctx_items[0],"Open in Notepad");
                                        my_strcpy(ctx_items[1],"Properties");
                                    }
                                    ctx_open=1;handled=1;break;
                                }
                                cnt++;
                            }
                        }
                    }
                    if(handled)break;
                    handled=1;break;
                }
            }
            /* Check desktop icons */
            if(!handled&&my<fh-TASKBAR_H){
                for(int i=0;i<NDESK_ICONS;i++){
                    struct DesktopIcon*d=&desk_icons[i];
                    int cx=d->x+18;
                    if(mx>=cx-32&&mx<cx+32&&my>=d->y&&my<d->y+70){
                        ctx_type=CTX_ICON;ctx_target=i;ctx_x=mx;ctx_y=my;
                        ctx_item_count=2;
                        my_strcpy(ctx_items[0],"Open");
                        my_strcpy(ctx_items[1],"Properties");
                        ctx_open=1;handled=1;break;
                    }
                }
            }
            /* Empty desktop */
            if(!handled&&my<fh-TASKBAR_H){
                ctx_type=CTX_DESKTOP;ctx_x=mx;ctx_y=my;
                ctx_item_count=4;
                my_strcpy(ctx_items[0],"Personalize");
                my_strcpy(ctx_items[1],"New Note");
                my_strcpy(ctx_items[2],"Terminal");
                my_strcpy(ctx_items[3],"My Computer");
                ctx_open=1;
            }
            goto input_done;
        }

        /* ── Left Click on Context Menu ── */
        if(ctx_open&&left_click){
            int mw=140,mh=ctx_item_count*20+4;
            int cx_pos=ctx_x,cy_pos=ctx_y;
            if(cx_pos+mw>fw)cx_pos=fw-mw-2;
            if(cy_pos+mh>fh-TASKBAR_H)cy_pos=fh-TASKBAR_H-mh-2;
            if(mx>=cx_pos&&mx<cx_pos+mw&&my>=cy_pos&&my<cy_pos+mh){
                int item=(my-(cy_pos+2))/20;
                if(item>=0&&item<ctx_item_count){
                    if(ctx_type==CTX_DESKTOP){
                        if(item==0){open_app(APP_CTRLPANEL);ctrlpanel_tab=2;}
                        else if(item==1){open_app(APP_NOTEPAD);}
                        else if(item==2){open_app(APP_TERMINAL);}
                        else if(item==3){open_app(APP_EXPLORER);}
                    }else if(ctx_type==CTX_ICON){
                        if(item==0){open_app(desk_icons[ctx_target].app_id);}
                        else if(item==1){open_app(APP_CTRLPANEL);ctrlpanel_tab=0;}
                    }else if(ctx_type==CTX_FILE){
                        if(item==0&&ctx_items[0][0]=='O'){
                            /* "Open in Viewer" or "Open in Notepad" */
                            if(ctx_items[0][8]=='V')open_bmp_viewer(ctx_file);
                            else open_file_in_notepad(ctx_file);
                        }else if(item==1&&ctx_items[1][0]=='O'){
                            open_file_in_notepad(ctx_file);
                        }else{
                            /* Properties - look up file info in FAT */
                            u8*root=ramdisk+root_sect*512;
                            char n83[11];fat_name83(ctx_file,n83);
                            for(int k=0;k<root_cnt;k++){
                                u8*e=root+k*32;
                                if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;
                                int ok=1;for(int j=0;j<11;j++)if(e[j]!=n83[j])ok=0;if(!ok)continue;
                                u32 fsz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
                                int fcl=e[26]|(e[27]<<8);
                                int nclust=0;int cl2=fcl;
                                int end_mark=is_fat16?0xFFF8:0xFF8;
                                while(cl2>=2&&cl2<end_mark&&nclust<1000){nclust++;cl2=fat_get(cl2);}
                                const char*fty="Data File";
                                if(e[8]=='T'&&e[9]=='X'&&e[10]=='T')fty="Text File";
                                else if(e[8]=='I'&&e[9]=='N'&&e[10]=='F')fty="INF Driver";
                                else if(e[8]=='B'&&e[9]=='I'&&e[10]=='N')fty="Binary";
                                else if(e[8]=='B'&&e[9]=='M'&&e[10]=='P')fty="Bitmap Image";
                                open_file_props(ctx_file,fsz,fty,nclust);
                                break;
                            }
                        }
                    }
                }
            }
            ctx_open=0;goto input_done;
        }
        if(left_click)ctx_open=0;

        /* ── Normal Left Click ── */
        if(left_click){
            int ty=fh-TASKBAR_H;

            /* Start menu items */
            if(start_menu){
                int mh=MENU_N*MENU_IH+6,mw=140,mmx=2,mmy=ty-mh;
                if(mx>=mmx&&mx<mmx+mw&&my>=mmy&&my<ty){
                    int item=(my-mmy-3)/MENU_IH;
                    if(item>=0&&item<MENU_N&&menu_items[item][0]!='-'){
                        start_menu=0;
                        switch(item){
                            case 0:open_app(APP_PAINT);break;
                            case 1:open_app(APP_NOTEPAD);break;
                            case 2:open_app(APP_TASKMGR);break;
                            case 3:open_app(APP_TERMINAL);break;
                            case 4:open_app(APP_EXPLORER);break;
                            case 5:open_app(APP_CTRLPANEL);break;
                            case 7:outb(0x64,0xFE);break;
                        }
                    } else start_menu=0;
                } else start_menu=0;
                goto input_done;
            }

            /* Start button */
            if(mx>=2&&mx<70&&my>ty+2){start_menu=!start_menu;goto input_done;}

            /* Taskbar window buttons */
            {int bx=74,btn_w=115;
            for(int i=0;i<nwins;i++){
                if(bx+btn_w>fw-100)break;
                if(mx>=bx&&mx<bx+btn_w-4&&my>ty+3){
                    if(wins[i].minimized){wins[i].minimized=0;win_raise(i);}
                    else if(i==nwins-1){wins[i].minimized=1;}
                    else{win_raise(i);}
                    goto input_done;
                }
                bx+=btn_w;
            }}

            start_menu=0;

            /* Window resize hit test (bottom-right handle or borders) */
            int resize_hit=-1;
            for(int i=nwins-1;i>=0;i--){
                if(!wins[i].visible||wins[i].minimized||wins[i].maximized)continue;
                struct Window*w=&wins[i];
                int br=(mx>=w->x+w->w-16&&mx<=w->x+w->w&&my>=w->y+w->h-16&&my<=w->y+w->h);
                int rb=(mx>=w->x+w->w-4&&mx<=w->x+w->w&&my>=w->y&&my<=w->y+w->h);
                int bb=(my>=w->y+w->h-4&&my<=w->y+w->h&&mx>=w->x&&mx<=w->x+w->w);
                if(br||rb||bb){resize_hit=i;break;}
            }
            if(resize_hit>=0){
                win_raise(resize_hit);
                resize_win=nwins-1;
                resize_ox=wins[resize_win].w-(mx-wins[resize_win].x);
                resize_oy=wins[resize_win].h-(my-wins[resize_win].y);
                goto input_done;
            }

            /* Desktop icon click */
            {int icon_hit=-1;
            for(int i=0;i<NDESK_ICONS;i++){
                struct DesktopIcon*d=&desk_icons[i];
                int cx=d->x+18;
                if(mx>=cx-32&&mx<cx+32&&my>=d->y&&my<d->y+70){icon_hit=i;break;}
            }
            if(icon_hit>=0){
                if(icon_hit==desk_last&&ticks-desk_last_t<50){open_app(desk_icons[icon_hit].app_id);desk_sel=-1;}
                else desk_sel=icon_hit;
                desk_last=icon_hit;desk_last_t=ticks;
                goto input_done;
            } else desk_sel=-1;}

            /* Window hit test */
            for(int i=nwins-1;i>=0;i--){
                if(!wins[i].visible||wins[i].minimized)continue;
                struct Window*w=&wins[i];
                if(mx<w->x||mx>=w->x+w->w||my<w->y||my>=w->y+w->h)continue;
                /* Close? */
                if(win_close_hit(w,mx,my)){
                    int is_note=(w->app_id==APP_NOTEPAD);
                    int is_paint=(w->app_id==APP_PAINT);
                    int dirty_warn=(is_note&&note_dirty)||(is_paint&&paint_dirty);
                    if(dirty_warn&&w->app_state!=99){
                        /* Mark window with close-pending state - show warning */
                        w->app_state=99;break;
                    }
                    if(w->app_id==APP_NOTEPAD){note_active=0;note_dirty=0;}
                    if(w->app_id==APP_TERMINAL)term_active=0;
                    if(w->app_id==APP_PAINT)paint_dirty=0;
                    for(int j=i;j<nwins-1;j++)wins[j]=wins[j+1];nwins--;break;
                }
                /* Dirty close warning overlay */
                if(w->app_state==99){
                    int wdx=w->x+w->w/2-110,wdy=w->y+w->h/2-45;
                    if(mx>=wdx+10&&mx<wdx+70&&my>=wdy+54&&my<wdy+78){
                        if(w->app_id==APP_NOTEPAD){note_save();note_active=0;note_dirty=0;}
                        else if(w->app_id==APP_PAINT){save_paint_bmp();paint_dirty=0;}
                        for(int j=i;j<nwins-1;j++)wins[j]=wins[j+1];nwins--;break;
                    }
                    if(mx>=wdx+80&&mx<wdx+140&&my>=wdy+54&&my<wdy+78){
                        if(w->app_id==APP_NOTEPAD){note_active=0;note_dirty=0;}
                        if(w->app_id==APP_PAINT)paint_dirty=0;
                        for(int j=i;j<nwins-1;j++)wins[j]=wins[j+1];nwins--;break;
                    }
                    if(mx>=wdx+150&&mx<wdx+210&&my>=wdy+54&&my<wdy+78){w->app_state=0;break;}
                    break;
                }
                /* Maximize? */
                if(win_max_hit(w,mx,my)){if(!w->maximized){w->prev_x=w->x;w->prev_y=w->y;w->prev_w=w->w;w->prev_h=w->h;w->x=0;w->y=0;w->w=fw;w->h=fh-TASKBAR_H;w->maximized=1;}else{w->x=w->prev_x;w->y=w->prev_y;w->w=w->prev_w;w->h=w->prev_h;w->maximized=0;}win_raise(i);break;}
                /* Minimize? */
                if(win_min_hit(w,mx,my)){w->minimized=1;if(w->app_id==APP_NOTEPAD)note_active=0;if(w->app_id==APP_TERMINAL)term_active=0;break;}
                /* Titlebar drag */
                if(my>=w->y+BORDER&&my<=w->y+BORDER+TITLEBAR_H){drag_win=i;drag_ox=mx-w->x;drag_oy=my-w->y;win_raise(i);drag_win=nwins-1;if(wins[drag_win].app_id==APP_NOTEPAD){note_active=1;term_active=0;}if(wins[drag_win].app_id==APP_TERMINAL){term_active=1;note_active=0;}break;}
                /* Client area click */
                win_raise(i);
                struct Window*wt=&wins[nwins-1];
                if(wt->app_id==APP_NOTEPAD){note_active=1;term_active=0;notepad_click(wt,mx,my);}
                if(wt->app_id==APP_TERMINAL){term_active=1;note_active=0;}
                if(wt->app_id==APP_CTRLPANEL)ctrlpanel_click(wt,mx,my);
                if(wt->app_id==APP_EXPLORER)explorer_click(wt,mx,my);
                if(wt->app_id==APP_FILEPROPS){if(fileprops_click(wt,mx,my))nwins--;}
                if(wt->app_id==APP_SAVEAS)saveas_click(wt,mx,my);
                break;
            }
            /* Close button with unsaved warning */
            /* (handled above in Close? section, additional dirty check done inline) */
        }

        if(!(mouse_btn&1)){
            drag_win=-1;
            resize_win=-1;
        }
        if(resize_win>=0&&resize_win<nwins&&(mouse_btn&1)){
            int nw=(mouse_x-wins[resize_win].x)+resize_ox;
            int nh=(mouse_y-wins[resize_win].y)+resize_oy;
            if(nw<220)nw=220;if(nh<140)nh=140;
            if(wins[resize_win].x+nw>fw)nw=fw-wins[resize_win].x;
            if(wins[resize_win].y+nh>fh-TASKBAR_H)nh=(fh-TASKBAR_H)-wins[resize_win].y;
            wins[resize_win].w=nw;wins[resize_win].h=nh;
        }
        if(drag_win>=0&&drag_win<nwins&&(mouse_btn&1)){
            if(!wins[drag_win].maximized){
                wins[drag_win].x=mouse_x-drag_ox;wins[drag_win].y=mouse_y-drag_oy;
                if(wins[drag_win].x<0)wins[drag_win].x=0;if(wins[drag_win].y<0)wins[drag_win].y=0;
                if(wins[drag_win].x+wins[drag_win].w>fw)wins[drag_win].x=fw-wins[drag_win].w;
                if(wins[drag_win].y+wins[drag_win].h>fh-TASKBAR_H)wins[drag_win].y=fh-TASKBAR_H-wins[drag_win].h;
            }
        }
        input_done:
        /* ── Link cursor detection ── */
        {
            int lmx=mouse_x,lmy=mouse_y;
            cursor_is_link=0;
            int ty2=fh-TASKBAR_H;
            /* Taskbar: start button and window buttons */
            if(lmy>ty2&&lmy<fh&&lmx>0&&lmx<fw)cursor_is_link=1;
            /* Desktop icons */
            if(!cursor_is_link&&lmy<ty2){
                for(int ci=0;ci<NDESK_ICONS&&!cursor_is_link;ci++){
                    struct DesktopIcon*d=&desk_icons[ci];
                    int cx=d->x+18;
                    if(lmx>=cx-32&&lmx<cx+32&&lmy>=d->y&&lmy<d->y+70)cursor_is_link=1;
                }
            }
            /* Context menu items */
            if(!cursor_is_link&&ctx_open){
                int cmw=140,cmh=ctx_item_count*20+4;
                int cpx=ctx_x,cpy=ctx_y;
                if(cpx+cmw>fw)cpx=fw-cmw-2;if(cpy+cmh>fh-TASKBAR_H)cpy=fh-TASKBAR_H-cmh-2;
                if(lmx>=cpx&&lmx<cpx+cmw&&lmy>=cpy&&lmy<cpy+cmh)cursor_is_link=1;
            }
            /* Window titlebar buttons (close/min/max) */
            if(!cursor_is_link){
                for(int wi=0;wi<nwins&&!cursor_is_link;wi++){
                    if(!wins[wi].visible||wins[wi].minimized)continue;
                    struct Window*ww=&wins[wi];
                    if(win_close_hit(ww,lmx,lmy)||win_max_hit(ww,lmx,lmy)||win_min_hit(ww,lmx,lmy))cursor_is_link=1;
                }
            }
            /* Interactive controls in topmost window */
            if(!cursor_is_link&&nwins>0){
                struct Window*wt=&wins[nwins-1];
                if(wt->visible&&!wt->minimized){
                    int px=win_client_x(wt),py0=win_client_y(wt),cw=win_client_w(wt);
                    if(wt->app_state==99){
                        int wdx=wt->x+wt->w/2-110,wdy=wt->y+wt->h/2-45;
                        if(lmx>=wdx+10&&lmx<wdx+210&&lmy>=wdy+54&&lmy<wdy+78)cursor_is_link=1;
                    }else if(wt->app_id==APP_NOTEPAD){
                        int sbx=px+cw-116,sby=py0+2;
                        int sabx=px+cw-62,saby=py0+2;
                        if(lmx>=sbx&&lmx<sbx+50&&lmy>=sby&&lmy<sby+16)cursor_is_link=1;
                        if(lmx>=sabx&&lmx<sabx+58&&lmy>=saby&&lmy<saby+16)cursor_is_link=1;
                    }else if(wt->app_id==APP_PAINT){
                        int cbx=px+cw-170,cby=py0+2;
                        int sbx=px+cw-118,sby=py0+2;
                        int sabx=px+cw-60,saby=py0+2;
                        if(lmy>=py0+3&&lmy<py0+17&&lmx>=px+58&&lmx<px+58+PAINT_NC*16)cursor_is_link=1;
                        if(lmx>=cbx&&lmx<cbx+48&&lmy>=cby&&lmy<cby+18)cursor_is_link=1;
                        if(lmx>=sbx&&lmx<sbx+54&&lmy>=sby&&lmy<sby+18)cursor_is_link=1;
                        if(lmx>=sabx&&lmx<sabx+56&&lmy>=saby&&lmy<saby+18)cursor_is_link=1;
                    }else if(wt->app_id==APP_EXPLORER){
                        if(wt->app_state==0){
                            int dx=px+30,dy=py0+38;
                            if(lmx>=dx&&lmx<dx+50&&lmy>=dy&&lmy<dy+60)cursor_is_link=1;
                        }else{
                            if(lmx>=px&&lmx<px+32&&lmy>=py0&&lmy<py0+16)cursor_is_link=1;
                            int hy=py0+18;
                            if(lmy>=hy+16&&lmy<hy+16+16*12&&lmx>=px&&lmx<px+cw)cursor_is_link=1;
                        }
                    }else if(wt->app_id==APP_CTRLPANEL){
                        if(lmy>=py0&&lmy<py0+18&&lmx>=px&&lmx<px+200)cursor_is_link=1;
                        if(ctrlpanel_tab==1){
                            int cy2=py0+24+20+16*5+20+16;
                            if(lmx>=px+10&&lmx<px+240&&lmy>=cy2&&lmy<cy2+22)cursor_is_link=1;
                            if(lmx>=px+40&&lmx<px+200&&lmy>=cy2+28&&lmy<cy2+52)cursor_is_link=1;
                        }else if(ctrlpanel_tab==2){
                            int cy2=py0+24+20+18;
                            for(int i=0;i<N_DESK_PAL;i++){
                                int sw=36,sh=26;int bx=px+(i%4)*(sw+8),by2=cy2+(i/4)*(sh+8);
                                if(lmx>=bx&&lmx<bx+sw&&lmy>=by2&&lmy<by2+sh)cursor_is_link=1;
                            }
                        }
                    }else if(wt->app_id==APP_SAVEAS){
                        int by=py0+60;
                        if(lmx>=px+cw-140&&lmx<px+cw-10&&lmy>=by&&lmy<by+22)cursor_is_link=1;
                    }else if(wt->app_id==APP_FILEPROPS){
                        int ly=fileprops_ok_y(wt);
                        if(lmx>=px+cw/2-30&&lmx<px+cw/2+30&&lmy>=ly&&lmy<ly+20)cursor_is_link=1;
                    }
                }
            }
            /* Start menu */
            if(!cursor_is_link&&start_menu){
                int ty3=fh-TASKBAR_H;
                int smh=MENU_N*MENU_IH+6,smw=140,smx=2,smy=ty3-smh;
                if(lmx>=smx+20&&lmx<smx+smw&&lmy>=smy&&lmy<ty3)cursor_is_link=1;
            }
        }
        draw_cursor(mouse_x,mouse_y);gui_flip();last_btn=mouse_btn;
        __asm__ volatile("hlt");
    }
    con_clear();
}

/* ══════════════════════════════════════
   CONSOLE INIT + CLI
   ══════════════════════════════════════ */
void console_init(void){
    bi_fb=*(u32*)0x704;
    if(*(u16*)0x708>=640&&*(u16*)0x70A>=480){fw=*(u16*)0x708;fh=*(u16*)0x70A;}
    bi_pitch=*(u16*)0x70C;bi_bpp=*(u8*)0x70E;bi_vesa=*(u8*)0x70F;
    bi_font=*(u32*)0x710;
    if(bi_vesa&&bi_bpp>=24&&bi_fb>=0xA0000&&bi_pitch>=640){
        use_fb=1;fb=(u8*)(unsigned long)bi_fb;fpitch=bi_pitch;fbpp=bi_bpp;
        cols=fw/8;rows=(fh-16)/16;if(cols>160)cols=160;if(rows>80)rows=80;COL_BG=0x401010;COL_FG=0xFFFFFF;COL_BAR=0xA0A0A0;
    }
    con_clear();
}
void cmd_meminfo(void){u32 cnt=*(u32*)0x71C;u8*m=(u8*)0x720;if(cnt>32)cnt=32;con_print("E820 regions: ");con_dec(cnt);con_char('\n');for(u32 i=0;i<cnt;i++){u8*e=m+i*20;u32 bl=e[0]|e[1]<<8|e[2]<<16|e[3]<<24;u32 ll=e[8]|e[9]<<8|e[10]<<16|e[11]<<24;u32 ty=e[16]|e[17]<<8|e[18]<<16|e[19]<<24;con_print("  base=");const char*h="0123456789ABCDEF";for(int q=28;q>=0;q-=4)con_char(h[(bl>>q)&15]);con_print(" len=");for(int q=28;q>=0;q-=4)con_char(h[(ll>>q)&15]);con_print(" type=");con_dec(ty);con_char('\n');}}
void cmd_sysinfo(void){char b[50];get_cpu_brand(b,50);con_print("CPU: ");con_print(b);con_char('\n');}
void cmd_vesainfo(void){
    con_print("VESA / Framebuffer Status:\n");
    con_print("  Active GUI (use_fb): "); con_dec(use_fb); con_char('\n');
    con_print("  VESA flag  (0x70F) : "); con_dec(bi_vesa); con_char('\n');
    con_print("  Last Mode  (0x702) : 0x"); con_hex16(*(u16*)0x702); con_char('\n');
    con_print("  Resolution         : "); con_dec(*(u16*)0x708); con_print(" x "); con_dec(*(u16*)0x70A); con_char('\n');
    con_print("  Color Depth(0x70E) : "); con_dec(bi_bpp); con_print(" bpp\n");
    con_print("  Scan Pitch (0x70C) : "); con_dec(bi_pitch); con_print(" bytes\n");
    con_print("  PhysBasePtr(0x704) : 0x"); con_hex(bi_fb); con_char('\n');
    if(!bi_vesa){
        con_print("  Reason: BIOS Set/Get VBE mode failed in bootloader.\n");
    } else if(bi_bpp < 24){
        con_print("  Reason: Color depth < 24 bpp (unsupported).\n");
    } else if(bi_fb < 0xA0000){
        con_print("  Reason: Invalid framebuffer base pointer (< 0xA0000).\n");
    } else if(bi_pitch < 640){
        con_print("  Reason: Invalid pitch (< 640).\n");
    } else {
        con_print("  Status: Framebuffer valid and ready.\n");
    }
}
void kmain(void){
    console_init();interrupts_init();fat_init();
    if(fat_read_file("FONT.BIN",(u8*)sys_font,sizeof(struct LoadedFont))>0){
        font_loaded=1;
    }
    con_print("BrickOS booting...\n");
    if(use_fb){
        boot_anim();
        run_gui();
    } else {
        con_print("WARNING: Falling back to text mode (VESA not active).\n");
        con_print("Type 'gui' or 'vesainfo' for details.\n");
    }
    con_print("brickos> ");
    char cmd[64];int len=0;
    while(1){
        char c=kbd_get();
        if(c=='\n'){cmd[len]=0;con_char('\n');
            for(int ci=0;ci<len;ci++)if(cmd[ci]>='A'&&cmd[ci]<='Z')cmd[ci]+=32;
            if(!len){}
            else if(!my_strcmp(cmd,"help"))con_print("help ls cat meminfo sysinfo vesainfo clear reboot gui\n");
            else if(!my_strcmp(cmd,"clear"))con_clear();
            else if(!my_strcmp(cmd,"ls"))cmd_ls();
            else if(!my_strcmp(cmd,"meminfo"))cmd_meminfo();
            else if(!my_strcmp(cmd,"sysinfo"))cmd_sysinfo();
            else if(!my_strcmp(cmd,"vesainfo"))cmd_vesainfo();
            else if(!my_strcmp(cmd,"reboot"))outb(0x64,0xFE);
            else if(!my_strcmp(cmd,"gui")){
                if(use_fb){
                    run_gui();
                } else {
                    con_print("Error: Cannot start GUI! VESA framebuffer not active.\n");
                    cmd_vesainfo();
                }
            }
            else if(len>4&&cmd[0]=='c'&&cmd[1]=='a'&&cmd[2]=='t'&&cmd[3]==' ')cmd_cat(cmd+4);
            else{con_print("Unknown: ");con_print(cmd);con_char('\n');}
            len=0;con_print("brickos> ");
        }else if(c=='\b'){if(len>0){len--;con_char('\b');}}
        else if(len<63){cmd[len++]=c;con_char(c);}
    }
}