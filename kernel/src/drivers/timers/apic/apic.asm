bits 64
section .text
global apic_timer_interrupt_handler
extern apic_timer_c_handler

extern ticks

apic_timer_interrupt_handler:
    inc qword [ticks]

    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pushfq
    mov rax, cr3
    push rax
    push rsp ; last because it is modified during push

    mov rdi, rsp
    call apic_timer_c_handler
    mov rsp, rax

    pop rsp
    pop rax
    mov cr3, rax
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq