#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"

static size_t gc_round = 0;
static size_t gc_allocated_bytes = 0;
static size_t gc_free_bytes = 0;
static const unsigned char RV_TAG_MARKED = 0x01;
static const unsigned char RV_TAG_STRING = 0x02;

typedef struct {
    size_t rv_size : 54;
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

void atom_cons(alloc_atom* item, alloc_atom** head) {
    CHECK((void*)item, !=, NULL, "%p");
    CHECK((void*)head, !=, NULL, "%p");
    CHECK((void*)item, !=, (void*)head, "%p");  // Prevent cycles

    if (head) {
        item->aa_next = *head;
        *head = item;
    } else {
        *head = item;
    }
    CHECK((void*)*head, !=, NULL, "%p");
}

static alloc_atom* mkt_alloc_find_free(size_t size) {
    alloc_atom* atom = objs_free;
    alloc_atom* previous = NULL;

    while (atom) {
        if (atom->aa_header.rv_size >= size) {
            if (previous)
                previous->aa_next = atom->aa_next;
            else
                objs_free = atom->aa_next;

            atom->aa_next = NULL;

            log_debug(
                "reuse: gc_round=%zu gc_allocated_bytes=%zu gc_free_bytes=%zu "
                "ptr=%p "
                "old_size=%zu new_size=%zu",
                gc_round, gc_allocated_bytes, gc_free_bytes,
                (void*)&atom->aa_data, atom->aa_header.rv_size, size);

            gc_free_bytes -= atom->aa_header.rv_size;
            atom->aa_header.rv_size = size;
            return atom;
        }

        if (previous)
            previous = atom;
        else
            objs_free = atom;

        atom = atom->aa_next;
    }
    return NULL;
}

// TODO: reuse atoms from the free list
static alloc_atom* mkt_alloc_atom_make(size_t size) {
    alloc_atom* atom = mkt_alloc_find_free(size);
    if (atom) return atom;

    const size_t bytes =
        sizeof(runtime_val_header) + sizeof(alloc_atom*) + size;
    atom = calloc(1, bytes);
    CHECK((void*)atom, !=, NULL, "%p");

    atom_cons(atom, &objs);

    gc_allocated_bytes += bytes;

    return atom;
}

void mkt_init() {}

static void mkt_gc_obj_mark(runtime_val_header* header) {
    CHECK((void*)header, !=, NULL, "%p");

    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;
    log_debug(
        "gc_round=%zu gc_allocated_bytes=%zu header: size=%zu color=%u tag=%u "
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

    while (stack_bottom < stack_top - sizeof(void*)) {
        size_t addr = *(size_t*)stack_bottom;
        alloc_atom* atom = mkt_gc_atom_find_data_by_addr(addr);
        if (atom == NULL) {
            stack_bottom += 1;
            continue;
        }
        runtime_val_header* header = &atom->aa_header;
        log_debug(
            "gc_round=%zu gc_allocated_bytes=%zu header: size=%zu color=%u "
            "tag=%u ptr=%p",
            gc_round, gc_allocated_bytes, header->rv_size, header->rv_color,
            header->rv_tag, (void*)addr);

        mkt_gc_obj_mark(header);

        stack_bottom += sizeof(void*);
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
    alloc_atom* atom = objs;
    alloc_atom* previous = NULL;

    while (atom) {
        if (atom->aa_header.rv_tag & RV_TAG_MARKED) {  // Skip
            // Reset the marked bit
            atom->aa_header.rv_tag &= ~RV_TAG_MARKED;
            previous = atom;
            atom = atom->aa_next;
            continue;
        }

        // Remove

        log_debug(
            "Free: gc_round=%zu gc_allocated_bytes=%zu header: size=%zu "
            "color=%u tag=%u ptr=%p",
            gc_round, gc_allocated_bytes, atom->aa_header.rv_size,
            atom->aa_header.rv_color, atom->aa_header.rv_tag, (void*)atom);

        const size_t bytes = sizeof(runtime_val_header) + sizeof(alloc_atom*) +
                             atom->aa_header.rv_size;
        CHECK(gc_allocated_bytes, >=, (size_t)bytes, "%zu");
        gc_allocated_bytes -= bytes;

        alloc_atom* const to_free = atom;
        atom = atom->aa_next;
        if (previous)
            previous->aa_next = atom;
        else
            objs = atom;

        // No actual freeing to be able to re-use the memory, just add the
        // atom to the free list
        memset_s(&to_free->aa_data, to_free->aa_header.rv_size, 0,
                 to_free->aa_header.rv_size);
        atom_cons(to_free, &objs_free);
        gc_free_bytes += to_free->aa_header.rv_size;
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

