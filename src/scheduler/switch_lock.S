    .extern unlock
    .extern pop_mask
    .global switch_lock

// void switch(uthread_t* next, spinlock* lk, int worker_idx);
switch_lock:
    push %r12
    push %r13
    push %r14
    push %r15
    push %rbx
    push %rbp

    // Save lock in rax
    mov %rsi, %rax

    lea current_uthreads(%rip), %rcx
    mov (%rcx, %rdx, 8), %rsi
    mov %rsp, 0(%rsi)
    mov 0(%rdi), %rsp
    mov %rdi, (%rcx, %rdx, 8)

    // Restore important registers
    pop %rbp
    pop %rbx
    pop %r15
    pop %r14
    pop %r13
    pop %r12

    push %r12

    // Unlock the passed in lock
    mov %rax, %rdi
    call unlock

    pop %r12

    ret

// Tell the linker that we don't need the stack to have executable permissions
.section .note.GNU-stack,"",@progbits
