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
static void itoa(char*buf,u32 v){if(!v){buf[0]='0';buf[1]=0;return;}char t[12];int i=0;while(v){t[i++]='0'+v%10;v/=10;}int j=0;for(int k=i-1;k>=0;k--)buf[j++]=t[k];buf[j]=0;}
struct LoadedFont {
    u8 height;
    u8 first_char;
    u8 last_char;
    u8 def_width;
    u8 width[256];
    u16 bitmap[256][16];
} __attribute__((packed));
static struct LoadedFont* sys_font = (struct LoadedFont*)0x40000;
static int font_loaded = 0;
/* ── BSS zero + entry ── */
extern u32 __bss_start,__bss_end;
void kmain(void);void early_idt(void);
void __attribute__((section(".text.start"),noreturn)) _start(void){
    u32*p=&__bss_start;while(p<&__bss_end)*p++=0;
    early_idt();kmain();for(;;)__asm__ volatile("hlt");
}

/* ── global framebuffer info ── */
u32 bi_fb;u16 bi_pitch;u8 bi_bpp,bi_vesa,bi_font_dummy;u32 bi_font;
int fw=640,fh=480;
u8*ramdisk=(u8*)0x20000;

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
void mouse_handler(void){
    u8 status=inb(0x64);
    while(status&1){
        u8 data=inb(0x60);
        if(status&0x20){
            if(mouse_cycle==0){if(data&0x08){mouse_bytes[0]=data;mouse_cycle=1;}}
            else if(mouse_cycle==1){mouse_bytes[1]=data;mouse_cycle=2;}
            else{mouse_bytes[2]=data;mouse_cycle=0;
                int rx=(int)(signed char)mouse_bytes[1];int ry=(int)(signed char)mouse_bytes[2];
                mouse_x+=rx;mouse_y-=ry;
                if(mouse_x<0)mouse_x=0;if(mouse_x>=fw)mouse_x=fw-1;
                if(mouse_y<0)mouse_y=0;if(mouse_y>=fh)mouse_y=fh-1;
                mouse_btn=mouse_bytes[0]&0x07;}
        }
        status=inb(0x64);
    }
}
void __attribute__((naked)) irq12_stub(void){__asm__ volatile("pusha\n mov %0,%%eax\n call *%%eax\n mov $0x20,%%al\n out %%al,$0xA0\n out %%al,$0x20\n popa\n iret\n"::"i"(mouse_handler));}
void mouse_wait(u8 t){u32 to=100000;if(t==0){while(to--){if(inb(0x64)&1)return;}}else{while(to--){if(!(inb(0x64)&2))return;}}}
void mouse_write(u8 v){mouse_wait(1);outb(0x64,0xD4);mouse_wait(1);outb(0x60,v);}
u8 mouse_read(void){mouse_wait(0);return inb(0x60);}
void mouse_init(void){
    mouse_wait(1);outb(0x64,0xA8);
    mouse_wait(1);outb(0x64,0x20);mouse_wait(0);u8 s=inb(0x60);
    s|=2;s&=~0x20;s|=1;mouse_wait(1);outb(0x64,0x60);mouse_wait(1);outb(0x60,s);
    mouse_write(0xF6);mouse_read();mouse_write(0xF4);mouse_read();
    while(inb(0x64)&1)inb(0x60);mouse_cycle=0;
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
char shadow[80*30];
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
void con_clear(void){for(int i=0;i<cols*rows;i++)shadow[i]=' ';cx=0;cy=0;lastcx=-1;lastcy=-1;if(use_fb){fb_rect(0,0,fw,fh,COL_BG);fb_rect(0,0,fw,16,COL_BAR);const char*t="BrickOS 2.0";for(int i=0;t[i];i++)fb_glyph(t[i],2+i,0,0x000000);}render_all();con_cursor();}
void con_char(char c){if(c=='\n'){cx=0;cy++;}else if(c=='\b'){if(cx>0){cx--;shadow[cy*cols+cx]=' ';if(use_fb)fb_putcell(cx,cy,' ');else tvga[(cy*cols+cx)*2]=' ';}}else{shadow[cy*cols+cx]=c;if(use_fb)fb_putcell(cx,cy,c);else{tvga[(cy*cols+cx)*2]=c;tvga[(cy*cols+cx)*2+1]=0x0F;}cx++;}if(cx>=cols){cx=0;cy++;}if(cy>=rows)con_scroll();con_cursor();}
void con_print(const char*s){while(*s)con_char(*s++);}
void con_dec(u32 v){char b[12];int i=0;if(!v){con_char('0');return;}while(v){b[i++]='0'+v%10;v/=10;}while(i--)con_char(b[i]);}
char kbd_get(void){while(khead==ktail)__asm__ volatile("hlt");char c=kbuf[ktail];ktail=(ktail+1)&(KBUF-1);return c;}
char kbd_try(void){if(khead==ktail)return 0;char c=kbuf[ktail];ktail=(ktail+1)&(KBUF-1);return c;}

/* ── FAT12 ── */
u16 rd16(int o){return ramdisk[o]|(ramdisk[o+1]<<8);}
int fat_sect,root_sect,data_sect,root_cnt;u8*fatp;
void fat_init(void){int res=rd16(14),nf=ramdisk[16];root_cnt=rd16(17);fat_sect=res;root_sect=res+nf*rd16(22);data_sect=root_sect+(root_cnt*32+511)/512;fatp=ramdisk+fat_sect*512;}
int fat_get(int e){int i=e+e/2;int w=fatp[i]|(fatp[i+1]<<8);return(e&1)?w>>4:w&0xFFF;}
void fat_name83(const char*in,char*out){for(int i=0;i<11;i++)out[i]=' ';int i;for(i=0;in[i]&&in[i]!='.'&&i<8;i++)out[i]=(in[i]>='a'&&in[i]<='z')?in[i]-32:in[i];const char*d=in;while(*d&&*d!='.')d++;if(*d=='.')for(i=0;d[1+i]&&i<3;i++)out[8+i]=(d[1+i]>='a'&&d[1+i]<='z')?d[1+i]-32:d[1+i];}
int fat_read_file(const char*name,u8*dest,int max_sz){
    char n83[11];fat_name83(name,n83);
    u8*root=ramdisk+root_sect*512;
    for(int i=0;i<root_cnt;i++){
        u8*e=root+i*32;if(e[0]==0)break;
        int ok=1;for(int j=0;j<11;j++)if(e[j]!=n83[j])ok=0;if(!ok)continue;
        int sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
        int cl=e[26]|(e[27]<<8);
        int total=0;
        while(cl<0xFF8&&total<sz&&total<max_sz){
            int s=data_sect+cl-2;if(s>=200)break;
            u8*p=ramdisk+s*512;int n=sz-total;if(n>512)n=512;
            if(total+n>max_sz)n=max_sz-total;
            for(int k=0;k<n;k++)dest[total+k]=p[k];
            total+=n;cl=fat_get(cl);
        }
        return total;
    }
    return -1;
}
void cmd_cat(const char*name){char n83[11];fat_name83(name,n83);u8*root=ramdisk+root_sect*512;for(int i=0;i<root_cnt;i++){u8*e=root+i*32;if(e[0]==0)break;int ok=1;for(int j=0;j<11;j++)if(e[j]!=n83[j])ok=0;if(!ok)continue;int sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);int cl=e[26]|(e[27]<<8);while(cl<0xFF8&&sz>0){int s=data_sect+cl-2;if(s>=200){con_print("(oor)\n");return;}u8*p=ramdisk+s*512;int n=sz>512?512:sz;for(int k=0;k<n;k++)con_char(p[k]);sz-=n;cl=fat_get(cl);}con_char('\n');return;}con_print("Not found\n");}
void cmd_ls(void){u8*root=ramdisk+root_sect*512;for(int i=0;i<root_cnt;i++){u8*e=root+i*32;if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;for(int j=0;j<8;j++)con_char(e[j]);con_char('.');for(int j=8;j<11;j++)con_char(e[j]);con_print("  ");con_dec(e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24));con_print(" bytes\n");}}

