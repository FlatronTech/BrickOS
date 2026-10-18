; vesa_switch.asm — Bezpieczna zmiana trybu VESA bez restartu
; Wczytywana pod 0x7000. Przechodzi do Real Mode z poprawnym IDTR (limit 0x3FF, base 0),
; wywołuje BIOS int 10h i wraca do 32-bit PM z odtworzonym IDTR.

[bits 32]
[org  0x7000]

entry32:
    push ebp
    mov  ebp, esp
    pushad
    pushfd

    movzx eax, word [ebp + 8]
    mov  [mode_save - 0x7000 + 0x7000], ax

    cli

    ; Zapisz obecny (32-bit) IDTR
    sidt [saved_idtr - 0x7000 + 0x7000]

    ; Załaduj Real Mode IDTR (wektory pod 0x0)
    lidt [rm_idtr - 0x7000 + 0x7000]

    ; Załaduj 16-bit GDT
    lgdt [gdtptr16 - 0x7000 + 0x7000]

    jmp  0x08:(.pm16 - 0x7000 + 0x7000)

[bits 16]
.pm16:
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Wyłącz PE
    mov  eax, cr0
    and  eax, 0xFFFFFFFE
    mov  cr0, eax

    jmp  0x0000:(.rm - 0x7000 + 0x7000)

.rm:
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  sp, 0x6F00

    ; Zmiana trybu VBE
    mov  ax, 0x4F02
    mov  bx, [mode_save - 0x7000 + 0x7000]
    or   bx, 0x4000             ; LFB
    int  0x10

    ; Pobranie ModeInfoBlock
    xor  ax, ax
    mov  es, ax
    mov  ax, 0x4F01
    mov  cx, [mode_save - 0x7000 + 0x7000]
    mov  di, 0x7200
    int  0x10

    ; Przepisz dane do BIOS data area (0x702..0x70F)
    mov  ax, [mode_save - 0x7000 + 0x7000]
    mov  [0x702], ax
    mov  ax, [0x7212]           ; XResolution
    mov  [0x708], ax
    mov  ax, [0x7214]           ; YResolution
    mov  [0x70A], ax
    mov  ax, [0x7210]           ; Pitch
    mov  [0x70C], ax
    mov  al, [0x7219]           ; BPP
    mov  [0x70E], al
    mov  byte [0x70F], 1        ; Active
    mov  eax, [0x7228]          ; LFB
    mov  [0x704], eax

    ; Powrót do 32-bit Protected Mode
    cli
    lgdt [gdtptr32 - 0x7000 + 0x7000]

    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    jmp  dword 0x08:(.back32 - 0x7000 + 0x7000)

[bits 32]
.back32:
    mov  eax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Odtwórz 32-bit IDTR kernela!
    lidt [saved_idtr - 0x7000 + 0x7000]

    popfd
    popad
    pop  ebp
    ret

mode_save: dw 0x4115

saved_idtr:
    dw 0
    dd 0

rm_idtr:
    dw 0x03FF
    dd 0x00000000

align 8
gdt16:
    dq 0
    dw 0xFFFF, 0x0000, 0x9A00, 0x0000
    dw 0xFFFF, 0x0000, 0x9200, 0x0000
gdt16_end:
gdtptr16:
    dw gdt16_end - gdt16 - 1
    dd gdt16

align 8
gdt32:
    dq 0
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF
gdt32_end:
gdtptr32:
    dw gdt32_end - gdt32 - 1
    dd gdt32
