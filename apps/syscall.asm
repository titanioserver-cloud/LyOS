bits 64
global _start
global do_syscall
extern main

_start:
    call main
.loop:
    jmp .loop

do_syscall:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov rcx, r8
    mov r8, r9
    mov r9, [rsp+8]
    int 0x80
    ret