/* ── CPUID ── */
void cpuid(u32 ax,u32*a,u32*b,u32*c,u32*d){__asm__ volatile("push %%ebx\ncpuid\nmov %%ebx,%%esi\npop %%ebx":"=a"(*a),"=S"(*b),"=c"(*c),"=d"(*d):"a"(ax));}

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
void draw_cursor(int mx,int my){for(int y=0;y<16;y++)for(int x=0;x<16;x++){char c=cursor_sprite[y][x];if(c=='*')gui_pixel(mx+x,my+y,0x000000);else if(c=='.')gui_pixel(mx+x,my+y,0xFFFFFF);}}

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
    const char*sub="Operating System v2.0";int sl=gui_str_width(sub);
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
#define MAX_WINS   8
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
    gui_print(w->title,w->x+BORDER+4,w->y+BORDER+2,0xFFFFFF);
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
}
/* hit-test helpers for buttons */
int win_close_hit(struct Window*w,int mx,int my){int cbx=w->x+w->w-BORDER-18,by_=w->y+BORDER+3;return mx>=cbx&&mx<cbx+14&&my>=by_&&my<by_+14;}
int win_max_hit(struct Window*w,int mx,int my){int cbx=w->x+w->w-BORDER-35,by_=w->y+BORDER+3;return mx>=cbx&&mx<cbx+14&&my>=by_&&my<by_+14;}
int win_min_hit(struct Window*w,int mx,int my){int cbx=w->x+w->w-BORDER-52,by_=w->y+BORDER+3;return mx>=cbx&&mx<cbx+14&&my>=by_&&my<by_+14;}

/* ══════════════════════════════════════
   PAINT APP  (canvas at fixed 0x50000 to avoid BSS/ramdisk collision)
   ══════════════════════════════════════ */
