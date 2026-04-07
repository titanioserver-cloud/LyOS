bits 64
global _start
extern main

_start:
    call main
.loop:
    jmp .loop