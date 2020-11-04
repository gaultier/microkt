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
    int obj_type_i, obj_tok_i;
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
        token_index_t node_string;               // NODE_STRING
        token_index_t node_i64;                  // NODE_I64, NODE_CHAR
    } node_n;
};

static void ast_node_dump(const ast_node_t* nodes, token_index_t node_i,
                          int indent) {
    PG_ASSERT_COND((void*)nodes, !=, NULL, "%p");

    const ast_node_t* node = &nodes[node_i];
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT: {
            log_debug_with_indent(indent, "ast_node %s",
                                  ast_node_kind_t_to_str[node->node_kind]);
            ast_node_dump(nodes, node->node_n.node_builtin_print.bp_arg_i, 2);
            break;
        }
        case NODE_KEYWORD_BOOL:
        case NODE_I64:
        case NODE_CHAR:
        case NODE_STRING: {
            log_debug_with_indent(indent, "ast_node %s",
                                  ast_node_kind_t_to_str[node->node_kind]);
            break;
        }
    }
}

static token_index_t ast_node_first_token(const ast_node_t* node) {
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT:
            return node->node_n.node_builtin_print.bp_keyword_print_i;
        case NODE_KEYWORD_BOOL:
            return node->node_n.node_boolean;
        case NODE_STRING:
            return node->node_n.node_string;
        case NODE_CHAR:
        case NODE_I64:
            return node->node_n.node_i64;
    }
}

static token_index_t ast_node_last_token(const ast_node_t* node) {
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT:
            return node->node_n.node_builtin_print.bp_rparen_i;
        case NODE_KEYWORD_BOOL:
            return node->node_n.node_boolean;
        case NODE_STRING:
            return node->node_n.node_string;
        case NODE_I64:
        case NODE_CHAR:
            return node->node_n.node_i64;
    }
}

#define NODE_PRINT(arg, keyword, rparen, type_idx)                       \
    ((ast_node_t){                                                       \
        .node_kind = NODE_BUILTIN_PRINT,                                 \
        .node_type_idx = type_idx,                                       \
        .node_n = {.node_builtin_print = {.bp_arg_i = arg,               \
                                          .bp_keyword_print_i = keyword, \
                                          .bp_rparen_i = rparen}}})
#define NODE_I64(n, type_idx)                \
    ((ast_node_t){.node_kind = NODE_I64,     \
                  .node_type_idx = type_idx, \
                  .node_n = {.node_i64 = n}})

#define NODE_CHAR(n, type_idx)               \
    ((ast_node_t){.node_kind = NODE_CHAR,    \
                  .node_type_idx = type_idx, \
                  .node_n = {.node_i64 = n}})

#define NODE_BOOL(n, type_idx)                    \
    ((ast_node_t){.node_kind = NODE_KEYWORD_BOOL, \
                  .node_type_idx = type_idx,      \
                  .node_n = {.node_boolean = n}})

#define NODE_STRING(n, type_idx)             \
    ((ast_node_t){.node_kind = NODE_STRING,  \
                  .node_type_idx = type_idx, \
                  .node_n = {.node_string = n}})

#define AS_PRINT(node) ((node).node_n.node_builtin_print)