#define PAINT_W 188
#define PAINT_H 120
#define PAINT_CANVAS ((u32*)0x50000)
static u32 paint_color=0x000000;
static const u32 paint_palette[]={0x000000,0xFFFFFF,0xFF0000,0x00CC00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0x808080,0xFF8000,0x008060,0x800080};
#define PAINT_NC 12
void paint_clear(void){u32*c=PAINT_CANVAS;for(int i=0;i<PAINT_W*PAINT_H;i++)c[i]=0xFFFFFF;}
void draw_paint(struct Window*w,int mx,int my,int btn){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w);
    /* toolbar */
    gui_rect(px,py0,cw,18,0xC0C0C0);
    gui_print("Color:",px+2,py0+1,0x000000);
    for(int i=0;i<PAINT_NC;i++){int bx=px+52+i*14,by2=py0+3;gui_rect(bx,by2,12,12,paint_palette[i]);gui_hline(bx,by2,12,0x000000);gui_vline(bx,by2,12,0x000000);gui_hline(bx,by2+11,12,0x000000);gui_vline(bx+11,by2,12,0x000000);if(paint_palette[i]==paint_color){gui_hline(bx-1,by2-1,14,0xFF0000);gui_hline(bx-1,by2+12,14,0xFF0000);gui_vline(bx-1,by2-1,14,0xFF0000);gui_vline(bx+12,by2-1,14,0xFF0000);}}
    int cbx=px+cw-40,cby=py0+2;gui_btn(cbx,cby,38,14,"Clear",0);
    /* canvas */
    int can_x=px+1,can_y=py0+20;
    u32*cv=PAINT_CANVAS;
    for(int j=0;j<PAINT_H;j++)for(int i=0;i<PAINT_W;i++)gui_pixel(can_x+i,can_y+j,cv[j*PAINT_W+i]);
    gui_hline(can_x-1,can_y-1,PAINT_W+2,0x808080);gui_vline(can_x-1,can_y-1,PAINT_H+2,0x808080);
    gui_hline(can_x-1,can_y+PAINT_H,PAINT_W+2,0x808080);gui_vline(can_x+PAINT_W,can_y-1,PAINT_H+2,0x808080);
    if(mx>=can_x&&mx<can_x+PAINT_W&&my>=can_y&&my<can_y+PAINT_H&&(btn&1)){int bx=mx-can_x,by2=my-can_y;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int nx=bx+dx,ny=by2+dy;if(nx>=0&&nx<PAINT_W&&ny>=0&&ny<PAINT_H)cv[ny*PAINT_W+nx]=paint_color;}}
    if(btn&1){for(int i=0;i<PAINT_NC;i++){int bx=px+52+i*14,by2=py0+3;if(mx>=bx&&mx<bx+12&&my>=by2&&my<by2+12)paint_color=paint_palette[i];}if(mx>=cbx&&mx<cbx+38&&my>=cby&&my<cby+14)paint_clear();}
}

/* ══════════════════════════════════════
   NOTEPAD APP
   ══════════════════════════════════════ */
#define NOTE_LINES 12
#define NOTE_COLS  34
static char note_buf[NOTE_LINES][NOTE_COLS+1];
static int note_cx=0,note_cy=0,note_active=0;
void note_clear(void){for(int i=0;i<NOTE_LINES;i++){for(int j=0;j<=NOTE_COLS;j++)note_buf[i][j]=0;}note_cx=0;note_cy=0;}
void draw_notepad(struct Window*w){
    int px=win_client_x(w),py0=win_client_y(w),cw=win_client_w(w);
    gui_rect(px,py0,cw,16,0xC0C0C0);
    gui_print("Notepad - type to edit",px+4,py0,0x000000);
    int ta_x=px,ta_y=py0+18;
    gui_rect(ta_x,ta_y,cw,NOTE_LINES*16+2,0x1E1E1E);
    int c_adv=font_loaded?10:8;
    for(int line=0;line<NOTE_LINES;line++){
        int lx=ta_x+2,ly=ta_y+1+line*16;
        char ln[4];itoa(ln,(u32)(line+1));gui_print_bg(ln,lx,ly,0x606060,0x1E1E1E);lx+=24;
        for(int col=0;col<NOTE_COLS&&note_buf[line][col];col++)gui_char(note_buf[line][col],lx+col*c_adv,ly,0xD4D4D4,0x1E1E1E);
        if(note_active&&line==note_cy&&(ticks/50)&1)gui_rect(lx+note_cx*c_adv,ly,2,15,0xFFFFFF);
    }
}
void notepad_key(char c){if(!note_active)return;if(c=='\n'){if(note_cy<NOTE_LINES-1){note_cy++;note_cx=0;}}else if(c=='\b'){if(note_cx>0){note_cx--;note_buf[note_cy][note_cx]=0;}else if(note_cy>0){note_cy--;note_cx=my_strlen(note_buf[note_cy]);}}else{if(note_cx<NOTE_COLS-1){note_buf[note_cy][note_cx++]=c;note_buf[note_cy][note_cx]=0;}}}

/* ══════════════════════════════════════
   TERMINAL APP
   ══════════════════════════════════════ */
