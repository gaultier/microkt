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
    int bp_arg_i, bp_keyword_print_i, bp_rparen_i;
} ast_builtin_println_t;

typedef struct {
    int bi_type_i, bi_lhs_i, bi_rhs_i;
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
    NODE_MODULO,
    NODE_LT,
    NODE_LE,
    NODE_EQ,
    NODE_NEQ,
    NODE_NOT,
    NODE_IF,
} ast_node_kind_t;

const char ast_node_kind_t_to_str[][30] = {
    [NODE_BUILTIN_PRINTLN] = "Print",
    [NODE_KEYWORD_BOOL] = "Bool",
    [NODE_STRING] = "String",
    [NODE_I64] = "I64",
    [NODE_CHAR] = "Char",
    [NODE_ADD] = "Plus",
    [NODE_SUBTRACT] = "Subtract",
    [NODE_MULTIPLY] = "Multiply",
    [NODE_MODULO] = "Modulo",
    [NODE_DIVIDE] = "Divide",
    [NODE_LT] = "Lt",
    [NODE_LE] = "Le",
    [NODE_EQ] = "Eq",
    [NODE_NEQ] = "Neq",
    [NODE_NOT] = "Not",
    [NODE_IF] = "If",
};

typedef struct {
    long long int nu_val;
    int nu_tok_i;
} node_number_t;

typedef struct {
    int if_first_tok_i, if_last_tok_i, if_node_cond_i, if_node_then_i,
        if_node_else_i;
} if_t;

struct ast_node_t {
    ast_node_kind_t node_kind;
    int node_type_i;
    union {
        ast_builtin_println_t node_builtin_println;  // NODE_BUILTIN_PRINTLN
        int node_string;                             // NODE_STRING, int = obj_i
        node_number_t node_num;  // NODE_I64, NODE_CHAR, NODE_BOOL
        binary_t node_binary;    // NODE_ADD, NODE_SUBTRACT, NODE_MULTIPLY,
        // NODE_DIVIDE, NODE_MODULO
        int node_unary;  // NODE_NOT, int = node_i
        if_t node_if;    // NODE_IF
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

#define NODE_BINARY(kind, lhs_i, rhs_i, type_i)                              \
    ((ast_node_t){.node_kind = kind,                                         \
                  .node_type_i = type_i,                                     \
                  .node_n = {.node_binary = ((binary_t){.bi_type_i = type_i, \
                                                        .bi_lhs_i = lhs_i,   \
                                                        .bi_rhs_i = rhs_i})}})

#define NODE_UNARY(kind, type_i, lhs_i)  \
    ((ast_node_t){.node_kind = kind,     \
                  .node_type_i = type_i, \
                  .node_n = {.node_unary = lhs_i}})

#define NODE_IF(type_i, first_tok_i, last_tok_i, node_cond_i, node_then_i, \
                node_else_i)                                               \
    ((ast_node_t){                                                         \
        .node_kind = NODE_IF,                                              \
        .node_type_i = type_i,                                             \
        .node_n = {.node_if = ((if_t){.if_first_tok_i = first_tok_i,       \
                                      .if_last_tok_i = last_tok_i,         \
                                      .if_node_cond_i = node_cond_i,       \
                                      .if_node_then_i = node_then_i,       \
                                      .if_node_else_i = node_else_i})}})

#define AS_BINARY(node) ((node).node_n.node_binary)

#define AS_PRINTLN(node) ((node).node_n.node_builtin_println)

#define OBJ_GLOBAL_VAR(type_i, tok_i, source, source_len) \
    ((obj_t){.obj_kind = OBJ_GLOBAL_VAR,                  \
             .obj_type_i = type_i,                        \
             .obj_tok_i = tok_i,                          \
             .obj = {.obj_global_var = (global_var_t){    \
                         .gl_source = source, .gl_source_len = source_len}}})

