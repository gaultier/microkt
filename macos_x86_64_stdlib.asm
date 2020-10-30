// Calling convention: System V ABI


.data
    int_to_string_data: .fill 21, 1, 0

.text

// 1st arg: string label id
// 2nd arg: string label length
// Returns: void
// No stack usage
print:
movq %rax, %rsi
movq %rdi, %rdx

movq $0x2000004, %rax
movq $1, %rdi
syscall

movq $0, %rax
movq $0, %rdi
movq $0, %rsi
movq $0, %rdx
ret

// No args
exit_ok:
movq $0x2000001, %rax
movq $0, %rdi
syscall
ret


// 1st arg: integer
// Returns: void
// No stack usage
// Uses int_to_string_data
int_to_string:
    // Reset the buffer
    movq $0, int_to_string_data+0(%rip)
    movq $0, int_to_string_data+1(%rip)
    movq $0, int_to_string_data+2(%rip)
    movq $0, int_to_string_data+3(%rip)
    movq $0, int_to_string_data+4(%rip)
    movq $0, int_to_string_data+5(%rip)
    movq $0, int_to_string_data+6(%rip)
    movq $0, int_to_string_data+7(%rip)
    movq $0, int_to_string_data+8(%rip)
    movq $0, int_to_string_data+9(%rip)
    movq $0, int_to_string_data+10(%rip)
    movq $0, int_to_string_data+11(%rip)
    movq $0, int_to_string_data+12(%rip)
    movq $0, int_to_string_data+13(%rip)
    movq $0, int_to_string_data+14(%rip)
    movq $0, int_to_string_data+15(%rip)
    movq $0, int_to_string_data+16(%rip)
    movq $0, int_to_string_data+17(%rip)
    movq $0, int_to_string_data+18(%rip)
    movq $0, int_to_string_data+19(%rip)
    movq $0, int_to_string_data+20(%rip)

    leaq int_to_string_data+21(%rip), %r9 // r9: Point at the end of the buffer
    xorq %r8, %r8 // r8: Loop index
    
    int_to_string_loop:
        cmpq $0, %rax // While dividee != 0
        jz int_to_string_end

        // Dividee / 10
        movq $10, %rcx 
        xorq %rdx, %rdx
        div %rcx

        add $48, %rdx // Convert integer to character by adding '0'

        dec %r9 // *(--end) = rem
        movb %dl, (%r9)

        incq %r8
        jmp int_to_string_loop

    int_to_string_end:
      // Epilog
      ret
