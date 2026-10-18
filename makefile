export PATH := /mingw32/bin:$(PATH)

CC = i686-w64-mingw32-gcc
LD = ld
AS = nasm
OBJCOPY = objcopy

CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx -O2 -c

APP_OBJS = app_paint.o app_notepad.o app_terminal.o app_ctrlpanel.o app_calc.o \
           app_explorer.o app_taskmgr.o app_sysinfo.o app_bmpview.o app_saveas.o \
           app_welcome.o app_fileprops.o \
           drv_display.o drv_keyboard.o drv_mouse.o

all: brickos.img

boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

start.o: start.asm
	$(AS) -f elf32 start.asm -o start.o

vesa_switch.bin: vesa_switch.asm
	$(AS) -f bin vesa_switch.asm -o vesa_switch.bin

vesa_trampoline.h: vesa_switch.bin gen_trampoline.py
	python gen_trampoline.py

kernel.o: kernel.c kernel.h vesa_trampoline.h
	$(CC) $(CFLAGS) kernel.c -o kernel.o

%.o: %.c kernel.h
	$(CC) $(CFLAGS) $< -o $@

kernel.pe: start.o kernel.o $(APP_OBJS) linker.ld
	$(LD) -m i386pe -T linker.ld -o kernel.pe start.o kernel.o $(APP_OBJS)

kernel.bin: kernel.pe
	$(OBJCOPY) -O binary kernel.pe kernel.bin

mkimg.exe: mkimg.c
	$(CC) -O2 mkimg.c -o mkimg.exe

FONT.BIN: font.fon build_font.py
	python build_font.py

brickos.img: boot.bin kernel.bin mkimg.exe FONT.BIN
	./mkimg.exe boot.bin kernel.bin brickos.img
	cp brickos.img gruzos.img

gruzos.img: brickos.img

app_paint.o: app_paint.c app_paint.h kernel.h
	$(CC) $(CFLAGS) app_paint.c -o app_paint.o

clean:
	rm -f *.o *.bin *.pe mkimg.exe vesa_trampoline.h
	-rm -f *.img