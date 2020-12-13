#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "buf.h"

static size_t* objs = NULL;

void mkt_scan_stack(runtime_val_header* stack_bottom,
                    runtime_val_header* stack_top) {
    printf("Stack size: %zu\n", stack_top - stack_bottom);

    while (stack_bottom < stack_top) {
        printf("Stack: bottom=%p top=%p\n", stack_bottom, stack_top);
        const size_t header_val = *(size_t*)stack_bottom;
        runtime_val_header header = {.rv_size = (header_val >> 10),
                                     .rv_color = (header_val >> 54) & (1 << 9),
                                     .rv_tag = (header_val & 0xff)};

        printf("header: size=%llu color=%u tag=%u\n", header.rv_size,
               header.rv_color, header.rv_tag);

        stack_bottom += sizeof(runtime_val_header) + header.rv_size;
    }
}

void* mkt_alloc(size_t size, runtime_val_header* stack_bottom,
                runtime_val_header* stack_top) {
    mkt_scan_stack(stack_bottom, stack_top);
    size_t* obj = malloc(sizeof(runtime_val_header) + size);

    runtime_val_header header = {
        .rv_size = size, .rv_color = 0, .rv_tag = TYPE_STRING};
    const long long unsigned int header_val =
        (header.rv_size << 10) | (header.rv_color << 8) | header.rv_tag;

    *obj = header_val;
    buf_push(objs, (size_t)obj);
    obj += 1;
    return obj;
}

void mkt_println_bool(int b) {
    if (b) {
        const char s[] = "true\n";
        write(1, s, sizeof(s) - 1);
    } else {
        const char s[] = "false\n";
        write(1, s, sizeof(s) - 1);
    }
}

void mkt_println_char(char c) {
    char s[2] = {c, '\n'};
    write(1, s, 2);
}

void mkt_println_int(long long int n) {
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

void mkt_println_string(char* s, size_t len) {
    s[len++] = '\n';
    write(1, s, len);
}

char* mkt_string_concat(const char* a, const char* b,
                        runtime_val_header* stack_bottom,
                        runtime_val_header* stack_top) {
    const size_t a_len = *(a - 8);
    const size_t b_len = *(b - 8);

    char* const ret = mkt_alloc(a_len + b_len, stack_bottom, stack_top);
    memcpy(ret, a, a_len);
    memcpy(ret + a_len, b, b_len);
    *(ret - 8) = a_len + b_len;

    return ret;
}

