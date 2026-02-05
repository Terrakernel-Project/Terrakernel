bits 64

section .bss
store_reg: resq 2

section .text
global prepare_cpu_asm
global get_cpu_gdtr

prepare_cpu_asm:
    cli

    mov     cr3, rdi
    lgdt    [rsi]

	; DO NOT LOAD AN IDT, INTERRUPTS ARE ONLY HANDLED BY THE BSP

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pushfq
    pop rax
    or rax, 0x202
    push rax
    push 0x08
    lea rax, [rel .flush]
    push rax
    iretq

.flush:
    mov rax, 0

    ret

get_cpu_gdtr:
    sgdt [store_reg]

    mov rax, [store_reg]

    ret
