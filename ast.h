#pragma once

struct ast_node;
typedef struct ast_node ast_node_t;

typedef struct {
    ast_node_t* arg;
} ast_builtin_print_t;

typedef enum {
    NODE_BUILTIN_PRINT,
    NODE_KEYWORD_BOOL,
} ast_node_kind_t;

struct ast_node {
    ast_node_kind_t node_kind;
    union {
        ast_builtin_print_t node_builtin_print;
        int node_boolean;
    } node_n;
};
