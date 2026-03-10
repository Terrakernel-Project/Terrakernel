bits 64

section .bss
store_data: resq 2
params: resq 4

section .text
global execute_ring3

extern exec_ring3_helper

execute_ring3:
	mov [params+0x00], rdx
	mov [params+0x08], rcx
	mov [params+0x10], r8
	mov [params+0x18], r9

	mov [store_data+0x00], rdi
	mov [store_data+0x08], rsi

	mov rdi, rsp

	call exec_ring3_helper ; the weird mov store_data is just to not change rsp before changing the stack

	mov rdi, [store_data+0x00]
	mov rsi, [store_data+0x08]	

	push 0x1B
	push rsi
	push 0x202
	push 0x23
	push rdi

	; set the parameters of the function
	mov rdi, [params+0x00]
	mov rsi, [params+0x08]
	mov rdx, [params+0x10]
	mov r9, [params+0x18]

	iretq ; make the switch