#define TERM_LINES 10
#define TERM_COLS  38
static char term_screen[TERM_LINES][TERM_COLS+1];
static int  term_cx_p=0,term_cy_p=0,term_active=0;
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
    gui_print("BrickOS 2.0 32-bit",px,py,0x000080);py+=20;
    gui_print("Vendor: FlatronTech",px,py,0x000000);py+=16;
    gui_print("Date: 2026-10-11",px,py,0x000000);py+=16;
    gui_print("Resolution: 640x480 VESA",px,py,0x000000);py+=16;
    gui_print("Drivers: .INF loaded",px,py,0x000000);py+=16;
    gui_print("Mouse: PS/2 Generic Mouse",px,py,0x000000);py+=16;
    gui_print("Kbd:   PS/2 Keyboard",px,py,0x000000);py+=16;
    gui_print("Monitor: Generic 640x480",px,py,0x000000);py+=16;
    py+=4;gui_print("System is operational!",px,py,0x008000);
}

/* ══════════════════════════════════════
   EXPLORER (My Computer style)
   ══════════════════════════════════════ */
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
        gui_print("3.5 Floppy (A:)",px+4,dy+50,0x000000);
        gui_print("Click to open",px+4,dy+64,0x808080);
        /* Status bar */
        gui_rect(px,py0+ch-14,cw,14,0xC0C0C0);gui_hline(px,py0+ch-15,cw,0x808080);
        gui_print("1 drive",px+4,py0+ch-13,0x000000);
    } else {
        /* File list view */
        gui_btn(px,py0,32,16,"<-",0);
        gui_rect(px+34,py0,cw-34,16,0xFFFFFF);gui_print("A:\\",px+38,py0,0x000080);
        int hy=py0+18;
        gui_rect(px,hy,cw,14,0xC0C0C0);
        gui_print("Name",px+18,hy+1,0x000000);gui_print("Size",px+130,hy+1,0x000000);gui_print("Type",px+190,hy+1,0x000000);
        u8*root=ramdisk+root_sect*512;int fy=hy+16,count=0;
        for(int i=0;i<root_cnt;i++){
            u8*e=root+i*32;
            if(e[0]==0)break;if(e[0]==0xE5||e[11]==0x0F)continue;
            if(count&1)gui_rect(px,fy,cw,16,0xEEEEFF);
            /* icon color by ext */
            u32 icol=0x6060FF;
            if(e[8]=='T'&&e[9]=='X'&&e[10]=='T')icol=0xFFCC00;
            if(e[8]=='I'&&e[9]=='N'&&e[10]=='F')icol=0xFF8000;
            if(e[8]=='B'&&e[9]=='I'&&e[10]=='N')icol=0xFF4040;
            gui_rect(px+2,fy+2,12,12,icol);gui_hline(px+2,fy+2,12,0x000000);gui_vline(px+2,fy+2,12,0x000000);
            /* name */
            char nm[14];int ni=0;
            for(int j=0;j<8&&e[j]!=' ';j++)nm[ni++]=e[j];
            nm[ni++]='.';for(int j=8;j<11&&e[j]!=' ';j++)nm[ni++]=e[j];nm[ni]=0;
            gui_print(nm,px+18,fy+1,0x000000);
            /* size */
            u32 sz=e[28]|(e[29]<<8)|(e[30]<<16)|(e[31]<<24);
            char szb[10];itoa(szb,sz);gui_print(szb,px+130,fy+1,0x000000);
            /* type */
            const char*ty="Data";
            if(e[8]=='T'&&e[9]=='X'&&e[10]=='T')ty="Text File";
            if(e[8]=='I'&&e[9]=='N'&&e[10]=='F')ty="Driver";
            if(e[8]=='B'&&e[9]=='I'&&e[10]=='N')ty="Binary";
            gui_print(ty,px+190,fy+1,0x000000);
            fy+=16;count++;if(fy>py0+ch-20)break;
        }
        /* Status bar */
        gui_rect(px,py0+ch-15,cw,15,0xC0C0C0);gui_hline(px,py0+ch-16,cw,0x808080);
        char cb[6];itoa(cb,(u32)count);gui_print(cb,px+4,py0+ch-14,0x000000);gui_print(" files",px+4+gui_str_width(cb),py0+ch-14,0x000000);
    }
}
/* Explorer click handler (called on mousedown in client area) */
void explorer_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),py0=win_client_y(w),ch=win_client_h(w);
    if(w->app_state==0){
        /* click on drive icon area */
        int dx=px+30,dy=py0+38;
        if(mx>=dx&&mx<dx+50&&my>=dy&&my<dy+60)w->app_state=1;
    } else {
        /* back button */
        if(mx>=px&&mx<px+32&&my>=py0&&my<py0+16)w->app_state=0;
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
        int tw=my_strlen(tabs[i])*8+10;
        u32 tbg=(ctrlpanel_tab==i)?0xFFFFFF:0xD0D0D0;
        gui_rect(tx,py0,tw,16,tbg);
        if(ctrlpanel_tab==i){gui_hline(tx,py0,tw,0xFFFFFF);gui_vline(tx,py0,16,0xFFFFFF);}
        else{gui_hline(tx,py0,tw,0xFFFFFF);gui_vline(tx,py0,16,0xFFFFFF);gui_hline(tx,py0+15,tw,0x808080);gui_vline(tx+tw-1,py0,16,0x808080);}
        gui_print(tabs[i],tx+5,py0,0x000000);tx+=tw;
    }
    int cy2=py0+20;
    if(ctrlpanel_tab==0){
        gui_print("BrickOS System Information",px,cy2,0x000080);cy2+=20;
        gui_print("OS Version: BrickOS 2.0",px,cy2,0x000000);cy2+=14;
        gui_print("Architecture: i386 32-bit PM",px,cy2,0x000000);cy2+=14;
        gui_print("Vendor: FlatronTech",px,cy2,0x000000);cy2+=14;
        gui_print("Build date: 2026-10-11",px,cy2,0x000000);cy2+=14;cy2+=6;
        u32 a,b,c,d;cpuid(0,&a,&b,&c,&d);
        char ven[13];*(u32*)&ven[0]=b;*(u32*)&ven[4]=d;*(u32*)&ven[8]=c;ven[12]=0;
        gui_print("CPU vendor: ",px,cy2,0x000000);gui_print(ven,px+96,cy2,0x000080);cy2+=14;
        u32 cnt=*(u32*)0x71C;if(cnt>32)cnt=32;u32 tkb=0;
        for(u32 i=0;i<cnt;i++){u8*e=(u8*)(0x720+i*20);u32 ty=e[16]|(e[17]<<8)|(e[18]<<16)|(e[19]<<24);if(ty==1){u32 ll=e[8]|(e[9]<<8)|(e[10]<<16)|(e[11]<<24);tkb+=ll/1024;}}
        char mb[12];itoa(mb,tkb/1024);gui_print("RAM:  ",px,cy2,0x000000);gui_print(mb,px+48,cy2,0x000080);gui_print(" MB",px+48+my_strlen(mb)*8,cy2,0x000000);cy2+=14;
        char upb[12];itoa(upb,ticks/100);gui_print("Uptime:",px,cy2,0x000000);gui_print(upb,px+56,cy2,0x000080);gui_print("s",px+56+my_strlen(upb)*8,cy2,0x000000);
    } else if(ctrlpanel_tab==1){
        gui_print("Display Settings",px,cy2,0x000080);cy2+=20;
        gui_print("Resolution: 640 x 480",px,cy2,0x000000);cy2+=14;
        gui_print("Mode: VESA LFB",px,cy2,0x000000);cy2+=14;
        char bppb[4];itoa(bppb,(u32)fbpp);
        gui_print("Color depth: ",px,cy2,0x000000);gui_print(bppb,px+104,cy2,0x000080);gui_print(" bpp",px+104+my_strlen(bppb)*8,cy2,0x000000);cy2+=14;
        char pitb[8];itoa(pitb,(u32)fpitch);
        gui_print("Pitch: ",px,cy2,0x000000);gui_print(pitb,px+56,cy2,0x000080);cy2+=14;
        gui_print("Refresh: 60Hz target",px,cy2,0x000000);cy2+=14;
        char fbb[12];itoa(fbb,bi_fb);gui_print("Framebuf: 0x",px,cy2,0x000000);gui_print(fbb,px+96,cy2,0x000080);
    } else {
        gui_print("Desktop Background",px,cy2,0x000080);cy2+=20;
        gui_print("Select color:",px,cy2,0x000000);cy2+=16;
        for(int i=0;i<N_DESK_PAL;i++){
            int sw=30,sh=24;int bx=px+(i%4)*(sw+6),by2=cy2+(i/4)*(sh+6);
            gui_rect(bx,by2,sw,sh,desk_pal[i]);
            gui_hline(bx,by2,sw,0x808080);gui_vline(bx,by2,sh,0x808080);
            gui_hline(bx,by2+sh-1,sw,0x808080);gui_vline(bx+sw-1,by2,sh,0x808080);
            if(desk_pal[i]==desktop_color){gui_hline(bx-2,by2-2,sw+4,0xFFFFFF);gui_hline(bx-2,by2+sh+1,sw+4,0xFFFFFF);gui_vline(bx-2,by2-2,sh+4,0xFFFFFF);gui_vline(bx+sw+1,by2-2,sh+4,0xFFFFFF);}
        }
        cy2+=70;
        gui_print("Taskbar: Classic style",px,cy2,0x000000);cy2+=14;
        gui_print("Font: Custom (font.fon)",px,cy2,0x000000);
    }
}
void ctrlpanel_click(struct Window*w,int mx,int my){
    int px=win_client_x(w),py0=win_client_y(w);
    const char*tabs[]={"System","Display","Personalize"};
    int tx=px;
    for(int i=0;i<3;i++){int tw=my_strlen(tabs[i])*8+10;if(mx>=tx&&mx<tx+tw&&my>=py0&&my<py0+16){ctrlpanel_tab=i;return;}tx+=tw;}
    if(ctrlpanel_tab==2){
        int cy2=py0+20+20+16;
        for(int i=0;i<N_DESK_PAL;i++){int sw=30,sh=24;int bx=px+(i%4)*(sw+6),by2=cy2+(i/4)*(sh+6);if(mx>=bx&&mx<bx+sw&&my>=by2&&my<by2+sh){desktop_color=desk_pal[i];return;}}
    }
}

