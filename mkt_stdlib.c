#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "probes.h"

static size_t gc_round = 0;
static size_t gc_allocated_bytes = 0;
static const unsigned char RV_TAG_MARKED = 0x01;
static const unsigned char RV_TAG_STRING = 0x02;
static const unsigned char RV_TAG_INSTANCE = 0x04;
static intptr_t* mkt_rbp;
static intptr_t* mkt_rsp;
static intptr_t* stack_top;

#define READ_RBP() __asm__ volatile("movq %%rbp, %0" : "=r"(mkt_rbp))
#define READ_RSP() __asm__ volatile("movq %%rsp, %0" : "=r"(mkt_rsp))

#define MKT_PROT_READ 0x01
#define MKT_PROT_WRITE 0x02
#define MKT_MAP_PRIVATE 0x02
#ifdef __APPLE__
#define MKT_MAP_ANON 0x1000
#else
#define MKT_MAP_ANON 0x20
#endif
#define MKT_SIGABRT 6
#define mkt_stdout 1
#define mkt_stderr 2

void* mkt_mmap(void* addr, size_t len, int prot, int flags, int fd,
               off_t offset);
int mkt_munmap(void* addr, size_t len);
ssize_t mkt_write(int fildes, const void* buf, size_t nbyte);
int mkt_kill(pid_t pid, int sig);
void mkt_abort(void) { mkt_kill(0, MKT_SIGABRT); }

#define STR(s) #s

#define CHECK_NO_STDLIB(a, cond, b, fmt)                                \
    do {                                                                \
        if (!((a)cond(b))) {                                            \
            const char s[] = __FILE__ ":CHECK failed: " STR(a) " " STR( \
                cond) " " STR(b) " is false\n";                         \
            mkt_write(mkt_stderr, s, sizeof(s));                        \
            mkt_abort();                                                \
        }                                                               \
    } while (0)

// TODO: optimize
static void* mkt_alloc(size_t len) {
    void* p = mkt_mmap(NULL, len, MKT_PROT_READ | MKT_PROT_WRITE,
                       MKT_MAP_ANON | MKT_MAP_PRIVATE, -1, 0);
    CHECK_NO_STDLIB(p, !=, 0, "%p");
    return p;
}

typedef struct {
    size_t rv_size : 54;
    unsigned int rv_color : 2;
    unsigned int rv_tag : 8;
} runtime_val_header;

struct alloc_atom {
    struct alloc_atom* aa_next;
    runtime_val_header aa_header;
    char aa_data[];
};

typedef struct alloc_atom alloc_atom;
static alloc_atom* objs = NULL;  // In use

void atom_cons(alloc_atom* item, alloc_atom** head) {
    CHECK_NO_STDLIB((void*)item, !=, NULL, "%p");
    CHECK_NO_STDLIB((void*)head, !=, NULL, "%p");
    CHECK_NO_STDLIB((void*)item, !=, (void*)head, "%p");  // Prevent cycles

    if (head) {
        item->aa_next = *head;
        *head = item;
    } else {
        *head = item;
    }
    CHECK_NO_STDLIB((void*)*head, !=, NULL, "%p");
}

static alloc_atom* mkt_alloc_atom_make(size_t size) {
    const size_t bytes =
        sizeof(runtime_val_header) + sizeof(alloc_atom*) + size;
    alloc_atom* atom = mkt_alloc(bytes);
    CHECK_NO_STDLIB((void*)atom, !=, NULL, "%p");
    atom->aa_header = (runtime_val_header){0};
    atom->aa_next = NULL;

    atom_cons(atom, &objs);

    gc_allocated_bytes += bytes;

    return atom;
}

void mkt_init() {
    READ_RBP();
    stack_top = (intptr_t*)*mkt_rbp;
}

static void mkt_gc_obj_mark(runtime_val_header* header) {
    CHECK_NO_STDLIB((void*)header, !=, NULL, "%p");

    if (header->rv_tag & RV_TAG_MARKED) return;  // Prevent cycles
    header->rv_tag |= RV_TAG_MARKED;

    if (header->rv_tag & RV_TAG_STRING) return;  // No transitive refs possible

    // UNIMPLEMENTED();  // TODO: gray worklist for objects
}

// TODO: optimize
static alloc_atom* mkt_gc_atom_find_data_by_addr(size_t addr) {
    if (addr == 0) return NULL;

    alloc_atom* atom = objs;
    while (atom) {
        if (addr == (size_t)&atom->aa_data) return atom;
        atom = atom->aa_next;
    }
    return NULL;
}

static void mkt_gc_scan_stack(const intptr_t* stack_bottom) {
    CHECK_NO_STDLIB((void*)stack_bottom, !=, NULL, "%p");

    const char* s_bottom = (char*)stack_bottom;
    CHECK_NO_STDLIB((void*)stack_bottom, <=, (void*)stack_top, "%p");

    const char* s_top = (char*)stack_top;
    while (s_bottom < s_top - sizeof(intptr_t)) {
        uintptr_t addr = *(uintptr_t*)s_bottom;
        alloc_atom* atom = mkt_gc_atom_find_data_by_addr(addr);
        if (atom == NULL) {
            s_bottom += 1;
            continue;
        }
        runtime_val_header* header = &atom->aa_header;
        mkt_gc_obj_mark(header);

        s_bottom += sizeof(intptr_t);
    }
}

