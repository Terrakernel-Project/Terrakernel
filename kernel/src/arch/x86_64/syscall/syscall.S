.section .text
.global syscall_func
.extern syscall_handler

syscall_func:
    swapgs

    pushq %rcx
    pushq %r11

    pushq %rax
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %r10
    pushq %r8
    pushq %r9

    movq %rsp, %rdi
    call syscall_handler

    popq %r9
    popq %r8
    popq %r10
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rax

    popq %r11
    popq %rcx

    swapgs
    sysretq
