bits 64

global do_syscall
global isr128
global enter_user_mode
extern syscall_handler

enter_user_mode:
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rax, rsp
    push 0x23
    push rax
    push 0x202
    push 0x1B
    push rdi
    iretq

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

isr128:
    push r9
    push r8
    push rcx
    push rdx
    push rsi
    push rdi
    push rax
    
    mov rdi, rsp
    call syscall_handler
    
    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    iretq