#pragma once
const char stdlib[] = "\n"
"\n"
".data\n"
"    int_to_string_data: .fill 21, 1, 0\n"
"\n"
".text\n"
"\n"
"print:\n"
"movq %rax, %rsi\n"
"movq %rdi, %rdx\n"
"\n"
"movq $0x2000004, %rax\n"
"movq $1, %rdi\n"
"syscall\n"
"\n"
"movq $0, %rax\n"
"movq $0, %rdi\n"
"movq $0, %rsi\n"
"movq $0, %rdx\n"
"ret\n"
"\n"
"print_int:\n"
"    call int_to_string\n"
"\n"
"    movq %rax, %rdi // len\n"
"    leaq int_to_string_data(%rip), %rax\n"
"    addq $21, %rax\n"
"    subq %rdi, %rax\n"
"    call print\n"
"    ret\n"
"\n"
"\n"
"int_to_string:\n"
"    movq $0, int_to_string_data+0(%rip)\n"
"    movq $0, int_to_string_data+1(%rip)\n"
"    movq $0, int_to_string_data+2(%rip)\n"
"    movq $0, int_to_string_data+3(%rip)\n"
"    movq $0, int_to_string_data+4(%rip)\n"
"    movq $0, int_to_string_data+5(%rip)\n"
"    movq $0, int_to_string_data+6(%rip)\n"
"    movq $0, int_to_string_data+7(%rip)\n"
"    movq $0, int_to_string_data+8(%rip)\n"
"    movq $0, int_to_string_data+9(%rip)\n"
"    movq $0, int_to_string_data+10(%rip)\n"
"    movq $0, int_to_string_data+11(%rip)\n"
"    movq $0, int_to_string_data+12(%rip)\n"
"    movq $0, int_to_string_data+13(%rip)\n"
"    movq $0, int_to_string_data+14(%rip)\n"
"    movq $0, int_to_string_data+15(%rip)\n"
"    movq $0, int_to_string_data+16(%rip)\n"
"    movq $0, int_to_string_data+17(%rip)\n"
"    movq $0, int_to_string_data+18(%rip)\n"
"    movq $0, int_to_string_data+19(%rip)\n"
"    movq $0, int_to_string_data+20(%rip)\n"
"\n"
"    xorq %r8, %r8 // r8: Loop index\n"
"    \n"
"    int_to_string_loop:\n"
"        cmpq $0, %rax // While n != 0\n"
"        jz int_to_string_end\n"
"\n"
"        movq $10, %rcx \n"
"        xorq %rdx, %rdx\n"
"        idiv %rcx\n"
"    \n"
"        add $48, %rdx // Convert integer to character by adding '0'\n"
"        leaq int_to_string_data(%rip), %r11\n"
"        addq $20, %r11\n"
"        subq %r8,  %r11\n"
"        movb %dl, (%r11)\n"
"\n"
"        incq %r8\n"
"        jmp int_to_string_loop\n"
"\n"
"    int_to_string_end:\n"
"      movq %r8, %rax\n"
"      ret\n"
;
