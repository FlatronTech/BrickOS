[bits 32]
[extern _kmain]

global _start
_start:
    call _kmain
    jmp $