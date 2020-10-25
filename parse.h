#pragma once

#include "ast.h"
#include "lex.h"

typedef struct {
    token_id_t* par_token_ids;
    usize par_token_ids_len;
    const u8* par_source;
    const usize par_source_len;
    const u8* par_file_name0;
    usize par_tok_i;
} parser_t;

typedef usize token_index_t;

parser_t parser_init(const u8* file_name0, const u8* source, usize source_len) {
    PG_ASSERT_COND(file_name0, !=, NULL, "%p");
    PG_ASSERT_COND(source, !=, NULL, "%p");

    lex_t lex = lex_init(source, source_len);

    usize token_ids_len = 0;
    usize token_ids_capacity = source_len / 8;  // Estimate
    token_id_t* token_ids =
        realloc(NULL, token_ids_capacity * sizeof(token_id_t));
    PG_ASSERT_COND(token_ids, !=, NULL, "%p");

    while (1) {
        const token_t token = lex_next(&lex);
        token_dump(&token);

        PG_ASSERT_COND(token_ids_len, <=, token_ids_capacity, "%llu");
        if (token_ids_len == token_ids_capacity) {
            token_ids_capacity = token_ids_capacity * 2 + 1;
            token_ids =
                realloc(token_ids, token_ids_capacity * sizeof(token_id_t));
            PG_ASSERT_COND(token_ids, !=, NULL, "%p");
        }
        token_ids[token_ids_len++] = token.tok_id;

        if (token.tok_id == LEX_TOKEN_ID_EOF) break;
    }

    return (parser_t){.par_file_name0 = file_name0,
                      .par_source = source,
                      .par_source_len = source_len,
                      .par_token_ids = token_ids,
                      .par_token_ids_len = token_ids_len,
                      .par_tok_i = 0};
}

token_index_t parser_next_token(parser_t* parser) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids_len, >, (usize)0, "%llu");
    PG_ASSERT_COND(parser->par_token_ids_len, >, parser->par_tok_i, "%llu");

    // TODO: skip comments

    const token_index_t res = parser->par_tok_i;
    parser->par_tok_i += 1;

    return res;
}

res_t parser_eat_token(parser_t* parser, token_id_t id,
                       token_index_t* return_token_index) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids_len, >, (usize)0, "%llu");
    PG_ASSERT_COND(parser->par_token_ids_len, >, parser->par_tok_i, "%llu");
    PG_ASSERT_COND(return_token_index, !=, NULL, "%p");

    if (parser->par_token_ids[parser->par_tok_i] == id) {
        *return_token_index = parser_next_token(parser);
        return RES_OK;
    } else
        return RES_NONE;
}

ast_node_t* parser_create_literal(parser_t* parser, ast_node_kind_t kind) {
    (void)parser;  // TODO: arena

    ast_node_t* node = realloc(NULL, sizeof(ast_node_t));
    PG_ASSERT_COND(node, !=, NULL, "%p");

    node->node_kind = kind;

    return node;
}

res_t parser_parse_primary(parser_t* parser, ast_node_t** return_node) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");

    token_index_t token = 0;
    if (parser_eat_token(parser, LEX_TOKEN_ID_TRUE, &token) == RES_OK) {
        *return_node = parser_create_literal(parser, NODE_KEYWORD_BOOL);
        (*return_node)->node_n.node_boolean = 1;
        return RES_OK;
    }
    if (parser_eat_token(parser, LEX_TOKEN_ID_FALSE, &token) == RES_OK) {
        *return_node = parser_create_literal(parser, NODE_KEYWORD_BOOL);
        (*return_node)->node_n.node_boolean = 0;
        return RES_OK;
    }

    return RES_NONE;
}

res_t parser_parse(parser_t* parser, ast_node_t*** nodes, usize* nodes_len) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids_len, >, (usize)0, "%llu");
    PG_ASSERT_COND(parser->par_token_ids_len, >, parser->par_tok_i, "%llu");
    PG_ASSERT_COND(nodes, !=, NULL, "%p");
    PG_ASSERT_COND(nodes_len, !=, NULL, "%p");

    usize nodes_capacity = 0;

    while (1) {
        ast_node_t* node = NULL;
        if (parser_parse_primary(parser, &node) == RES_OK) {
            PG_ASSERT_COND(*nodes_len, <=, nodes_capacity, "%llu");
            if (*nodes_len == nodes_capacity) {
                nodes_capacity = nodes_capacity * 2 + 1;
                *nodes = realloc(*nodes, nodes_capacity * sizeof(ast_node_t));
                PG_ASSERT_COND(*nodes, !=, NULL, "%p");
            }
            (*nodes)[(*nodes_len)++] = node;
        }

        const token_index_t next = parser->par_token_ids[parser->par_tok_i];

        if (next == LEX_TOKEN_ID_EOF)
            break;
        else {
            // TODO: errors
            return RES_ERR;
        }
    }
    return RES_OK;
}
