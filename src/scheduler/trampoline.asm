    .global trampoline

trampoline:
    pop %rdi              # first arg = passed in void*
    pop %rax              # rax = f()
    push %rax             # alignment
    call enable_sigprof   # enable interrupts for this thread
    call *%rax            # call f() with args in rdi
    call exit_yield      # terminate routine


.section .note.GNU-stack,"",@progbits