/* ══════════════════════════════════════
   WELCOME
   ══════════════════════════════════════ */
void draw_welcome(struct Window*w){
    int px=win_client_x(w),py=win_client_y(w);
    gui_rect(w->x+BORDER+2,win_client_y(w),win_client_w(w),win_client_h(w),0xFFFFCC);
    gui_print("Welcome to BrickOS!",px,py,0x000080);py+=20;
    gui_print("Click & drag titlebars to move",px,py,0x000000);py+=16;
    gui_print("Click [X] to close windows",px,py,0x000000);py+=16;
    gui_print("Click [_] to minimize",px,py,0x000000);py+=16;
    gui_print("Click [ ] to maximize",px,py,0x000000);py+=16;
    gui_print("Double-click desktop icons",px,py,0x000000);py+=16;
    py+=4;gui_print("Enjoy BrickOS!",px,py,0x008000);
}

/* ══════════════════════════════════════
   DESKTOP ICONS
   ══════════════════════════════════════ */
#define NDESK_ICONS 6
struct DesktopIcon{int x,y,app_id;u32 col1,col2;char label[16];};
static struct DesktopIcon desk_icons[NDESK_ICONS]={
    {20,  12, APP_EXPLORER,  0x3366FF, 0x1133AA, "My Computer"},
    {20,  84, APP_CTRLPANEL, 0xFF8800, 0xBB5500, "Control Panel"},
    {20, 156, APP_NOTEPAD,   0xFFFF44, 0x888800, "Notepad"},
    {20, 228, APP_PAINT,     0xFF4444, 0xAA1111, "Paint"},
    {20, 300, APP_TERMINAL,  0x004400, 0x008800, "Terminal"},
    {20, 372, APP_WELCOME,   0x44AAFF, 0x1177BB, "Welcome"},
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
        /* shadow */
        gui_rect(x+2,y+2,36,36,0x000000);
        /* icon bg */
        gui_rect(x,y,36,36,d->col2);
        gui_rect(x+2,y+2,32,32,d->col1);
        /* custom mini-graphic inside icon */
        if(d->app_id==APP_EXPLORER){
            gui_rect(x+6,y+8,24,18,0xD0D0D0);
            gui_rect(x+8,y+10,20,12,0x202060);
            gui_rect(x+12,y+26,12,3,0xD0D0D0);
        } else if(d->app_id==APP_CTRLPANEL){
            gui_hline(x+8,y+10,20,0x000000);gui_rect(x+12,y+8,4,5,0xFFFFFF);
            gui_hline(x+8,y+18,20,0x000000);gui_rect(x+20,y+16,4,5,0xFFFFFF);
            gui_hline(x+8,y+26,20,0x000000);gui_rect(x+14,y+24,4,5,0xFFFFFF);
        } else if(d->app_id==APP_NOTEPAD){
            gui_rect(x+9,y+6,18,24,0xFFFFFF);
            gui_hline(x+12,y+11,12,0x0000AA);
            gui_hline(x+12,y+16,12,0x0000AA);
            gui_hline(x+12,y+21,12,0x0000AA);
        } else if(d->app_id==APP_PAINT){
            gui_rect(x+7,y+7,22,22,0xDFDFDF);
            gui_rect(x+10,y+10,4,4,0xFF0000);
            gui_rect(x+18,y+10,4,4,0x00AA00);
            gui_rect(x+10,y+18,4,4,0x0000FF);
            gui_rect(x+18,y+18,4,4,0xFFFF00);
        } else if(d->app_id==APP_TERMINAL){
            gui_rect(x+6,y+7,24,22,0x000000);
            gui_print(">",x+8,y+9,0x00FF00);
            gui_hline(x+16,y+20,6,0x00FF00);
        } else {
            gui_rect(x+7,y+7,22,22,0x0040A0);
            gui_rect(x+16,y+11,4,4,0xFFFFFF);
            gui_rect(x+16,y+17,4,8,0xFFFFFF);
        }
        if(desk_sel==i){gui_hline(x-1,y-1,38,0xFFFFFF);gui_hline(x-1,y+36,38,0xFFFFFF);gui_vline(x-1,y-1,38,0xFFFFFF);gui_vline(x+36,y-1,38,0xFFFFFF);}
        draw_desk_label(d->label,x+18,y+40,desk_sel==i);
    }
}

