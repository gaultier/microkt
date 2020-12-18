#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"

static size_t gc_round = 0;
static size_t gc_allocated_bytes = 0;
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
    void* aa_data;
};

typedef struct alloc_atom alloc_atom;
static alloc_atom* objs = NULL;       // In use
static alloc_atom* objs_free = NULL;  // Free list, re-usable

static alloc_atom* mkt_alloc_atom_make(size_t size) {
    const size_t bytes = sizeof(runtime_val_header) + sizeof(size_t) + size;
    alloc_atom* atom = calloc(1, bytes);
    CHECK((void*)atom, !=, NULL, "%p");

    // Insert at the start
    if (objs) {
        atom->aa_next = objs->aa_next;
        objs->aa_next = atom;
    } else
        objs = atom;

    CHECK((void*)objs, !=, NULL, "%p");

    gc_allocated_bytes += bytes;

    return atom;
}

void mkt_init() {
    // Dummy unused atom to avoid dealing with NULL
    // Not managed by the GC and alive for the whole program duration
    /* objs_free = objs = calloc(1, sizeof(alloc_atom)); */

    /* CHECK((void*)objs, !=, NULL, "%p"); */
    /* CHECK((void*)objs_free, !=, NULL, "%p"); */
}

static void mkt_gc_obj_mark(runtime_val_header* header) {
    CHECK((void*)header, !=, NULL, "%p");

    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;
    log_debug(
        "gc_round=%zu gc_allocated_bytes=%zu header: size=%llu color=%u tag=%u "
        "ptr=%p",
        gc_round, gc_allocated_bytes, header->rv_size, header->rv_color,
        header->rv_tag, (void*)(header + 1));

    if (header->rv_tag & RV_TAG_STRING) return;  // No transitive refs possible

    UNIMPLEMENTED();  // TODO: gray worklist
}

// TODO: optimize
static alloc_atom* mkt_gc_atom_find_data_by_addr(size_t addr) {
    alloc_atom* atom = objs;
    while (atom) {
        if (addr == (size_t)&atom->aa_data) return atom;
        atom = atom->aa_next;
    }
    return NULL;
}

static void mkt_gc_scan_stack(char* stack_bottom, char* stack_top) {
    CHECK((void*)stack_bottom, !=, NULL, "%p");
    CHECK((void*)stack_top, !=, NULL, "%p");
    CHECK((void*)stack_bottom, <=, (void*)stack_top, "%p");

    log_debug("gc_round=%zu gc_allocated_bytes=%zu size=%zu bottom=%p top=%p",
              gc_round, gc_allocated_bytes, stack_top - stack_bottom,
              (void*)stack_bottom, (void*)stack_top);

    while (stack_bottom < stack_top - sizeof(size_t)) {
        size_t addr = *(size_t*)stack_bottom;
        alloc_atom* atom = mkt_gc_atom_find_data_by_addr(addr);
        if (atom == NULL) {
            stack_bottom += 1;
            continue;
        }
        runtime_val_header* header = &atom->aa_header;
        log_debug(
            "gc_round=%zu gc_allocated_bytes=%zu header: size=%llu color=%u "
            "tag=%u ptr=%p",
            gc_round, gc_allocated_bytes, header->rv_size, header->rv_color,
            header->rv_tag, (void*)addr);

        mkt_gc_obj_mark(header);

        stack_bottom += sizeof(runtime_val_header) + header->rv_size;
    }
}

static void mkt_gc_obj_blacken(runtime_val_header* header) {
    CHECK((void*)header, !=, NULL, "%p");

    // TODO: optimize to go from white -> black directly
    if (header->rv_tag & RV_TAG_STRING) return;
}

static void mkt_gc_trace_refs() {
    // TODO
}

