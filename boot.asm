[bits 16]
org 0x7C00
jmp short boot
nop
db 'MSWIN4.1'
dw 512
db 1
dw 1
db 2
dw 224
dw 2880
db 0xF0
dw 9
dw 18
dw 2
dd 0
dd 0
db 0,0,0x29
dd 0x20260828
db 'GRUZOS     '
db 'FAT12   '

boot:
xor ax,ax
mov ds,ax
mov es,ax
mov ss,ax
mov sp,0x7C00
mov [0x700],dl

mov si,m0
call print

; A20 gate
mov ax,0x2401
int 0x15
in al,0x92
or al,2
out 0x92,al

; E820 Memory Map
xor ebx,ebx
mov di,0x720
mov edx,0x534D4150
.en:
mov eax,0xE820
mov ecx,20
int 0x15
jc .ed
cmp eax,0x534D4150
jne .ed
add di,20
inc dword [0x71C]
test ebx,ebx
jnz .en
.ed:

; BIOS 8x16 font
mov ax,0x1130
mov bh,6
int 0x10
mov ax,es
movzx eax,ax
shl eax,4
movzx ebx,bp
add eax,ebx
mov [0x710],eax

; VESA Mode 0x4112 (or 0x4140)
mov cx,0x112
mov ax,0x4F02
mov bx,0x4112
int 0x10
cmp ax,0x4F
je .vok
mov cx,0x140
mov ax,0x4F02
mov bx,0x4140
int 0x10
cmp ax,0x4F
jne .vf
.vok:
push ds
pop es
mov ax,0x4F01
mov di,0xC00
int 0x10
cmp ax,0x4F
jne .vf
mov eax,[0xC28]
mov [0x704],eax
mov ax,[0xC10]
mov [0x70C],ax
mov al,[0xC19]
mov [0x70E],al
mov byte [0x70F],1
jmp .rd
.vf:
mov byte [0x70F],0
.rd:

; RAMDISK (200 sectors) to 0x2000:0000
mov bp,0
mov cx,200
mov dx,0x2000
call load

; KERNEL (96 sectors) to 0x1000:0000
mov bp,33
mov cx,96
mov dx,0x1000
call load

; Enter Protected Mode
cli
lgdt [gdtd]
mov eax,cr0
or al,1
mov cr0,eax
jmp dword 0x08:pm

[bits 32]
pm:
mov eax,0x10
mov ds,eax
mov es,eax
mov ss,eax
mov esp,0x90000
call 0x10000
jmp $

[bits 16]
load:
mov es,dx
.next:
push cx
mov ax,bp
xor dx,dx
mov bx,36
div bx
mov ch,al
mov ax,dx
mov bl,18
div bl
mov dh,al
mov cl,ah
inc cl
mov al,19
sub al,cl
xor ah,ah
pop bx
cmp ax,bx
jbe .cok
mov ax,bx
.cok:
mov si,es
shl si,4
neg si
jz .nb
shr si,9
cmp ax,si
jbe .nb
mov ax,si
.nb:
push bx
push ax
mov ah,2
mov dl,[0x700]
xor bx,bx
int 0x13
jnc .rok
xor ax,ax
mov dl,[0x700]
int 0x13
pop ax
pop cx
jmp .next
.rok:
pop ax
pop cx
add bp,ax
sub cx,ax
shl ax,5
mov bx,es
add bx,ax
mov es,bx
test cx,cx
jnz .next
ret

print:
lodsb
test al,al
jz .done
mov ah,0x0E
int 0x10
jmp print
.done:
ret

m0 db 'GruzOS...',13,10,0

gdt:
dq 0
dw 0xFFFF,0,0x9A00,0xCF
dw 0xFFFF,0,0x9200,0xCF
gdtd:
dw $-gdt-1
dd gdt

times 510-($-$$) db 0
dw 0xAA55
