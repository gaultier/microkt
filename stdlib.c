#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "buf.h"
#include "common.h"

static const unsigned char RV_TAG_MARKED = 0x01;
static const unsigned char RV_TAG_STRING = 0x02;

typedef struct {
    unsigned long long int rv_size : 54;
    unsigned int rv_color : 2;
    unsigned int rv_tag : 8;
} runtime_val_header;

struct alloc_atom {
    struct alloc_atom* aa_next;
    runtime_val_header aa_header;
    char aa_data[];
};

typedef struct alloc_atom alloc_atom;
static alloc_atom* objs = NULL;
static alloc_atom* objs_end = NULL;
static runtime_val_header** gray_objs = NULL;

alloc_atom* mkt_alloc_atom_make(size_t size) {
    alloc_atom* atom = calloc(1, sizeof(alloc_atom) + size);
    objs_end->aa_next = atom;
    objs_end = atom;
    return atom;
}

void mkt_init() {
    alloc_atom* atom = calloc(1, sizeof(alloc_atom));
    objs_end = atom;
}

void mkt_obj_mark(runtime_val_header* header) {
    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;
    buf_push(gray_objs, header);

    log_debug("marked: header=%p", (void*)header);
}

void mkt_scan_heap() {
    alloc_atom* atom = objs;
    while (atom) {
        runtime_val_header* header = &atom->aa_header;
        log_debug("header: size=%llu color=%u tag=%u ptr=%p", header->rv_size,
                  header->rv_color, header->rv_tag, (void*)atom->aa_data);

        mkt_obj_mark(&atom->aa_header);

        atom = atom->aa_next;
    }
}

// TODO: optimize
alloc_atom* mkt_atom_find_data_by_addr(size_t addr) {
    alloc_atom* atom = objs;
    while (atom) {
        if (addr == (size_t)atom->aa_data) return atom;
        atom = atom->aa_next;
    }
    return NULL;
}

void mkt_scan_stack(char* stack_bottom, char* stack_top) {
    log_debug("size=%zu bottom=%p top=%p", stack_top - stack_bottom,
              (void*)stack_bottom, (void*)stack_top);

    while (stack_bottom < stack_top) {
        uintptr_t addr = *(uintptr_t*)stack_bottom;
        alloc_atom* atom = mkt_atom_find_data_by_addr(addr);
        if (atom == NULL) {
            stack_bottom += 1;
            continue;
        }
        runtime_val_header* header = &atom->aa_header;
        log_debug("header: size=%llu color=%u tag=%u ptr=%p", header->rv_size,
                  header->rv_color, header->rv_tag, (void*)addr);

        mkt_obj_mark(header);

        stack_bottom += sizeof(runtime_val_header) + header->rv_size;
    }
}

void mkt_obj_blacken(runtime_val_header* header) {
    // TODO: optimize to go from white -> black directly
    if (header->rv_tag & RV_TAG_STRING) return;
}

void mkt_trace_refs() {
    for (size_t i = 0; i < buf_size(gray_objs); i++)
        mkt_obj_blacken(gray_objs[i]);
}

void mkt_sweep() {}

void mkt_gc(char* stack_bottom, char* stack_top) {
    mkt_scan_stack(stack_bottom, stack_top);
    mkt_scan_heap();
    mkt_trace_refs();
    mkt_sweep();
}

void* mkt_string_make(size_t size, char* stack_bottom, char* stack_top) {
    mkt_gc(stack_bottom, stack_top);

    alloc_atom* atom = mkt_alloc_atom_make(size);
    atom->aa_header =
        (runtime_val_header){.rv_size = size, .rv_tag = RV_TAG_STRING};

    return atom->aa_data;
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
    CHECK(header.rv_tag & RV_TAG_STRING, !=, 0, "%u");

    const char newline = '\n';
    write(1, s, header.rv_size);
    write(1, &newline, 1);
}

char* mkt_string_concat(const char* a, const char* b, char* stack_bottom,
                        char* stack_top) {
    const size_t a_len = *(a - 8);
    const size_t b_len = *(b - 8);

    char* const ret = mkt_string_make(a_len + b_len, stack_bottom, stack_top);
    memcpy(ret, a, a_len);
    memcpy(ret + a_len, b, b_len);
    *(ret - 8) = a_len + b_len;

    return ret;
}

