bits 64

section .text
global apic_timer_interrupt_handler
extern apic_c_timer_interrupt_handler

apic_timer_interrupt_handler:
    test byte [rsp + 8], 3
    jz .from_kernel
    
.from_user:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
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
    
    mov rdi, rsp
    mov rsi, 1
    call apic_c_timer_interrupt_handler
    
    test rax, rax
    je .no_switch_user
    mov rsp, rax
    
.no_switch_user:
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
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    iretq
    
.from_kernel:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
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
    
    mov rdi, rsp
    call apic_c_timer_interrupt_handler
    
    test rax, rax
    je .no_switch_rsp
    mov rsp, rax

.no_switch_rsp:
    pop rax
    ;mov cr3, rax
    popfq
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    iretq