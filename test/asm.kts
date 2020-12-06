
asm("""
leaq .Lsyscall_write(%rip), %rdi
mov (%rdi), %rax
mov $1, %edi
leaq .Ltrue+2(%rip), %rsi
mov $3, %rdx
syscall
xorq %rax, %rax
""")
// expect: ue
