.section .text
.global syscall_func
.extern syscall_handler

syscall_func:
    swapgs
    
    pushq %r11
    pushq %rcx
    pushq %r9
    pushq %r8
    pushq %r10
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax
    
    movq %rsp, %rdi
    
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    call syscall_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    
    addq $8, %rsp
    
    popq %rdi
    popq %rsi
    popq %rdx
    popq %r10
    popq %r8
    popq %r9
    
    popq %rcx
    popq %r11
    
    swapgs
    sysretq
