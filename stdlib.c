long long int write(int fildes, const void *buf, unsigned long long int nbyte);
__asm(
    ".globl _write\n"
    "_write:\n"
    ".cfi_startproc\n"
    "mov $0x2000004, %rax\n"
    "mov $1, %rdi\n"
    "syscall\n"
    "ret\n"
    ".cfi_endproc\n");

void println_bool(int b) {
    if (b) {
        const char s[] = "true\n";
        write(1, s, sizeof(s) - 1);
    } else {
        const char s[] = "false\n";
        write(1, s, sizeof(s) - 1);
    }
}
