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
    ast_node_t* par_nodes;          // Arena of all nodes
    token_index_t* par_stmt_nodes;  // Array of statements. Each statement is
                                    // stored as the node index which is the
                                    // root of the statement in the ast
    loc_t* par_token_locs;
} parser_t;

static parser_t parser_init(const u8* file_name0, const u8* source,
                            usize source_len) {
    PG_ASSERT_COND((void*)file_name0, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");

    lexer_t lexer = lex_init(source, source_len);

    token_id_t* token_ids = NULL;
    buf_grow(token_ids, source_len / 8);

    loc_t* token_locs = NULL;
    buf_grow(token_locs, source_len / 8);

    while (1) {
        const token_t token = lex_next(&lexer);
        token_dump(&token);

        buf_push(token_ids, token.tok_id);
        buf_push(token_locs, token.tok_loc);

        if (token.tok_id == LEX_TOKEN_ID_EOF) break;
    }

    return (parser_t){.par_file_name0 = file_name0,
                      .par_source = source,
                      .par_source_len = source_len,
                      .par_token_ids = token_ids,
                      .par_nodes = NULL,
                      .par_stmt_nodes = NULL,
                      .par_token_locs = token_locs,
                      .par_tok_i = 0};
}

static void parser_ast_node_source(const parser_t* parser,
                                   const ast_node_t* node, const u8** source,
                                   usize* source_len) {
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source_len, !=, NULL, "%p");

    const loc_t first_token =
        parser->par_token_locs[ast_node_first_token(node)];
    const loc_t last_token = parser->par_token_locs[ast_node_last_token(node)];

    *source = &parser->par_source[(node->node_kind == NODE_STRING_LITERAL)
                                      ? first_token.loc_start + 1
                                      : first_token.loc_start];
    *source_len = (node->node_kind == NODE_STRING_LITERAL)
                      ? (last_token.loc_end - first_token.loc_start - 2)
                      : (last_token.loc_end - first_token.loc_start);
}

static token_index_t parser_next_token(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    // TODO: skip comments

    const token_index_t res = parser->par_tok_i;
    parser->par_tok_i += 1;

    return res;
}

static res_t parser_eat_token(parser_t* parser, token_id_t id,
                              token_index_t* return_token_index) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");
    PG_ASSERT_COND((void*)return_token_index, !=, NULL, "%p");

    if (parser->par_token_ids[parser->par_tok_i] == id) {
        *return_token_index = parser_next_token(parser);
        return RES_OK;
    } else
        return RES_NONE;
}

static res_t parser_parse_primary(parser_t* parser, usize* new_primary_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_primary_node_i, !=, NULL, "%p");

    token_index_t token = 0;
    if (parser_eat_token(parser, LEX_TOKEN_ID_TRUE, &token) == RES_OK) {
        const ast_node_t new_node = {.node_kind = NODE_KEYWORD_BOOL,
                                     .node_n = {.node_boolean = token}};
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_eat_token(parser, LEX_TOKEN_ID_FALSE, &token) == RES_OK) {
        const ast_node_t new_node = {.node_kind = NODE_KEYWORD_BOOL,
                                     .node_n = {.node_boolean = token}};
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_eat_token(parser, LEX_TOKEN_ID_STRING_LITERAL, &token) ==
        RES_OK) {
        const ast_node_t new_node = {.node_kind = NODE_STRING_LITERAL,
                                     .node_n = {.node_string_literal = token}};
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_eat_token(parser, LEX_TOKEN_ID_INT_LITERAL, &token) == RES_OK) {
        const ast_node_t new_node = {.node_kind = NODE_INT_LITERAL,
                                     .node_n = {.node_int_literal = token}};
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return RES_NONE;
}

static res_t parser_expect_token(parser_t* parser, token_id_t id,
                                 token_index_t* token) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((void*)token, !=, NULL, "%p");

    const token_index_t tok = parser_next_token(parser);
    if (parser->par_token_ids[tok] != id) {
        // TODO: errors
        return RES_ERR;
    }
    *token = tok;
    return RES_OK;
}

static res_t parser_parse_builtin_print(parser_t* parser, usize* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    token_index_t keyword_print = 0;
    if (parser_eat_token(parser, LEX_TOKEN_ID_BUILTIN_PRINT, &keyword_print) ==
        RES_OK) {
        token_index_t lparen = 0;
        if (parser_expect_token(parser, LEX_TOKEN_ID_LPAREN, &lparen) != RES_OK)
            return RES_ERR;

        usize arg_i = 0;
        if (parser_parse_primary(parser, &arg_i) != RES_OK) return RES_ERR;

        token_index_t rparen = 0;
        if (parser_expect_token(parser, LEX_TOKEN_ID_RPAREN, &rparen) != RES_OK)
            return RES_ERR;

        const ast_node_t new_node = {
            .node_kind = NODE_BUILTIN_PRINT,
            .node_n = {
                .node_builtin_print = {.bp_arg_i = arg_i,
                                       .bp_keyword_print_i = keyword_print,
                                       .bp_rparen_i = rparen}}};
        buf_push(parser->par_nodes, new_node);
        *new_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    return RES_NONE;
}

static res_t parser_parse(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    while (1) {
        usize new_node_i = 0;
        if (parser_parse_builtin_print(parser, &new_node_i) == RES_OK) {
            ast_node_dump(parser->par_nodes, new_node_i, 0);
            buf_push(parser->par_stmt_nodes, new_node_i);

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
