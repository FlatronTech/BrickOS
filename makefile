export PATH := /mingw32/bin:$(PATH)

CC = i686-w64-mingw32-gcc
LD = ld
AS = nasm
OBJCOPY = objcopy

CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx -O2 -c

all: gruzos.img

boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

kernel.o: kernel.c
	$(CC) $(CFLAGS) kernel.c -o kernel.o

start.o: start.asm
	$(AS) -f elf32 start.asm -o start.o

kernel.pe: start.o kernel.o linker.ld
	$(LD) -m i386pe -T linker.ld -o kernel.pe start.o kernel.o

kernel.bin: kernel.pe
	$(OBJCOPY) -O binary kernel.pe kernel.bin

mkimg.exe: mkimg.c
	$(CC) -O2 mkimg.c -o mkimg.exe

FONT.BIN: font.fon build_font.py
	python build_font.py

gruzos.img: boot.bin kernel.bin mkimg.exe FONT.BIN
	./mkimg.exe boot.bin kernel.bin gruzos.img

clean:
	rm -f *.o *.bin *.img *.pe mkimg.exe