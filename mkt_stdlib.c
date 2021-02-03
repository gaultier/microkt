#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
// macOS Big Sur's mman.h header does not define MAP_ANONYMOUS for some reason
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x1000
#endif

#include <unistd.h>

#include "common.h"
#include "probes.h"

static u64 gc_round = 0;
static u64 gc_allocated_bytes = 0;
static const unsigned char RV_TAG_MARKED = 0x01;
static const unsigned char RV_TAG_STRING = 0x02;
static const unsigned char RV_TAG_INSTANCE = 0x04;
static void* mkt_rsp = NULL;
void* mkt_rbp = NULL;

void* mkt_save_rbp() {
#define READ_RBP() __asm__ volatile("movq %%rbp, %0" : "=r"(mkt_rbp))
    READ_RBP();
#undef READ_RBP

    return mkt_rbp;
}

void* mkt_save_rsp() {
#define READ_RSP() __asm__ volatile("movq %%rsp, %0" : "=r"(mkt_rsp))
    READ_RSP();
#undef READ_RSP

    return mkt_rsp;
}

// TODO: optimize
static void* mkt_alloc(u64 len) {
    void* p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    CHECK(p, !=, NULL, "%p");
    return p;
}

typedef struct {
    u64 rv_size : 54;
    u32 rv_color : 2;
    u32 rv_tag : 8;
} runtime_val_header;

struct alloc_atom {
    struct alloc_atom* aa_next;
    runtime_val_header aa_header;
    char aa_data[];
};

typedef struct alloc_atom alloc_atom;
static alloc_atom* objs = NULL;  // In use

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

static alloc_atom* mkt_alloc_atom_make(u64 size) {
    const u64 bytes = sizeof(runtime_val_header) + sizeof(alloc_atom*) + size;
    alloc_atom* atom = mkt_alloc(bytes);
    CHECK((void*)atom, !=, NULL, "%p");
    atom->aa_header = (runtime_val_header){0};
    atom->aa_next = NULL;

    atom_cons(atom, &objs);

    gc_allocated_bytes += bytes;

    return atom;
}

static void mkt_gc_obj_mark(runtime_val_header* header) {
    CHECK((void*)header, !=, NULL, "%p");

    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;

    if (header->rv_tag & RV_TAG_STRING) return;  // No transitive refs possible

    // UNIMPLEMENTED();  // TODO: gray worklist for objects
}

// TODO: optimize
static alloc_atom* mkt_gc_atom_find_data_by_addr(void* ptr) {
    if (ptr == NULL) return NULL;

    alloc_atom* atom = objs;
    while (atom) {
        if (ptr == &atom->aa_data) return atom;
        atom = atom->aa_next;
    }
    return NULL;
}

static void mkt_gc_scan_stack() {
    CHECK((void*)mkt_rsp, <=, (void*)mkt_rbp, "%p");

    for (char* p = (char*)mkt_rsp; p < (char*)mkt_rbp - sizeof(void*); p++) {
        alloc_atom* atom = mkt_gc_atom_find_data_by_addr(*((void**)p));
        if (atom == NULL) continue;

        mkt_gc_obj_mark(&atom->aa_header);
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
    MKT_GC_SWEEP_START(gc_round, gc_allocated_bytes);
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
        const u64 bytes = sizeof(runtime_val_header) + sizeof(alloc_atom*) +
                          atom->aa_header.rv_size;
        CHECK(gc_allocated_bytes, >=, bytes, "%llu");
        gc_allocated_bytes -= bytes;

        alloc_atom* to_free = atom;
        atom = atom->aa_next;
        if (previous)
            previous->aa_next = atom;
        else
            objs = atom;

        MKT_GC_SWEEP_FREE(gc_round, gc_allocated_bytes, (void*)to_free);
        CHECK(munmap(to_free, bytes), ==, 0, "%d");
    }
    MKT_GC_SWEEP_DONE(gc_round, gc_allocated_bytes);
}

void mkt_gc() {
    mkt_save_rsp();
    CHECK((void*)mkt_rsp, <=, (void*)mkt_rbp, "%p");

    gc_round += 1;

    mkt_gc_scan_stack();
    mkt_gc_trace_refs();
    mkt_gc_sweep();
}

