#pragma once

#include <stdint.h>

#include "common.h"

typedef enum {
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_I64,
    TYPE_STRING,
    TYPE_BUILTIN_PRINT,
} type_kind_t;

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
typedef int token_index_t;

struct ast_node_t;

typedef struct ast_node_t ast_node_t;

typedef struct {
    token_index_t bp_arg_i;  // Index of argument node
    token_index_t bp_keyword_print_i;
    token_index_t bp_rparen_i;
} ast_builtin_print_t;

typedef enum {
    NODE_BUILTIN_PRINT,
    NODE_KEYWORD_BOOL,
    NODE_STRING,
    NODE_I64,
    NODE_CHAR,
} ast_node_kind_t;

const char ast_node_kind_t_to_str[][30] = {
    [NODE_BUILTIN_PRINT] = "print", [NODE_KEYWORD_BOOL] = "bool",
    [NODE_STRING] = "String",       [NODE_I64] = "I64",
    [NODE_CHAR] = "Char",
};

struct ast_node_t {
    ast_node_kind_t node_kind;
    int node_type_idx;
    union {
        ast_builtin_print_t node_builtin_print;  // NODE_BUILTIN_PRINT
        token_index_t node_boolean;              // NODE_KEYWORD_BOOL
        int node_string;                         // NODE_STRING, int = obj_i
        token_index_t node_i64;                  // NODE_I64, NODE_CHAR
    } node_n;
};

