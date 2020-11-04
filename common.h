#pragma once

#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long long int usize;
typedef signed long long int isize;
typedef char u8;
typedef unsigned short u16;

typedef enum {
    RES_OK,
    RES_NONE,
    RES_UNEXPECTED_TOKEN,
    RES_INVALID_SOURCE_FILE_NAME,
    RES_SOURCE_FILE_READ_FAILED,
    RES_ASM_FILE_READ_FAILED,
    RES_FAILED_AS,
    RES_FAILED_LD,
} res_t;

static const u8 res_to_str[][100] = {
    [RES_OK] = "ok\n",
    [RES_NONE] = "none\n",
    [RES_UNEXPECTED_TOKEN] =
        "%s%s:%llu:%llu:%sUnexpected token. Expected `%s`, got `%s`\n",
    [RES_INVALID_SOURCE_FILE_NAME] = "Invalid source file name %s\n",
    [RES_SOURCE_FILE_READ_FAILED] = "Failed to read source file %s: %s\n",
    [RES_ASM_FILE_READ_FAILED] = "Failed to read asm file %s: %s\n",
    [RES_FAILED_AS] = "Failed to run `%s`: %s\n",
    [RES_FAILED_LD] = "Failed to run `%s`: %s\n",
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

#ifndef WITH_LOGS
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
#define log_debug(fmt, ...)                                                \
    do {                                                                   \
        fprintf(stderr, "[debug] %s:%s:%d: " fmt "\n", __FILE__, __func__, \
                __LINE__, __VA_ARGS__);                                    \
    } while (0)
#define log_debug_with_indent(indent, fmt, ...)                              \
    do {                                                                     \
        fprintf(stderr, "[debug] %s:%s:%d: ", __FILE__, __func__, __LINE__); \
        for (usize i = 0; i < indent; i++) fprintf(stderr, " ");             \
        fprintf(stderr, fmt "\n", __VA_ARGS__);                              \
    } while (0)
#endif

#define UNREACHABLE() (assert(0 && "Unreachable"))

static const u8 color_red[] = "\x1b[31m";
static const u8 color_reset[] = "\x1b[0m";
static const u8 color_grey[] = "\x1b[90m";
