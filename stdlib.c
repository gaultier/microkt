#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
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

alloc_atom* mkt_alloc_atom_make(size_t size) {
    alloc_atom* atom = calloc(1, sizeof(alloc_atom) + size);

    if (objs)
        objs->aa_next = atom;
    else
        objs = atom;

    return atom;
}

void mkt_init() {}

void mkt_obj_mark(runtime_val_header* header) {
    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;
    log_debug("header: size=%llu color=%u tag=%u ptr=%p", header->rv_size,
              header->rv_color, header->rv_tag, (void*)(header + 1));

    if (header->rv_tag & RV_TAG_STRING) return;  // No transitive refs possible

    UNIMPLEMENTED();  // TODO: gray worklist
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
    // TODO
}

void mkt_sweep() {
    alloc_atom* atom = objs;
    alloc_atom* previous = NULL;

    while (atom) {
        log_debug("header: size=%llu color=%u tag=%u ptr=%p",
                  atom->aa_header.rv_size, atom->aa_header.rv_color,
                  atom->aa_header.rv_tag, (void*)atom);

        if (atom->aa_header.rv_tag & RV_TAG_MARKED) {  // Skip
            // Reset the marked bit
            atom->aa_header.rv_tag = atom->aa_header.rv_tag & ~RV_TAG_MARKED;
            previous = atom;
            atom = atom->aa_next;
        } else {  // Remove
            if (previous)
                previous->aa_next = atom->aa_next;
            else
                objs = NULL;

            alloc_atom* to_free = atom;
            atom = atom->aa_next;

            log_debug("Free: header: size=%llu color=%u tag=%u ptr=%p",
                      to_free->aa_header.rv_size, to_free->aa_header.rv_color,
                      to_free->aa_header.rv_tag, (void*)to_free);
            free(to_free);
        }
    }
}

void mkt_scan_regs() {
    // TODO?
}

void mkt_gc(char* stack_bottom, char* stack_top) {
    mkt_scan_stack(stack_bottom, stack_top);
    mkt_scan_regs();
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

