#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void* alloc(size_t size) { return malloc(size); }

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

void println_string(char* s, size_t len) {
    s[len++] = '\n';
    write(1, s, len);
}

char* string_concat(const char* a, const char* b) {
    const size_t a_len = *(a - 8);
    const size_t b_len = *(b - 8);

    char* const ret = alloc(a_len + b_len);
    memcpy(ret, a, a_len);
    memcpy(ret + a_len, b, b_len);
    *(ret - 8) = a_len + b_len;

    return ret;
}

