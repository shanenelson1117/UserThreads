// Only called from sigprof handler
    .global switch_no_lock

// void switch(uthread_t* next, int worker_idx);
switch_no_lock:
    push %r12
    push %r13
    push %r14
    push %r15
    push %rbx
    push %rbp

    lea current_uthreads(%rip), %rcx
    mov (%rcx, %rsi, 8), %rdx
    mov %rsp, 0(%rdx)
    mov 0(%rdi), %rsp
    mov %rdi, (%rcx, %rsi, 8)

    // Restore important registers
    pop %rbp
    pop %rbx
    pop %r15
    pop %r14
    pop %r13
    pop %r12

    ret

// Tell the linker that we don't need the stack to have executable permissions
.section .note.GNU-stack,"",@progbits
