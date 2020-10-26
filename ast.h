#pragma once

#include "common.h"

struct ast_node_t;
typedef struct ast_node_t ast_node_t;

typedef struct {
    ast_node_t* arg;
} ast_builtin_print_t;

typedef enum {
    NODE_BUILTIN_PRINT,
    NODE_KEYWORD_BOOL,
} ast_node_kind_t;

const u8 ast_node_kind_t_to_str[][30] = {
    [NODE_BUILTIN_PRINT] = "print",
    [NODE_KEYWORD_BOOL] = "bool",
};

struct ast_node_t {
    ast_node_kind_t node_kind;
    union {
        ast_builtin_print_t node_builtin_print;
        int node_boolean;
    } node_n;
};

void ast_node_dump(const ast_node_t* node, usize indent) {
    PG_ASSERT_COND(node, !=, NULL, "%p");

    printf("[debug] ");
    for (usize i = 0; i < indent; i++) printf(" ");

    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT: {
            printf("ast_node %s\n", ast_node_kind_t_to_str[node->node_kind]);
            ast_node_dump(node->node_n.node_builtin_print.arg, 2);
            break;
        }
        case NODE_KEYWORD_BOOL: {
            printf("ast_node %s %s\n", ast_node_kind_t_to_str[node->node_kind],
                   (node->node_n.node_boolean == 0) ? "false" : "true");
            break;
        }
    }
}