void* mkt_string_make(u64 size) {
    mkt_gc();

    alloc_atom* atom = mkt_alloc_atom_make(size);
    CHECK((void*)atom, !=, NULL, "%p");
    atom->aa_header =
        (runtime_val_header){.rv_size = size, .rv_tag = RV_TAG_STRING};

    return &atom->aa_data;
}

void* mkt_instance_make(u64 size) {
    mkt_gc();

    alloc_atom* atom = mkt_alloc_atom_make(size);
    CHECK((void*)atom, !=, NULL, "%p");
    atom->aa_header =
        (runtime_val_header){.rv_size = size, .rv_tag = RV_TAG_INSTANCE};

    return &atom->aa_data;
}

void mkt_bool_println(i32 b) {
    if (b) {
        const char s[] = "true\n";
        write(1, s, sizeof(s) - 1);
    } else {
        const char s[] = "false\n";
        write(1, s, sizeof(s) - 1);
    }
}

void mkt_char_println(char c) {
    char s[2] = {c, '\n'};
    write(1, s, 2);
}

static void mkt_int_to_string(i64 n, char* s, i32* s_len) {
    CHECK((void*)s_len, !=, NULL, "%p");

    *s_len = 0;
    const i32 neg = n < 0;
    n = neg ? -n : n;

    do {
        const char rem = n % 10;
        n /= 10;

        CHECK(*s_len, >=, 0, "%d");
        CHECK(*s_len, <=, 22, "%d");
        s[22 - (*s_len)++] = rem + '0';
    } while (n != 0);

    if (neg) s[22 - (*s_len)++] = '-';
}

void mkt_int_println(i64 n) {
    char s[23] = "";
    i32 s_len = 0;
    mkt_int_to_string(n, s, &s_len);

    write(1, s + sizeof(s) - s_len, s_len);
    const char newline = '\n';
    write(1, &newline, 1);
}

void mkt_string_println(char* s) {
    CHECK((void*)s, !=, NULL, "%p");
    const runtime_val_header* const s_header = ((runtime_val_header*)s) - 1;

    CHECK(s_header->rv_tag & RV_TAG_STRING, !=, 0, "%u");

    write(1, s, s_header->rv_size);
    const char newline = '\n';
    write(1, &newline, 1);
}

char* mkt_string_concat(const char* a, const char* b) {
    CHECK((void*)a, !=, NULL, "%p");
    CHECK((void*)b, !=, NULL, "%p");
    const runtime_val_header* a_header = (runtime_val_header*)a - 1;
    const runtime_val_header* b_header = (runtime_val_header*)b - 1;
    CHECK((void*)a_header, !=, NULL, "%p");
    CHECK((void*)b_header, !=, NULL, "%p");

    CHECK(a_header->rv_tag & RV_TAG_STRING, !=, 0, "%u");
    CHECK(b_header->rv_tag & RV_TAG_STRING, !=, 0, "%u");

    char* const ret = mkt_string_make(a_header->rv_size + b_header->rv_size);
    CHECK((void*)ret, !=, NULL, "%p");
    CHECK((void*)a, !=, NULL, "%p");
    CHECK((void*)b, !=, NULL, "%p");

    memcpy(ret, a, a_header->rv_size);
    memcpy(ret + a_header->rv_size, b, b_header->rv_size);
    *(ret - 8) = a_header->rv_size + b_header->rv_size;

    return ret;
}

void mkt_instance_println(void* addr) {
    CHECK(addr, !=, NULL, "%p");

    const char s[] = "Instance of size ";
    write(1, s, sizeof(s) - 1);

    const runtime_val_header* const header =
        (runtime_val_header*)((u64)addr - sizeof(runtime_val_header*));
    CHECK(header->rv_tag & RV_TAG_INSTANCE, !=, 0, "%d");

    char size_s[23] = "";
    i32 size_s_len = 0;
    mkt_int_to_string(header->rv_size, size_s, &size_s_len);
    write(1, size_s + sizeof(size_s) - size_s_len, size_s_len);

    const char newline = '\n';
    write(1, &newline, 1);
}
