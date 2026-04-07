bits 64

global irq0_handler
global irq1_handler
global irq12_handler
extern timer_tick
extern keyboard_handler_c
extern mouse_handler_c

%macro PUSHALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POPALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

irq0_handler:
    push 0
    push 32
    PUSHALL
    mov rdi, rsp
    call timer_tick
    mov rsp, rax
    mov al, 0x20
    out 0x20, al
    POPALL
    add rsp, 16
    iretq

irq1_handler:
    push 0
    push 33
    PUSHALL
    call keyboard_handler_c
    mov al, 0x20
    out 0x20, al
    POPALL
    add rsp, 16
    iretq

irq12_handler:
    push 0
    push 44
    PUSHALL
    call mouse_handler_c
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    POPALL
    add rsp, 16
    iretq