static void mkt_gc_obj_blacken(runtime_val_header* header) {
    CHECK_NO_STDLIB((void*)header, !=, NULL, "%p");

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
        const size_t bytes = sizeof(runtime_val_header) + sizeof(alloc_atom*) +
                             atom->aa_header.rv_size;
        CHECK_NO_STDLIB(gc_allocated_bytes, >=, (size_t)bytes, "%zu");
        gc_allocated_bytes -= bytes;

        alloc_atom* to_free = atom;
        atom = atom->aa_next;
        if (previous)
            previous->aa_next = atom;
        else
            objs = atom;

        MKT_GC_SWEEP_FREE(gc_round, gc_allocated_bytes, (void*)to_free);
        CHECK_NO_STDLIB(mkt_munmap(to_free, bytes), ==, 0, "%d");
    }
    MKT_GC_SWEEP_DONE(gc_round, gc_allocated_bytes);
}

static void mkt_gc() {
    READ_RSP();

    gc_round += 1;

    mkt_gc_scan_stack(mkt_rsp);
    mkt_gc_trace_refs();
    mkt_gc_sweep();
}

void* mkt_string_make(size_t size) {
    mkt_gc();

    alloc_atom* atom = mkt_alloc_atom_make(size);
    CHECK_NO_STDLIB((void*)atom, !=, NULL, "%p");
    atom->aa_header =
        (runtime_val_header){.rv_size = size, .rv_tag = RV_TAG_STRING};

    return &atom->aa_data;
}

void* mkt_instance_make(size_t size) {
    mkt_gc();

    alloc_atom* atom = mkt_alloc_atom_make(size);
    CHECK_NO_STDLIB((void*)atom, !=, NULL, "%p");
    atom->aa_header =
        (runtime_val_header){.rv_size = size, .rv_tag = RV_TAG_INSTANCE};

    return &atom->aa_data;
}

void mkt_bool_println(int b) {
    if (b) {
        const char s[] = "true\n";
        mkt_write(mkt_stdout, s, sizeof(s) - 1);
    } else {
        const char s[] = "false\n";
        mkt_write(mkt_stdout, s, sizeof(s) - 1);
    }
}

void mkt_char_println(char c) {
    char s[2] = {c, '\n'};
    mkt_write(mkt_stdout, s, 2);
}

static void mkt_int_to_string(long long int n, char* s, int* s_len) {
    CHECK_NO_STDLIB((void*)s_len, !=, 0, "%p");

    *s_len = 0;
    const int neg = n < 0;
    n = neg ? -n : n;

    do {
        const char rem = n % 10;
        n /= 10;

        CHECK_NO_STDLIB(*s_len, >=, 0, "%d");
        CHECK_NO_STDLIB(*s_len, <=, 22, "%d");
        s[22 - (*s_len)++] = rem + '0';
    } while (n != 0);

    if (neg) s[22 - (*s_len)++] = '-';
}

void mkt_int_println(long long int n) {
    char s[23] = "";
    int s_len = 0;
    mkt_int_to_string(n, s, &s_len);

    mkt_write(mkt_stdout, s + sizeof(s) - s_len, s_len);
    const char newline = '\n';
    mkt_write(mkt_stdout, &newline, 1);
}

void mkt_string_println(char* s, const runtime_val_header* s_header) {
    CHECK_NO_STDLIB((void*)s, !=, NULL, "%p");
    CHECK_NO_STDLIB((void*)s_header, !=, NULL, "%p");

    CHECK_NO_STDLIB(s_header->rv_tag & RV_TAG_STRING, !=, 0, "%u");

    mkt_write(mkt_stdout, s, s_header->rv_size);
    const char newline = '\n';
    mkt_write(mkt_stdout, &newline, 1);
}

char* mkt_string_concat(const char* a, const runtime_val_header* a_header,
                        const char* b, const runtime_val_header* b_header) {
    char* const ret = mkt_string_make(a_header->rv_size + b_header->rv_size);
    CHECK_NO_STDLIB((void*)ret, !=, NULL, "%p");
    CHECK_NO_STDLIB((void*)a, !=, NULL, "%p");
    CHECK_NO_STDLIB((void*)b, !=, NULL, "%p");

    for (size_t i = 0; i < a_header->rv_size; i++) ret[i] = a[i];
    for (size_t i = 0; i < b_header->rv_size; i++)
        ret[a_header->rv_size + i] = b[i];
    *(ret - 8) = a_header->rv_size + b_header->rv_size;

    return ret;
}

void mkt_instance_println(void* addr) {
    CHECK_NO_STDLIB(addr, !=, 0, "%p");

    const char s[] = "Instance of size ";
    mkt_write(mkt_stdout, s, sizeof(s) - 1);

    const runtime_val_header* const header =
        (runtime_val_header*)((uintptr_t)addr - sizeof(runtime_val_header*));
    char size_s[23] = "";
    int size_s_len = 0;
    mkt_int_to_string(header->rv_size, size_s, &size_s_len);
    mkt_write(mkt_stdout, size_s + sizeof(size_s) - size_s_len, size_s_len);

    const char newline = '\n';
    mkt_write(mkt_stdout, &newline, 1);
}
