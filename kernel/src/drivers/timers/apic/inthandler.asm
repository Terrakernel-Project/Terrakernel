bits 64
section .text
global apic_timer_interrupt_handler
extern apic_c_timer_interrupt_handler

apic_timer_interrupt_handler:
    push rsp
    push rax
    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rax, cr3
    push rax
    push rbp

    mov rdi, rsp
    call apic_c_timer_interrupt_handler
    test rax, rax
    je .no_reset_rsp
    mov rsp, rax
.no_reset_rsp:
    pop rbp
    pop rax
    mov cr3, rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rsp

    iretq
