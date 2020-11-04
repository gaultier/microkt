#pragma once

#include <unistd.h>

#include "ast.h"
#include "common.h"
#include "ir.h"
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
    lexer_t par_lexer;
    bool par_is_tty;
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

    while (true) {
        const token_t token = lex_next(&lexer);
        token_dump(&token, &lexer);

        buf_push(token_ids, token.tok_id);
        buf_push(token_locs, token.tok_loc);

        if (token.tok_id == LEX_TOKEN_ID_EOF) break;
    }
    PG_ASSERT_COND(buf_size(token_ids), ==, buf_size(token_locs), "%lu");

    return (parser_t){.par_file_name0 = file_name0,
                      .par_source = source,
                      .par_source_len = source_len,
                      .par_token_ids = token_ids,
                      .par_nodes = NULL,
                      .par_stmt_nodes = NULL,
                      .par_token_locs = token_locs,
                      .par_tok_i = 0,
                      .par_lexer = lexer,
                      .par_is_tty = isatty(2)};
}

static void parser_ast_node_source(const parser_t* parser,
                                   const ast_node_t* node, const u8** source,
                                   usize* source_len) {
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source_len, !=, NULL, "%p");

    const token_index_t first = ast_node_first_token(node);
    PG_ASSERT_COND(first, <, (usize)buf_size(parser->par_token_locs), "%llu");
    const token_index_t last = ast_node_last_token(node);
    PG_ASSERT_COND(last, <, (usize)buf_size(parser->par_token_locs), "%llu");

    const loc_t first_token = parser->par_token_locs[first];
    const loc_t last_token = parser->par_token_locs[last];

    *source = &parser->par_source[(node->node_kind == NODE_STRING)
                                      ? first_token.loc_start + 1
                                      : first_token.loc_start];
    *source_len = (node->node_kind == NODE_STRING)
                      ? (last_token.loc_end - first_token.loc_start - 2)
                      : (last_token.loc_end - first_token.loc_start);
}

static bool parser_is_at_end(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    return parser->par_tok_i >= buf_size(parser->par_token_ids);
}

static token_id_t parser_current(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    return parser->par_token_ids[parser->par_tok_i];
}

static void parser_advance_until_after(parser_t* parser, token_id_t id) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    while (!parser_is_at_end(parser) && parser_current(parser) != id) {
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (usize)buf_size(parser->par_token_ids), "%llu");
        parser->par_tok_i += 1;
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (usize)buf_size(parser->par_token_ids), "%llu");
    }

    parser->par_tok_i += 1;
}

static token_id_t parser_peek(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    usize i = parser->par_tok_i;

    while (i < buf_size(parser->par_token_ids)) {
        const token_id_t id = parser->par_token_ids[i];
        log_debug("peeking at pos=%llu", i);
        if (id == LEX_TOKEN_ID_COMMENT) {
            log_debug("Skipping over comment at pos=%llu", i);
            i += 1;
            continue;
        }

        return id;
    }
    UNREACHABLE();
}

static bool parser_match(parser_t* parser, token_id_t id,
                         token_index_t* return_token_index) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%llu");

    if (parser_is_at_end(parser)) {
        log_debug("did not match %s, at end", token_id_t_to_str[id]);
        return false;
    }

    const token_id_t current_id = parser_peek(parser);
    if (id != current_id) {
        log_debug("did not match %s, got %s", token_id_t_to_str[id],
                  token_id_t_to_str[current_id]);
        return false;
    }

    parser_advance_until_after(parser, id);
    PG_ASSERT_COND(parser->par_tok_i, <, (usize)buf_size(parser->par_token_ids),
                   "%llu");

    *return_token_index = parser->par_tok_i - 1;

    log_debug("matched %s, now current token: %s at tok_i=%llu",
              token_id_t_to_str[id], token_id_t_to_str[parser_current(parser)],
              parser->par_tok_i);

    return true;
}

static res_t parser_parse_primary(parser_t* parser, usize* new_primary_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_primary_node_i, !=, NULL, "%p");

    token_index_t token = 0;

    if (parser_match(parser, LEX_TOKEN_ID_TRUE, &token)) {
        const ast_node_t new_node = NODE_BOOL(token);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, LEX_TOKEN_ID_FALSE, &token)) {
        const ast_node_t new_node = NODE_BOOL(token);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, LEX_TOKEN_ID_STRING, &token)) {
        const ast_node_t new_node = NODE_STRING(token);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, LEX_TOKEN_ID_INT, &token)) {
        const ast_node_t new_node = NODE_INT(token);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return RES_NONE;
}

