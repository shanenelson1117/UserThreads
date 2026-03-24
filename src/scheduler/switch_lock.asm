    .extern unlock
    .extern pop_mask
    .global switch_lock

// void switch(uthread_t* next, spinlock* lk);
switch_lock:
    push %r12
    push %r13
    push %r14
    push %r15
    push %rbx
    push %rbp
    push %r12


    // Save lock in rax
    mov %rsi, %rax

    mov current_uthread(%rip), %rsi
    mov %rsp, 0(%rsi)
    mov 0(%rdi), %rsp
    mov %rdi, current_uthread(%rip)

    // Unlock the passed in lock
    mov %rax, %rdi
    call unlock

    // Restore important registers
    pop %r12
    pop %rbp
    pop %rbx
    pop %r15
    pop %r14
    pop %r13
    pop %r12

    push %r12

    call pop_mask

    pop %r12

    ret

// Tell the linker that we don't need the stack to have executable permissions
.section .note.GNU-stack,"",@progbits