void open_app(int app_id){
    switch(app_id){
        case APP_PAINT:{int wi=win_new(APP_PAINT,85,40,320,210,"Paint");if(wi>=0)paint_clear();break;}
        case APP_NOTEPAD:{int wi=win_new(APP_NOTEPAD,100,50,316,230,"Notepad");if(wi>=0){note_clear();note_active=1;term_active=0;}break;}
        case APP_TASKMGR:win_new(APP_TASKMGR,120,40,330,250,"Task Manager");break;
        case APP_TERMINAL:{int wi=win_new(APP_TERMINAL,90,60,328,200,"Terminal");if(wi>=0){term_clear_screen();term_print("BrickOS Terminal\n$ ");term_active=1;note_active=0;}break;}
        case APP_SYSINFO:win_new(APP_SYSINFO,85,25,260,205,"System Info");break;
        case APP_EXPLORER:win_new(APP_EXPLORER,355,25,270,205,"My Computer");break;
        case APP_CTRLPANEL:win_new(APP_CTRLPANEL,110,40,330,250,"Control Panel");break;
        case APP_WELCOME:win_new(APP_WELCOME,160,245,310,165,"Welcome");break;
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
    /* Clock */
    char upb[16];itoa(upb,ticks/100);int ul=gui_str_width(upb);
    gui_rect(fw-ul-12,ty+4,ul+10,18,0x808080);
    gui_print(upb,fw-ul-7,ty+5,0xFFFFFF);
    /* Window buttons */
    int bx=74;
    for(int i=0;i<nwins;i++){
        if(bx+90>fw-ul-16)break;
        int foc=(i==nwins-1);
        int is_min=wins[i].minimized;
        u32 bbg=is_min?0xA8A8A8:(foc?0xA0A0A0:0xC0C0C0);
        gui_rect(bx,ty+3,86,TASKBAR_H-6,bbg);
        if(foc||is_min){gui_hline(bx,ty+3,86,0x808080);gui_vline(bx,ty+3,TASKBAR_H-6,0x808080);}
        else{gui_hline(bx,ty+3,86,0xFFFFFF);gui_vline(bx,ty+3,TASKBAR_H-6,0xFFFFFF);gui_hline(bx,ty+TASKBAR_H-4,86,0x808080);gui_vline(bx+85,ty+3,TASKBAR_H-6,0x808080);}
        char tb[16];int tl=0;
        while(wins[i].title[tl]&&tl<12){tb[tl]=wins[i].title[tl];tl++;}tb[tl]=0;
        int tw=gui_str_width(tb);
        int tx_pos=bx+4;
        if(tw<80)tx_pos=bx+(86-tw)/2;
        int ty_pos=ty+3+(TASKBAR_H-6-(font_loaded?sys_font->height:16))/2;
        gui_print(tb,tx_pos,ty_pos,0x000000);
        bx+=90;
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
            case APP_NOTEPAD: notepad_key(c);note_active=1;return;
            case APP_TERMINAL:terminal_key(c);term_active=1;return;
            default:return;
        }
    }
}

