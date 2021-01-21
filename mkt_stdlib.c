#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "probes.h"

static u64 gc_round = 0;
static u64 gc_allocated_bytes = 0;
static const unsigned char RV_TAG_MARKED = 0x01;
static const unsigned char RV_TAG_STRING = 0x02;
static const unsigned char RV_TAG_INSTANCE = 0x04;
static i64* mkt_rbp;
static i64* mkt_rsp;
static i64* stack_top;

#define READ_RBP() __asm__ volatile("movq %%rbp, %0" : "=r"(mkt_rbp))
#define READ_RSP() __asm__ volatile("movq %%rsp, %0" : "=r"(mkt_rsp))

// TODO: optimize
static void* mkt_alloc(u64 len) {
    void* p =
        mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
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

void mkt_init() {
    READ_RBP();
    stack_top = (i64*)*mkt_rbp;
}

static void mkt_gc_obj_mark(runtime_val_header* header) {
    CHECK((void*)header, !=, NULL, "%p");

    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;

    if (header->rv_tag & RV_TAG_STRING) return;  // No transitive refs possible

    // UNIMPLEMENTED();  // TODO: gray worklist for objects
}

// TODO: optimize
static alloc_atom* mkt_gc_atom_find_data_by_addr(u64 addr) {
    if (addr == 0) return NULL;

    alloc_atom* atom = objs;
    while (atom) {
        if (addr == (u64)&atom->aa_data) return atom;
        atom = atom->aa_next;
    }
    return NULL;
}

static void mkt_gc_scan_stack(const i64* stack_bottom) {
    CHECK((void*)stack_bottom, !=, NULL, "%p");

    const char* s_bottom = (char*)stack_bottom;
    CHECK((void*)stack_bottom, <=, (void*)stack_top, "%p");

    const char* s_top = (char*)stack_top;
    while (s_bottom < s_top - sizeof(i64)) {
        u64 addr = *(u64*)s_bottom;
        alloc_atom* atom = mkt_gc_atom_find_data_by_addr(addr);
        if (atom == NULL) {
            s_bottom += 1;
            continue;
        }
        runtime_val_header* header = &atom->aa_header;
        mkt_gc_obj_mark(header);

        s_bottom += sizeof(i64);
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
    /* MKT_GC_SWEEP_START(gc_round, gc_allocated_bytes); */
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
        CHECK(gc_allocated_bytes, >=, bytes, PRIu64);
        gc_allocated_bytes -= bytes;

        alloc_atom* to_free = atom;
        atom = atom->aa_next;
        if (previous)
            previous->aa_next = atom;
        else
            objs = atom;

        /* MKT_GC_SWEEP_FREE(gc_round, gc_allocated_bytes, (void*)to_free); */
        CHECK(munmap(to_free, bytes), ==, 0, "%d");
    }
    /* MKT_GC_SWEEP_DONE(gc_round, gc_allocated_bytes); */
}

static void mkt_gc() {
    READ_RSP();

    gc_round += 1;

    mkt_gc_scan_stack(mkt_rsp);
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

void mkt_string_println(char* s, const runtime_val_header* s_header) {
    CHECK((void*)s, !=, NULL, "%p");
    CHECK((void*)s_header, !=, NULL, "%p");

    CHECK(s_header->rv_tag & RV_TAG_STRING, !=, 0, "%u");

    write(1, s, s_header->rv_size);
    const char newline = '\n';
    write(1, &newline, 1);
}

char* mkt_string_concat(const char* a, const runtime_val_header* a_header,
                        const char* b, const runtime_val_header* b_header) {
    char* const ret = mkt_string_make(a_header->rv_size + b_header->rv_size);
    CHECK((void*)ret, !=, NULL, "%p");
    CHECK((void*)a, !=, NULL, "%p");
    CHECK((void*)b, !=, NULL, "%p");

    for (u64 i = 0; i < a_header->rv_size; i++) ret[i] = a[i];
    for (u64 i = 0; i < b_header->rv_size; i++)
        ret[a_header->rv_size + i] = b[i];
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
