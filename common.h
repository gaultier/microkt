#pragma once

#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long long int usize;
typedef char u8;
typedef unsigned short u16;

typedef enum {
    RES_ERR,
    RES_OK,
    RES_NONE,
    RES_UNEXPECTED_TOKEN,
    RES_INVALID_SOURCE_FILE_NAME,
    RES_SOURCE_FILE_READ_FAILED,
} res_t;

static const u8 res_to_str[][100] = {
    [RES_ERR] = "error",
    [RES_OK] = "ok",
    [RES_NONE] = "none",
    [RES_UNEXPECTED_TOKEN] = "Unexpected token: expected %s, got %s",
    [RES_INVALID_SOURCE_FILE_NAME] = "Invalid source file name %s",
    [RES_SOURCE_FILE_READ_FAILED] = "Failed to read source file %s",
};

// On macos this macro is defined in some system headers
#ifndef MIN
#define MIN(a, b) (a) < (b) ? (a) : (b)
#endif

#define STR(s) #s

#define PG_ASSERT_COND(a, cond, b, fmt)                           \
    do {                                                          \
        if (!((a)cond(b))) {                                      \
            fprintf(stderr,                                       \
                    __FILE__ ":%d:PG_ASSERT_COND failed: " fmt    \
                             " " STR(cond) " " fmt " is false\n", \
                    __LINE__, a, b);                              \
            assert(0);                                            \
        }                                                         \
    } while (0)

#define UNIMPLEMENTED()                                            \
    do {                                                           \
        fprintf(stderr, __FILE__ ":%d:unimplemented\n", __LINE__); \
        exit(1);                                                   \
    } while (0)

#define IGNORE(x)  \
    do {           \
        (void)(x); \
    } while (0)

#ifdef NDEBUG
#define log_debug(fmt, ...) \
    do {                    \
        IGNORE(fmt);        \
    } while (0)
#define log_debug_with_indent(indent, fmt, ...) \
    do {                                        \
        IGNORE(indent);                         \
        IGNORE(fmt);                            \
    } while (0)
#else
#define log_debug(fmt, ...)                                             \
    do {                                                                \
        fprintf(stderr, "[debug] %s:%d: " fmt "\n", __func__, __LINE__, \
                __VA_ARGS__);                                           \
    } while (0)
#define log_debug_with_indent(indent, fmt, ...)                  \
    do {                                                         \
        fprintf(stderr, "[debug] %s:%d: ", __func__, __LINE__);  \
        for (usize i = 0; i < indent; i++) fprintf(stderr, " "); \
        fprintf(stderr, fmt "\n", __VA_ARGS__);                  \
    } while (0)
#endif

#define UNREACHABLE() (assert(0 && "Unreachable"))