/* ══════════════════════════════════════
   MAIN GUI LOOP
   ══════════════════════════════════════ */
void run_gui(void){
    esc_pressed=0;start_menu=0;
    nwins=0;
    win_new(APP_SYSINFO, 85,  25, 260, 205, "System Info");
    win_new(APP_EXPLORER, 355, 25, 270, 205, "My Computer");
    win_new(APP_WELCOME,  160, 245, 310, 165, "Welcome");
    paint_clear();note_clear();term_clear_screen();
    term_print("BrickOS Terminal\nType 'help' for list\n");
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
            }
        }

        draw_taskbar();

        /* ── Mouse click ── */
        if(mouse_btn&&!last_btn){
            int mx=mouse_x,my=mouse_y;
            int ty=fh-TASKBAR_H;

            /* Start menu items */
            if(start_menu){
                int mh=MENU_N*MENU_IH+6,mw=140,mmx=2,mmy=ty-mh;
                if(mx>=mmx+20&&mx<mmx+mw&&my>=mmy&&my<ty){
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
            {int bx=74;char upb2[16];itoa(upb2,ticks/100);int ul2=gui_str_width(upb2);
            for(int i=0;i<nwins;i++){if(bx+90>fw-ul2-16)break;if(mx>=bx&&mx<bx+86&&my>ty+3){/* toggle minimize or raise */if(wins[i].minimized){wins[i].minimized=0;win_raise(i);}else if(i==nwins-1){wins[i].minimized=1;}else{win_raise(i);}goto input_done;}bx+=90;}}

            start_menu=0;

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
                if(win_close_hit(w,mx,my)){if(w->app_id==APP_NOTEPAD)note_active=0;if(w->app_id==APP_TERMINAL)term_active=0;for(int j=i;j<nwins-1;j++)wins[j]=wins[j+1];nwins--;break;}
                /* Maximize? */
                if(win_max_hit(w,mx,my)){if(!w->maximized){w->prev_x=w->x;w->prev_y=w->y;w->prev_w=w->w;w->prev_h=w->h;w->x=0;w->y=0;w->w=fw;w->h=fh-TASKBAR_H;w->maximized=1;}else{w->x=w->prev_x;w->y=w->prev_y;w->w=w->prev_w;w->h=w->prev_h;w->maximized=0;}win_raise(i);break;}
                /* Minimize? */
                if(win_min_hit(w,mx,my)){w->minimized=1;if(w->app_id==APP_NOTEPAD)note_active=0;if(w->app_id==APP_TERMINAL)term_active=0;break;}
                /* Titlebar drag */
                if(my>=w->y+BORDER&&my<=w->y+BORDER+TITLEBAR_H){drag_win=i;drag_ox=mx-w->x;drag_oy=my-w->y;win_raise(i);drag_win=nwins-1;if(wins[drag_win].app_id==APP_NOTEPAD){note_active=1;term_active=0;}if(wins[drag_win].app_id==APP_TERMINAL){term_active=1;note_active=0;}break;}
                /* Client area click */
                win_raise(i);
                struct Window*wt=&wins[nwins-1];
                if(wt->app_id==APP_NOTEPAD){note_active=1;term_active=0;}
                if(wt->app_id==APP_TERMINAL){term_active=1;note_active=0;}
                if(wt->app_id==APP_CTRLPANEL)ctrlpanel_click(wt,mx,my);
                if(wt->app_id==APP_EXPLORER)explorer_click(wt,mx,my);
                break;
            }
        }
        if(!mouse_btn)drag_win=-1;
        if(drag_win>=0&&drag_win<nwins&&mouse_btn){
            if(!wins[drag_win].maximized){
                wins[drag_win].x=mouse_x-drag_ox;wins[drag_win].y=mouse_y-drag_oy;
                if(wins[drag_win].x<0)wins[drag_win].x=0;if(wins[drag_win].y<0)wins[drag_win].y=0;
                if(wins[drag_win].x+wins[drag_win].w>fw)wins[drag_win].x=fw-wins[drag_win].w;
                if(wins[drag_win].y+wins[drag_win].h>fh-TASKBAR_H)wins[drag_win].y=fh-TASKBAR_H-wins[drag_win].h;
            }
        }
        input_done:
        draw_cursor(mouse_x,mouse_y);gui_flip();last_btn=mouse_btn;
        __asm__ volatile("hlt");
    }
    con_clear();
}

