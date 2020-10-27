#pragma once

#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long long int usize;
typedef char u8;
typedef unsigned short u16;

typedef u8 res_t;
#define RES_OK ((res_t)0)
#define RES_ERR ((res_t)1)
#define RES_NONE ((res_t)2)

#define MIN(a, b) (a) < (b) ? (a) : (b)

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

#define log_debug(fmt, ...)                                          \
    do {                                                             \
        fprintf(stderr, "\n[debug] %s:%d: " fmt, __func__, __LINE__, \
                __VA_ARGS__);                                        \
    } while (0)

#define log_debug_with_indent(indent, fmt, ...)                   \
    do {                                                          \
        fprintf(stderr, "\n[debug] %s:%d: ", __func__, __LINE__); \
        for (usize i = 0; i < indent; i++) fprintf(stderr, " ");  \
        fprintf(stderr, fmt, __VA_ARGS__);                        \
    } while (0)
