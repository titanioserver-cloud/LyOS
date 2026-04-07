section .multiboot_header
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start))

    align 8
framebuffer_tag_start:
    dw 5
    dw 0
    dd 20
    dd 1280
    dd 720
    dd 32
framebuffer_tag_end:

    align 8
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 4096
p4_table: resb 4096
p3_table: resb 4096
p2_table: resb 16384
stack_bottom: resb 16384
stack_top:

section .rodata
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .text
global start
extern kernel_main

bits 32
start:
    mov esp, stack_top
    mov edi, ebx

    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x0002
    mov cr0, eax

    mov eax, cr4
    or eax, 3 << 9
    or eax, 1 << 18
    mov cr4, eax

    xor ecx, ecx
    xgetbv
    or eax, 7
    xsetbv

    mov eax, p3_table
    or eax, 0b111
    mov [p4_table], eax

    mov eax, p2_table
    or eax, 0b111
    mov [p3_table], eax
    mov eax, p2_table + 4096
    or eax, 0b111
    mov [p3_table + 8], eax
    mov eax, p2_table + 8192
    or eax, 0b111
    mov [p3_table + 16], eax
    mov eax, p2_table + 12288
    or eax, 0b111
    mov [p3_table + 24], eax
    
    mov ecx, 0
.map_p2:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000111
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 2048
    jne .map_p2

    mov eax, p4_table
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

bits 64
long_mode_start:
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    call kernel_main
    cli
.hlt:
    hlt
    jmp .hlt