/* ══════════════════════════════════════
   CONSOLE INIT + CLI
   ══════════════════════════════════════ */
void console_init(void){
    bi_fb=*(u32*)0x704;bi_pitch=*(u16*)0x70C;bi_bpp=*(u8*)0x70E;bi_vesa=*(u8*)0x70F;
    bi_font=*(u32*)0x710;
    if(bi_vesa&&bi_bpp>=24&&bi_fb>=0xA0000&&bi_pitch>=640){
        use_fb=1;fb=(u8*)(unsigned long)bi_fb;fpitch=bi_pitch;fbpp=bi_bpp;
        cols=80;rows=29;COL_BG=0x401010;COL_FG=0xFFFFFF;COL_BAR=0xA0A0A0;
    }
    con_clear();
}
void cmd_meminfo(void){u32 cnt=*(u32*)0x71C;u8*m=(u8*)0x720;if(cnt>32)cnt=32;con_print("E820 regions: ");con_dec(cnt);con_char('\n');for(u32 i=0;i<cnt;i++){u8*e=m+i*20;u32 bl=e[0]|e[1]<<8|e[2]<<16|e[3]<<24;u32 ll=e[8]|e[9]<<8|e[10]<<16|e[11]<<24;u32 ty=e[16]|e[17]<<8|e[18]<<16|e[19]<<24;con_print("  base=");const char*h="0123456789ABCDEF";for(int q=28;q>=0;q-=4)con_char(h[(bl>>q)&15]);con_print(" len=");for(int q=28;q>=0;q-=4)con_char(h[(ll>>q)&15]);con_print(" type=");con_dec(ty);con_char('\n');}}
void cmd_sysinfo(void){u32 a,b,c,d;cpuid(0,&a,&b,&c,&d);char ven[13];*(u32*)&ven[0]=b;*(u32*)&ven[4]=d;*(u32*)&ven[8]=c;ven[12]=0;con_print("CPU: ");con_print(ven);con_char('\n');}
void kmain(void){
    console_init();interrupts_init();fat_init();
    if(fat_read_file("FONT.BIN",(u8*)sys_font,sizeof(struct LoadedFont))>0){
        font_loaded=1;
    }
    con_print("BrickOS booting...\n");
    if(use_fb){boot_anim();run_gui();}
    con_print("brickos> ");
    char cmd[64];int len=0;
    while(1){
        char c=kbd_get();
        if(c=='\n'){cmd[len]=0;con_char('\n');
            if(!len){}
            else if(!my_strcmp(cmd,"help"))con_print("help ls cat meminfo sysinfo clear reboot gui\n");
            else if(!my_strcmp(cmd,"clear"))con_clear();
            else if(!my_strcmp(cmd,"ls"))cmd_ls();
            else if(!my_strcmp(cmd,"meminfo"))cmd_meminfo();
            else if(!my_strcmp(cmd,"sysinfo"))cmd_sysinfo();
            else if(!my_strcmp(cmd,"reboot"))outb(0x64,0xFE);
            else if(!my_strcmp(cmd,"gui")&&use_fb)run_gui();
            else if(len>4&&cmd[0]=='c'&&cmd[1]=='a'&&cmd[2]=='t'&&cmd[3]==' ')cmd_cat(cmd+4);
            else{con_print("Unknown: ");con_print(cmd);con_char('\n');}
            len=0;con_print("brickos> ");
        }else if(c=='\b'){if(len>0){len--;con_char('\b');}}
        else if(len<63){cmd[len++]=c;con_char(c);}
    }
}