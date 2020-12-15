#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"

static char* objs = NULL;
static char* objs_end = NULL;

static const size_t heap_size_initial =
    2 * 1024 * 1024;  // 2 Mib initial heap size

void mkt_init() {
    objs = malloc(heap_size_initial);
    objs_end = objs;
}

void mkt_scan_heap() {
    char* obj = (char*)objs;
    while (obj < (char*)objs_end) {
        runtime_val_header header = *(runtime_val_header*)obj;
        log_debug("header: size=%llu color=%u tag=%u ptr=%p", header.rv_size,
                  header.rv_color, header.rv_tag,
                  (void*)(obj + sizeof(header)));

        if (header.rv_tag == 0) return;

        obj += sizeof(runtime_val_header) + header.rv_size;
    }
}

void mkt_scan_stack(char* stack_bottom, char* stack_top) {
    log_debug("size=%zu bottom=%p top=%p", stack_top - stack_bottom,
              (void*)stack_bottom, (void*)stack_top);

    while (stack_bottom < stack_top) {
        uintptr_t addr = *(uintptr_t*)stack_bottom;
        log_debug("addr=%zu", addr);
        if (addr < (uintptr_t)objs || addr >= (uintptr_t)objs_end) {
            stack_bottom += 1;
            continue;
        }
        runtime_val_header header = *(runtime_val_header*)(addr - 8);
        log_debug("header: size=%llu color=%u tag=%u", header.rv_size,
                  header.rv_color, header.rv_tag);

        if (header.rv_tag == 0) return;

        stack_bottom += sizeof(runtime_val_header) + header.rv_size;
    }
}

void* mkt_alloc(size_t size, char* stack_bottom, char* stack_top) {
    mkt_scan_stack(stack_bottom, stack_top);
    mkt_scan_heap();

    // TODO: realloc
    char* obj = (char*)objs_end;
    objs_end += sizeof(runtime_val_header) + size;
    if (objs_end >= objs + heap_size_initial) UNIMPLEMENTED();

    runtime_val_header header = {.rv_size = size};
    size_t* header_val = (size_t*)&header;
    size_t* obj_header = (size_t*)obj;
    *obj_header = *header_val;
    obj += sizeof(runtime_val_header);

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

void mkt_println_string(char* s) {
    runtime_val_header header = *(runtime_val_header*)(s - 8);

    const char newline = '\n';
    write(1, s, header.rv_size);
    write(1, &newline, 1);
}

char* mkt_string_concat(const char* a, const char* b, char* stack_bottom,
                        char* stack_top) {
    const size_t a_len = *(a - 8);
    const size_t b_len = *(b - 8);

    char* const ret = mkt_alloc(a_len + b_len, stack_bottom, stack_top);
    memcpy(ret, a, a_len);
    memcpy(ret + a_len, b, b_len);
    *(ret - 8) = a_len + b_len;

    return ret;
}

