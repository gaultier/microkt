#pragma once

#include "common.h"
#include "lex.h"

typedef usize token_index_t;

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
} ast_node_kind_t;

const u8 ast_node_kind_t_to_str[][30] = {
    [NODE_BUILTIN_PRINT] = "print",
    [NODE_KEYWORD_BOOL] = "bool",
};

struct ast_node_t {
    ast_node_kind_t node_kind;
    union {
        ast_builtin_print_t node_builtin_print;
        token_index_t node_boolean;
    } node_n;
};

void ast_node_dump(const ast_node_t* nodes, token_index_t node_i,
                   usize indent) {
    PG_ASSERT_COND(nodes, !=, NULL, "%p");

    fprintf(stderr, "[debug] ");
    for (usize i = 0; i < indent; i++) printf(" ");

    const ast_node_t* node = &nodes[node_i];
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT: {
            fprintf(stderr, "ast_node %s\n",
                    ast_node_kind_t_to_str[node->node_kind]);
            ast_node_dump(nodes, node->node_n.node_builtin_print.bp_arg_i, 2);
            break;
        }
        case NODE_KEYWORD_BOOL: {
            fprintf(stderr, "ast_node %s\n",
                    ast_node_kind_t_to_str[node->node_kind]);
            break;
        }
    }
}

token_index_t ast_node_first_token(const ast_node_t* node) {
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT:
            return node->node_n.node_builtin_print.bp_keyword_print_i;
        case NODE_KEYWORD_BOOL:
            return node->node_n.node_boolean;
    }
}

token_index_t ast_node_last_token(const ast_node_t* node) {
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT:
            return node->node_n.node_builtin_print.bp_rparen_i;
        case NODE_KEYWORD_BOOL:
            return node->node_n.node_boolean;
    }
}
