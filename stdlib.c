
long long int write(int fildes, const void* buf, unsigned long long int nbyte);
__asm(
    ".globl _write\n"
    "_write:\n"
    ".cfi_startproc\n"
#ifdef __APPLE__
    "mov $0x2000004, %rax\n"
#else
    "mov $1, %rax\n"
#endif
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

void println_char(char c) {
    char s[2] = {c, '\n'};
    write(1, s, 2);
}

void println_int(long long int n) {
    char s[23] = "";
    int len = 0;
    s[23 - 1 - len++] = '\n';

    const int neg = n < 0;
    n = neg ? -n : n;

    do {
        const char rem = n % 10;
        n /= 10;
        s[23 - 1 - len++] = rem + '0';
    } while (n != 0);

    if (neg) s[23 - 1 - len++] = '-';

    write(1, s + 23 - len, len);
}

void println_string(char* s, int len) {
    s[len++] = '\n';
    write(1, s, len);
}