static void parser_print_source_on_error(const parser_t* parser,
                                         const loc_t* actual_token_loc,
                                         const loc_pos_t* pos_start) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)actual_token_loc, !=, NULL, "%p");
    PG_ASSERT_COND((void*)pos_start, !=, NULL, "%p");

    const u8* const actual_source =
        &parser->par_source[actual_token_loc->loc_start];
    const usize actual_source_len =
        actual_token_loc->loc_end - actual_token_loc->loc_start;

    if (parser->par_is_tty) fprintf(stderr, "%s", color_grey);

    static u8 prefix[MAXPATHLEN + 50] = "\0";
    snprintf(prefix, sizeof(prefix), "%s:%llu:%llu:", parser->par_file_name0,
             pos_start->pos_line, pos_start->pos_column);
    usize prefix_len = strlen(prefix);
    fprintf(stderr, "%s", prefix);
    if (parser->par_is_tty) fprintf(stderr, "%s", color_reset);

    // If there is a token before, print it
    if (parser->par_tok_i > 0) {
        const loc_t before_actual_token_loc =
            parser->par_token_locs[parser->par_tok_i - 1];
        const u8* const before_actual_source =
            &parser->par_source[before_actual_token_loc.loc_start];
        const usize before_actual_source_len =
            // Include spaces here, meaning we consider the start of the
            // previous token until the start of the actual token
            actual_token_loc->loc_start - before_actual_token_loc.loc_start;
        prefix_len += before_actual_source_len;

        fprintf(stderr, "%.*s", (int)before_actual_source_len,
                before_actual_source);
    }
    fprintf(stderr, "%.*s", (int)actual_source_len, actual_source);

    // If there is a token after, print it
    if (parser->par_tok_i < buf_size(parser->par_token_ids) - 1) {
        const loc_t after_actual_token_loc =
            parser->par_token_locs[parser->par_tok_i + 1];
        const u8* const after_actual_source =
            &parser->par_source[actual_token_loc->loc_end];
        const usize after_actual_source_len =
            // Include spaces here, meaning we consider the end of the
            // actual token until the end of the next token
            after_actual_token_loc.loc_end - actual_token_loc->loc_end;

        // Do not add to prefix_len here since this is the suffix

        fprintf(stderr, "%.*s", (int)after_actual_source_len,
                after_actual_source);
    }

    fprintf(stderr, "\n");
    for (usize i = 0; i < prefix_len; i++) fprintf(stderr, " ");

    if (parser->par_is_tty) fprintf(stderr, "%s", color_red);
    for (usize i = 0; i < actual_source_len; i++) fprintf(stderr, "^");
    if (parser->par_is_tty) fprintf(stderr, "%s", color_reset);

    fprintf(stderr, "\n");
}

static res_t parser_err_unexpected_token(const parser_t* parser,
                                         token_id_t expected) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const res_t res = RES_UNEXPECTED_TOKEN;

    const loc_t actual_token_loc = parser->par_token_locs[parser->par_tok_i];
    const loc_pos_t pos_start =
        lex_pos(&parser->par_lexer, actual_token_loc.loc_start);

    fprintf(stderr, res_to_str[res], (parser->par_is_tty ? color_grey : ""),
            parser->par_file_name0, pos_start.pos_line, pos_start.pos_column,
            (parser->par_is_tty ? color_reset : ""),
            token_id_t_to_str[expected],
            token_id_t_to_str[parser_current(parser)]);

    parser_print_source_on_error(parser, &actual_token_loc, &pos_start);

    return RES_UNEXPECTED_TOKEN;
}

static res_t parser_expect_token(parser_t* parser, token_id_t expected,
                                 token_index_t* token) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((usize)buf_size(parser->par_token_ids), >, (usize)0, "%llu");
    PG_ASSERT_COND((void*)token, !=, NULL, "%p");

    if (!parser_match(parser, expected, token)) {
        log_debug("expected token not found: %s", token_id_t_to_str[expected]);
        return parser_err_unexpected_token(parser, expected);
    }
    return RES_OK;
}

static res_t parser_parse_builtin_print(parser_t* parser, usize* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    token_index_t keyword_print = 0;
    res_t res = RES_NONE;
    if (parser_match(parser, LEX_TOKEN_ID_BUILTIN_PRINT, &keyword_print)) {
        token_index_t lparen = 0;
        if ((res = parser_expect_token(parser, LEX_TOKEN_ID_LPAREN, &lparen)) !=
            RES_OK)
            return res;

        usize arg_i = 0;
        if ((res = parser_parse_primary(parser, &arg_i)) != RES_OK) return res;

        token_index_t rparen = 0;
        if ((res = parser_expect_token(parser, LEX_TOKEN_ID_RPAREN, &rparen)) !=
            RES_OK)
            return res;

        const ast_node_t new_node = NODE_PRINT(arg_i, keyword_print, rparen);
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

    usize new_node_i = 0;
    res_t res = RES_NONE;

    if ((res = parser_parse_builtin_print(parser, &new_node_i)) == RES_OK) {
        ast_node_dump(parser->par_nodes, new_node_i, 0);
        buf_push(parser->par_stmt_nodes, new_node_i);

    } else {
        return parser_err_unexpected_token(parser, LEX_TOKEN_ID_BUILTIN_PRINT);
    }

    while (!parser_is_at_end(parser)) {
        if ((res = parser_parse_builtin_print(parser, &new_node_i)) == RES_OK) {
            ast_node_dump(parser->par_nodes, new_node_i, 0);
            buf_push(parser->par_stmt_nodes, new_node_i);

            continue;
        }
        log_debug("failed to parse builtin_print: res=%d tok_i=%llu", res,
                  parser->par_tok_i);

        const token_index_t current = parser_current(parser);
        if (current == LEX_TOKEN_ID_COMMENT) {
            parser->par_tok_i += 1;
            continue;
        }
        if (current == LEX_TOKEN_ID_EOF)
            return RES_OK;
        else
            return res;
    }
    UNREACHABLE();
}

static usize parse_node_to_int(const parser_t* parser, const ast_node_t* node) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");
    PG_ASSERT_COND(node->node_kind, ==, NODE_INT, "%d");

    const u8* string = NULL;
    usize string_len = 0;
    parser_ast_node_source(parser, node, &string, &string_len);
    log_debug("emit_call_print_integer int `%.*s`", (int)string_len, string);
    PG_ASSERT_COND(string_len, <, (usize)25, "%llu");

    // TOOD: limit in the lexer the length of a number literal
    static u8 string0[25] = "\0";
    memcpy(string0, string, string_len);
    return strtoll(string0, NULL, 10);
}
