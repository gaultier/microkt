#pragma once

#include "ast.h"
#include "buf.h"
#include "lex.h"

typedef struct {
    token_id_t* par_token_ids;
    const u8* par_source;
    const usize par_source_len;
    const u8* par_file_name0;
    usize par_tok_i;
    ast_node_t* par_nodes;
} parser_t;

typedef usize token_index_t;

parser_t parser_init(const u8* file_name0, const u8* source, usize source_len) {
    PG_ASSERT_COND(file_name0, !=, NULL, "%p");
    PG_ASSERT_COND(source, !=, NULL, "%p");

    lexer_t lexer = lex_init(source, source_len);

    token_id_t* token_ids = NULL;
    buf_grow(token_ids, source_len / 8);
    PG_ASSERT_COND(token_ids, !=, NULL, "%p");

    while (1) {
        const token_t token = lex_next(&lexer);
        token_dump(&token);

        buf_push(token_ids, token.tok_id);

        if (token.tok_id == LEX_TOKEN_ID_EOF) break;
    }

    return (parser_t){.par_file_name0 = file_name0,
                      .par_source = source,
                      .par_source_len = source_len,
                      .par_token_ids = token_ids,
                      .par_nodes = NULL,
                      .par_tok_i = 0};
}

token_index_t parser_next_token(parser_t* parser) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    // TODO: skip comments

    const token_index_t res = parser->par_tok_i;
    parser->par_tok_i += 1;

    return res;
}

res_t parser_eat_token(parser_t* parser, token_id_t id,
                       token_index_t* return_token_index) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");
    PG_ASSERT_COND(return_token_index, !=, NULL, "%p");

    if (parser->par_token_ids[parser->par_tok_i] == id) {
        *return_token_index = parser_next_token(parser);
        return RES_OK;
    } else
        return RES_NONE;
}

res_t parser_parse_primary(parser_t* parser, usize* new_primary_node_i) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(new_primary_node_i, !=, NULL, "%p");

    token_index_t token = 0;
    if (parser_eat_token(parser, LEX_TOKEN_ID_TRUE, &token) == RES_OK) {
        const ast_node_t new_node = {.node_kind = NODE_KEYWORD_BOOL,
                                     .node_n = {.node_boolean = 1}};
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_eat_token(parser, LEX_TOKEN_ID_FALSE, &token) == RES_OK) {
        const ast_node_t new_node = {.node_kind = NODE_KEYWORD_BOOL,
                                     .node_n = {.node_boolean = 0}};
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return RES_NONE;
}

res_t parser_expect_token(parser_t* parser, token_id_t id) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");

    const token_index_t token = parser_next_token(parser);
    if (parser->par_token_ids[token] != id) {
        // TODO: errors
        return RES_ERR;
    }
    return RES_OK;
}

res_t parser_parse_builtin_print(parser_t* parser, usize* new_node_i) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(new_node_i, !=, NULL, "%p");

    token_index_t token = 0;
    if (parser_eat_token(parser, LEX_TOKEN_ID_BUILTIN_PRINT, &token) ==
        RES_OK) {
        if (parser_expect_token(parser, LEX_TOKEN_ID_LPAREN) != RES_OK)
            return RES_ERR;

        usize primary_node_i = 0;
        if (parser_parse_primary(parser, &primary_node_i) != RES_OK)
            return RES_ERR;

        if (parser_expect_token(parser, LEX_TOKEN_ID_RPAREN) != RES_OK)
            return RES_ERR;

        const ast_node_t new_node = {
            .node_kind = NODE_BUILTIN_PRINT,
            .node_n = {.node_builtin_print = {.bp_arg_i = primary_node_i}}};
        buf_push(parser->par_nodes, new_node);
        *new_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    return RES_NONE;
}

res_t parser_parse(parser_t* parser) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    while (1) {
        usize new_node_i = 0;
        if (parser_parse_builtin_print(parser, &new_node_i) == RES_OK) {
            ast_node_dump(parser->par_nodes, new_node_i, 0);

            continue;
        }

        const token_index_t next = parser->par_token_ids[parser->par_tok_i];

        if (next == LEX_TOKEN_ID_EOF)
            return RES_OK;
        else {
            // TODO: errors
            return RES_ERR;
        }
    }
}