static void mkt_gc_sweep() {
    alloc_atom** atom = &objs;

    while (*atom) {
        if ((*atom)->aa_header.rv_tag & RV_TAG_MARKED) {  // Skip
            // Reset the marked bit
            (*atom)->aa_header.rv_tag =
                (*atom)->aa_header.rv_tag & ~RV_TAG_MARKED;
            atom = &(*atom)->aa_next;
        } else {  // Remove
            alloc_atom* to_free = *atom;

            log_debug(
                "Free: gc_round=%zu gc_allocated_bytes=%zu header: size=%llu "
                "color=%u tag=%u ptr=%p",
                gc_round, gc_allocated_bytes, to_free->aa_header.rv_size,
                to_free->aa_header.rv_color, to_free->aa_header.rv_tag,
                (void*)to_free);

            const size_t bytes = sizeof(runtime_val_header) + sizeof(size_t) +
                                 to_free->aa_header.rv_size;
            CHECK(gc_allocated_bytes, >=, (size_t)bytes, "%zu");
            gc_allocated_bytes -= bytes;

            // No actual freeing to be able to re-use the memory, just add the
            // atom to the free list
            if (objs_free) {
                to_free->aa_next = objs_free->aa_next;
                objs_free->aa_next = to_free;
            } else
                objs_free = to_free;

            CHECK((void*)objs_free, !=, NULL, "%p");

            *atom = (*atom)->aa_next;
        }
    }
}

static void mkt_gc(char* stack_bottom, char* stack_top) {
    CHECK((void*)stack_bottom, !=, NULL, "%p");
    CHECK((void*)stack_top, !=, NULL, "%p");
    CHECK((void*)stack_bottom, <=, (void*)stack_top, "%p");

    gc_round += 1;

    log_debug("stats before: round=%zu gc_allocated_bytes=%zu", gc_round,
              gc_allocated_bytes);
    mkt_gc_scan_stack(stack_bottom, stack_top);
    mkt_gc_trace_refs();
    mkt_gc_sweep();
}

void* mkt_string_make(size_t size, char* stack_bottom, char* stack_top) {
    CHECK((void*)stack_bottom, !=, NULL, "%p");
    CHECK((void*)stack_top, !=, NULL, "%p");
    CHECK((void*)stack_bottom, <=, (void*)stack_top, "%p");

    mkt_gc(stack_bottom, stack_top);

    alloc_atom* atom = mkt_alloc_atom_make(size);
    CHECK((void*)atom, !=, NULL, "%p");
    atom->aa_header =
        (runtime_val_header){.rv_size = size, .rv_tag = RV_TAG_STRING};

    log_debug(
        "stats after: round=%zu string_make_size=%zu gc_allocated_bytes=%zu",
        gc_round, size, gc_allocated_bytes);
    return &atom->aa_data;
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
    CHECK((void*)s, !=, NULL, "%p");

    runtime_val_header header = *(runtime_val_header*)(s - 8);
    CHECK(header.rv_tag & RV_TAG_STRING, !=, 0, "%u");

    const char newline = '\n';
    write(1, s, header.rv_size);
    write(1, &newline, 1);
}

char* mkt_string_concat(const char* a, const char* b, char* stack_bottom,
                        char* stack_top) {
    CHECK((void*)a, !=, NULL, "%p");
    CHECK((void*)b, !=, NULL, "%p");
    CHECK((void*)stack_bottom, !=, NULL, "%p");
    CHECK((void*)stack_top, !=, NULL, "%p");
    CHECK((void*)stack_bottom, <=, (void*)stack_top, "%p");

    const size_t a_len = *(a - 8);
    const size_t b_len = *(b - 8);

    char* const ret = mkt_string_make(a_len + b_len, stack_bottom, stack_top);
    CHECK((void*)ret, !=, NULL, "%p");
    CHECK((void*)a, !=, NULL, "%p");
    CHECK((void*)b, !=, NULL, "%p");

    memcpy(ret, a, a_len);
    memcpy(ret + a_len, b, b_len);
    *(ret - 8) = a_len + b_len;

    return ret;
}

