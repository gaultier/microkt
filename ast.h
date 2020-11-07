#pragma once

#include <stdint.h>

#include "common.h"

typedef enum {
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_I64,
    TYPE_STRING,
    TYPE_BUILTIN_PRINTLN,
} type_kind_t;

static const char type_to_str[][20] = {
    [TYPE_BOOL] = "Bool",
    [TYPE_CHAR] = "Char",
    [TYPE_I64] = "Int64",
    [TYPE_STRING] = "String",
    [TYPE_BUILTIN_PRINTLN] = "BuiltinPrint",
};

typedef struct {
    int ty_size;
    type_kind_t ty_kind;
} type_t;

typedef struct {
    const char* gl_source;
    int gl_source_len;
} global_var_t;

typedef enum { OBJ_GLOBAL_VAR } obj_kind_t;

typedef struct {
    int obj_type_i, obj_tok_i;
    obj_kind_t obj_kind;
    union {
        global_var_t obj_global_var;  // OBJ_GLOBAL_VAR
    } obj;
} obj_t;

struct ast_node_t;

typedef struct ast_node_t ast_node_t;

typedef struct {
    int bp_arg_i;  // Index of argument node
    int bp_keyword_print_i;
    int bp_rparen_i;
} ast_builtin_println_t;

typedef struct {
    int bi_type_i;
    int bi_lhs_i;
    int bi_rhs_i;
} binary_t;

typedef enum {
    NODE_BUILTIN_PRINTLN,
    NODE_KEYWORD_BOOL,
    NODE_STRING,
    NODE_I64,
    NODE_CHAR,
    NODE_ADD,
    NODE_SUBTRACT,
    NODE_MULTIPLY,
    NODE_DIVIDE,
} ast_node_kind_t;

const char ast_node_kind_t_to_str[][30] = {
    [NODE_BUILTIN_PRINTLN] = "print",
    [NODE_KEYWORD_BOOL] = "bool",
    [NODE_STRING] = "String",
    [NODE_I64] = "I64",
    [NODE_CHAR] = "Char",
    [NODE_ADD] = "Plus",
    [NODE_SUBTRACT] = "Subtract",
    [NODE_MULTIPLY] = "Multiply",
    [NODE_DIVIDE] = "Divide",
};

typedef struct {
    int64_t nu_val;
    int nu_tok_i;
} node_number_t;

struct ast_node_t {
    ast_node_kind_t node_kind;
    int node_type_i;
    union {
        ast_builtin_println_t node_builtin_println;  // NODE_BUILTIN_PRINTLN
        int node_boolean;                            // NODE_KEYWORD_BOOL
        int node_string;                             // NODE_STRING, int = obj_i
        node_number_t node_num;                      // NODE_I64, NODE_CHAR
        binary_t
            node_binary;  // NODE_ADD, NODE_SUBTRACT, NODE_MULTIPLY, NODE_DIVIDE
    } node_n;
};

#define NODE_PRINTLN(arg, keyword, rparen, type_i)                         \
    ((ast_node_t){                                                         \
        .node_kind = NODE_BUILTIN_PRINTLN,                                 \
        .node_type_i = type_i,                                             \
        .node_n = {.node_builtin_println = {.bp_arg_i = arg,               \
                                            .bp_keyword_print_i = keyword, \
                                            .bp_rparen_i = rparen}}})
#define NODE_I64(tok_i, type_i, val)                                        \
    ((ast_node_t){.node_kind = NODE_I64,                                    \
                  .node_type_i = type_i,                                    \
                  .node_n = {.node_num = (node_number_t){.nu_tok_i = tok_i, \
                                                         .nu_val = val}}})

#define NODE_CHAR(tok_i, type_i, val)                                       \
    ((ast_node_t){.node_kind = NODE_CHAR,                                   \
                  .node_type_i = type_i,                                    \
                  .node_n = {.node_num = (node_number_t){.nu_tok_i = tok_i, \
                                                         .nu_val = val}}})

#define NODE_BOOL(n, type_i)                      \
    ((ast_node_t){.node_kind = NODE_KEYWORD_BOOL, \
                  .node_type_i = type_i,          \
                  .node_n = {.node_boolean = n}})

#define NODE_STRING(n, type_i)              \
    ((ast_node_t){.node_kind = NODE_STRING, \
                  .node_type_i = type_i,    \
                  .node_n = {.node_string = n}})

#define NODE_ADD(lhs_i, rhs_i, type_i)                                       \
    ((ast_node_t){.node_kind = NODE_ADD,                                     \
                  .node_type_i = type_i,                                     \
                  .node_n = {.node_binary = ((binary_t){.bi_type_i = type_i, \
                                                        .bi_lhs_i = lhs_i,   \
                                                        .bi_rhs_i = rhs_i})}})

#define NODE_SUBTRACT(lhs_i, rhs_i, type_i)                                  \
    ((ast_node_t){.node_kind = NODE_SUBTRACT,                                \
                  .node_type_i = type_i,                                     \
                  .node_n = {.node_binary = ((binary_t){.bi_type_i = type_i, \
                                                        .bi_lhs_i = lhs_i,   \
                                                        .bi_rhs_i = rhs_i})}})

#define NODE_MULTIPLY(lhs_i, rhs_i, type_i)                                  \
    ((ast_node_t){.node_kind = NODE_MULTIPLY,                                \
                  .node_type_i = type_i,                                     \
                  .node_n = {.node_binary = ((binary_t){.bi_type_i = type_i, \
                                                        .bi_lhs_i = lhs_i,   \
                                                        .bi_rhs_i = rhs_i})}})

#define NODE_DIVIDE(lhs_i, rhs_i, type_i)                                    \
    ((ast_node_t){.node_kind = NODE_DIVIDE,                                  \
                  .node_type_i = type_i,                                     \
                  .node_n = {.node_binary = ((binary_t){.bi_type_i = type_i, \
                                                        .bi_lhs_i = lhs_i,   \
                                                        .bi_rhs_i = rhs_i})}})

#define AS_BINARY(node) ((node).node_n.node_binary)

#define AS_PRINTLN(node) ((node).node_n.node_builtin_println)

#define OBJ_GLOBAL_VAR(type_i, tok_i, source, source_len) \
    ((obj_t){.obj_kind = OBJ_GLOBAL_VAR,                  \
             .obj_type_i = type_i,                        \
             .obj_tok_i = tok_i,                          \
             .obj = {.obj_global_var = (global_var_t){    \
                         .gl_source = source, .gl_source_len = source_len}}})

