#pragma once

#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    RES_OK,
    RES_NONE,
    RES_UNEXPECTED_TOKEN,
    RES_INVALID_SOURCE_FILE_NAME,
    RES_SOURCE_FILE_READ_FAILED,
    RES_ASM_FILE_READ_FAILED,
    RES_FAILED_AS,
    RES_FAILED_LD,
    RES_NON_MATCHING_TYPES,
    RES_EXPECTED_PRIMARY,
    RES_UNKNOWN_VAR,
    RES_ASSIGNING_VAL,
    RES_NEED_AT_LEAST_ONE_STMT,
    RES_MISSING_PARAM,
    RES_ERR,
} res_t;

static const char res_to_str[][100] = {
    [RES_OK] = "ok\n",
    [RES_NONE] = "none\n",
    [RES_INVALID_SOURCE_FILE_NAME] = "Invalid source file name %s\n",
    [RES_SOURCE_FILE_READ_FAILED] = "Failed to read source file %s: %s\n",
    [RES_ASM_FILE_READ_FAILED] = "Failed to read asm file %s: %s\n",
    [RES_FAILED_AS] = "Failed to run `%s`: %s\n",
    [RES_FAILED_LD] = "Failed to run `%s`: %s\n",
};

#define STR(s) #s

#define CHECK(a, cond, b, fmt)                                    \
    do {                                                          \
        if (!((a)cond(b))) {                                      \
            fprintf(stderr,                                       \
                    __FILE__ ":%d:CHECK failed: " fmt             \
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

#define UNREACHABLE()                                            \
    do {                                                         \
        fprintf(stderr, __FILE__ ":%d:unreachable\n", __LINE__); \
        exit(1);                                                 \
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
        for (int i = 0; i < indent; i++) fprintf(stderr, " ");               \
        fprintf(stderr, fmt "\n", __VA_ARGS__);                              \
    } while (0)
#endif

static const char color_red[] = "\x1b[31m";
static const char color_reset[] = "\x1b[0m";
static const char color_grey[] = "\x1b[37m";

static bool is_space(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

static void trim_end(const char** string, int* string_len) {
    CHECK((void*)string, !=, NULL, "%p");
    CHECK((void*)string_len, !=, NULL, "%p");

    while (*string_len > 0 && is_space((*string)[*string_len - 1]))
        *string_len -= 1;
}
