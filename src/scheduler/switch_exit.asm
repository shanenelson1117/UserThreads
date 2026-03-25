
    .global switch_exit

// void switch_exit(uthread_t* next);
switch_exit:
    mov 0(%rdi), %rsp
    mov %rdi, current_uthread(%rip)

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
