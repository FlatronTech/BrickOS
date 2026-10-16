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
db 'BRICKOS    '
db 'FAT12   '

boot:
xor ax,ax
mov ds,ax
mov es,ax
mov ss,ax
mov sp,0x7C00
mov [0x700],dl

; A20 gate
mov ax,0x2401
int 0x15
in al,0x92
or al,2
out 0x92,al

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

; VESA Mode selector from CMOS 0x38 (1=800x600, 2=640x480)
mov si, vmodes_800
mov al, 0x38
out 0x70, al
in al, 0x71
cmp al, 2
jne .mloop
mov si, vmodes_640

.mloop:
cs lodsw
test ax, ax
jz .vf
mov bx, ax
mov [0x702], bx ; save current mode being tried
mov ax, 0x4F02
int 0x10
cmp ax, 0x004F
jne .mloop

mov cx, bx
and cx, 0x1FF
push ds
pop es
mov ax, 0x4F01
mov di, 0xC00
int 0x10
cmp ax, 0x004F
jne .mloop

; Check if LFB address is valid (not 0) and at least 24 bpp
mov eax, [0xC28]
test eax, eax
jz .mloop
mov al, [0xC19]
cmp al, 24
jb .mloop

mov [0x704], eax
mov ax, [0xC12]
mov [0x708], ax
mov ax, [0xC14]
mov [0x70A], ax
mov ax, [0xC10]
mov [0x70C], ax
mov al, [0xC19]
mov [0x70E], al
mov byte [0x70F], 1
jmp unreal

.vf:
mov byte [0x70F], 0

unreal:
; Unreal Mode setup: load GDT and set FS limit to 4GB
cli
lgdt [gdtd]
mov eax,cr0
or al,1
mov cr0,eax
jmp $+2
mov bx,0x10
mov fs,bx
and al,0xFE
mov cr0,eax
jmp $+2
sti

; Load entire 2880 sectors (1.44MB) of disk to 0x800000 via 0x2000:0000 buffer
xor bp,bp           ; start LBA = 0
mov cx,2880         ; 2880 sectors (1.44MB floppy)
call load_unreal

; Enter Protected Mode
cli
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

; Copy kernel (128KB) from 0x800000 + 33*512 to 0x10000
mov esi, 0x800000 + 33*512
mov edi, 0x10000
mov ecx, 32768
rep movsd

call 0x10000
jmp $

vmodes_800:
dw 0x4115, 0x4143, 0x4118
vmodes_640:
dw 0x4112, 0x4142, 0

[bits 16]
load_unreal:
mov ax, 0x2000
mov es, ax          ; ES = 0x2000 (buffer 0x20000)
.next:
push cx
; Calculate CHS from LBA in BP
mov ax,bp
xor dx,dx
mov bx,36
div bx
mov ch,al           ; cylinder = LBA / 36
mov ax,dx
mov bl,18
div bl
mov dh,al           ; head = (LBA % 36) / 18
mov cl,ah           ; sector = (LBA % 18) + 1
inc cl

mov al,19
sub al,cl           ; sectors remaining on this track
xor ah,ah
pop bx
cmp ax,bx
jbe .cok
mov ax,bx
.cok:
push bx
push ax             ; save sectors to read
mov ah,2
mov dl,[0x700]
xor bx,bx
int 0x13
jnc .rok
; Reset disk on error and retry
xor ax,ax
mov dl,[0x700]
int 0x13
pop ax
pop cx
jmp .next

.rok:
pop ax              ; AX = sectors read
pop cx              ; CX = remaining sectors
; Copy AX sectors from 0x20000 to fs:[ram_ptr]
push cx
push si
push di
movzx ecx,ax
shl ecx,7           ; ECX = AX * 128 dwords = AX * 512 bytes
xor esi,esi
mov edi,[ram_ptr]
.cpy:
mov edx,[es:esi]
a32 mov [fs:edi],edx
add esi,4
add edi,4
dec ecx
jnz .cpy
mov [ram_ptr],edi
pop di
pop si
pop cx

add bp,ax
sub cx,ax
jnz .next
ret

ram_ptr dd 0x800000

gdt:
dq 0
dw 0xFFFF,0,0x9A00,0xCF  ; 4GB 32-bit Code (base 0)
dw 0xFFFF,0,0x9200,0xCF  ; 4GB 32-bit Data (base 0)
gdtd:
dw $-gdt-1
dd gdt

times 510-($-$$) db 0
dw 0xAA55
