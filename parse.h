#pragma once

#include <stdint.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"
#include "lex.h"

typedef struct {
    token_id_t* par_token_ids;
    const char* par_source;
    const int par_source_len;
    const char* par_file_name0;
    int par_tok_i;
    ast_node_t* par_nodes;  // Arena of all nodes
    int* par_stmt_nodes;    // Array of statements. Each statement is
                            // stored as the node index which is the
                            // root of the statement in the ast
    loc_t* par_token_locs;
    lexer_t par_lexer;
    bool par_is_tty;
    obj_t* par_objects;
    type_t* par_types;
} parser_t;

static parser_t parser_init(const char* file_name0, const char* source,
                            int source_len) {
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
    PG_ASSERT_COND((int)buf_size(token_ids), ==, (int)buf_size(token_locs),
                   "%d");

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

static int64_t parse_tok_to_i64(const parser_t* parser, int tok_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const loc_t loc = parser->par_token_locs[tok_i];
    const char* const string = &parser->par_source[loc.loc_start];
    int string_len = loc.loc_end - loc.loc_start;

    PG_ASSERT_COND(string_len, >, (int)0, "%d");
    PG_ASSERT_COND(string_len, <, (int)25, "%d");

    // TOOD: limit in the lexer the length of a number literal
    static char string0[25] = "\0";
    memcpy(string0, string, (size_t)string_len);
    return strtoll(string0, NULL, 10);
}

static int64_t parse_tok_to_char(const parser_t* parser, int tok_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const loc_t loc = parser->par_token_locs[tok_i];
    const char* const string = &parser->par_source[loc.loc_start + 1];
    int string_len = loc.loc_end - loc.loc_start - 2;

    PG_ASSERT_COND(string_len, >, (int)0, "%d");
    PG_ASSERT_COND(string_len, <, (int)2, "%d");  // TODO: expand

    return string[0];
}

static void ast_node_dump(const ast_node_t* nodes, int node_i, int indent) {
    PG_ASSERT_COND((void*)nodes, !=, NULL, "%p");

    const ast_node_t* node = &nodes[node_i];
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT: {
            log_debug_with_indent(indent, "ast_node #%d %s", node_i,
                                  ast_node_kind_t_to_str[node->node_kind]);
            ast_node_dump(nodes, node->node_n.node_builtin_print.bp_arg_i, 2);
            break;
        }
        case NODE_ADD: {
            log_debug_with_indent(indent, "ast_node #%d %s", node_i,
                                  ast_node_kind_t_to_str[node->node_kind]);
            ast_node_dump(nodes, node->node_n.node_binary.bi_lhs_i, 2);
            ast_node_dump(nodes, node->node_n.node_binary.bi_rhs_i, 2);

            break;
        }
        case NODE_KEYWORD_BOOL:
        case NODE_I64:
        case NODE_CHAR:
        case NODE_STRING: {
            log_debug_with_indent(indent, "ast_node #%d %s", node_i,
                                  ast_node_kind_t_to_str[node->node_kind]);
            break;
        }
    }
}

static int ast_node_first_token(const parser_t* parser,
                                const ast_node_t* node) {
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT:
            return node->node_n.node_builtin_print.bp_keyword_print_i;
        case NODE_KEYWORD_BOOL:
            return node->node_n.node_boolean;
        case NODE_STRING:
            return parser->par_objects[node->node_n.node_string].obj_tok_i;
        case NODE_CHAR:
        case NODE_I64:
            return node->node_n.node_num.nu_tok_i;
        case NODE_ADD:
            return node->node_n.node_binary.bi_lhs_i;
    }
}

static int ast_node_last_token(const parser_t* parser, const ast_node_t* node) {
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINT:
            return node->node_n.node_builtin_print.bp_rparen_i;
        case NODE_KEYWORD_BOOL:
            return node->node_n.node_boolean;
        case NODE_STRING:
            return parser->par_objects[node->node_n.node_string].obj_tok_i;
        case NODE_I64:
        case NODE_CHAR:
            return node->node_n.node_num.nu_tok_i;
        case NODE_ADD:
            return node->node_n.node_binary.bi_rhs_i;
    }
}

static void parser_tok_source(const parser_t* parser, int tok_i,
                              const char** source, int* source_len) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source_len, !=, NULL, "%p");

    const token_id_t tok = parser->par_token_ids[tok_i];
    const loc_t loc = parser->par_token_locs[tok_i];

    // Without quotes for char/string
    *source = &parser->par_source[(tok == LEX_TOKEN_ID_STRING ||
                                   tok == LEX_TOKEN_ID_CHAR)
                                      ? loc.loc_start + 1
                                      : loc.loc_start];
    *source_len = (tok == LEX_TOKEN_ID_STRING || tok == LEX_TOKEN_ID_CHAR)
                      ? (loc.loc_end - loc.loc_start - 2)
                      : (loc.loc_end - loc.loc_start);
}

static void parser_ast_node_source(const parser_t* parser,
                                   const ast_node_t* node, const char** source,
                                   int* source_len) {
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source_len, !=, NULL, "%p");

    const int first = ast_node_first_token(parser, node);
    PG_ASSERT_COND(first, <, (int)buf_size(parser->par_token_locs), "%d");
    const int last = ast_node_last_token(parser, node);
    PG_ASSERT_COND(last, <, (int)buf_size(parser->par_token_locs), "%d");

    const loc_t first_token = parser->par_token_locs[first];
    const loc_t last_token = parser->par_token_locs[last];

    // Without quotes for char/string
    *source = &parser->par_source[(node->node_kind == NODE_STRING ||
                                   node->node_kind == NODE_CHAR)
                                      ? first_token.loc_start + 1
                                      : first_token.loc_start];
    *source_len =
        (node->node_kind == NODE_STRING || node->node_kind == NODE_CHAR)
            ? (last_token.loc_end - first_token.loc_start - 2)
            : (last_token.loc_end - first_token.loc_start);
}

static bool parser_is_at_end(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    return parser->par_tok_i >= (int)buf_size(parser->par_token_ids);
}

static token_id_t parser_current(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    return parser->par_token_ids[parser->par_tok_i];
}

static void parser_advance_until_after(parser_t* parser, token_id_t id) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, (int)0, "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%d");

    while (!parser_is_at_end(parser) && parser_current(parser) != id) {
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (int)buf_size(parser->par_token_ids), "%d");
        parser->par_tok_i += 1;
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (int)buf_size(parser->par_token_ids), "%d");
    }

    parser->par_tok_i += 1;
}

static token_id_t parser_peek(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, (int)0, "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%d");

    int i = parser->par_tok_i;

    while (i < (int)buf_size(parser->par_token_ids)) {
        const token_id_t id = parser->par_token_ids[i];
        log_debug("peeking at pos=%d", i);
        if (id == LEX_TOKEN_ID_COMMENT) {
            log_debug("Skipping over comment at pos=%d", i);
            i += 1;
            continue;
        }

        return id;
    }
    UNREACHABLE();
}

static void parser_print_source_on_error(const parser_t* parser,
                                         int first_tok_i, int last_tok_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const loc_t first_tok_loc = parser->par_token_locs[first_tok_i];
    const loc_t last_tok_loc = parser->par_token_locs[last_tok_i];

    const loc_pos_t first_tok_loc_pos =
        lex_pos(&parser->par_lexer, first_tok_loc.loc_start);

    const char* const actual_source =
        &parser->par_source[first_tok_loc.loc_start];
    const int actual_source_len =
        last_tok_loc.loc_end - first_tok_loc.loc_start;

    if (parser->par_is_tty) fprintf(stderr, "%s", color_grey);

    static char prefix[MAXPATHLEN + 50] = "\0";
    snprintf(prefix, sizeof(prefix), "%s:%d:%d:", parser->par_file_name0,
             first_tok_loc_pos.pos_line, first_tok_loc_pos.pos_column);
    int prefix_len = strlen(prefix);
    fprintf(stderr, "%s", prefix);
    if (parser->par_is_tty) fprintf(stderr, "%s", color_reset);

    // If there is a token before, print it
    if (first_tok_i > 0) {
        const loc_t before_first_tok_loc =
            parser->par_token_locs[first_tok_i - 1];
        const char* const before_first_tok_source =
            &parser->par_source[before_first_tok_loc.loc_start];
        const int before_first_tok_source_len =
            // Include spaces here, meaning we consider the start of the
            // previous token until the start of the actual token
            first_tok_loc.loc_start - before_first_tok_loc.loc_start;
        prefix_len += before_first_tok_source_len;

        fprintf(stderr, "%.*s", (int)before_first_tok_source_len,
                before_first_tok_source);
    }
    fprintf(stderr, "%.*s", (int)actual_source_len, actual_source);

    // If there is a token after, print it
    if (last_tok_i < (int)buf_size(parser->par_token_ids) - 1) {
        const loc_t after_last_tok_loc = parser->par_token_locs[last_tok_i + 1];
        const char* const after_last_tok_source =
            &parser->par_source[last_tok_loc.loc_end];
        const int after_last_tok_source_len =
            // Include spaces here, meaning we consider the end of the
            // actual token until the end of the next token
            after_last_tok_loc.loc_end - last_tok_loc.loc_end;

        // Do not add to prefix_len here since this is the suffix

        fprintf(stderr, "%.*s", (int)after_last_tok_source_len,
                after_last_tok_source);
    }

    fprintf(stderr, "\n");
    for (int i = 0; i < prefix_len; i++) fprintf(stderr, " ");

    if (parser->par_is_tty) fprintf(stderr, "%s", color_red);
    for (int i = 0; i < actual_source_len; i++) fprintf(stderr, "^");
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

    parser_print_source_on_error(parser, parser->par_tok_i, parser->par_tok_i);

    return res;
}

static res_t parser_err_non_matching_types(const parser_t* parser, int lhs_i,
                                           int rhs_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const ast_node_t* const lhs = &parser->par_nodes[lhs_i];
    const ast_node_t* const rhs = &parser->par_nodes[rhs_i];

    const type_t* const lhs_type = &parser->par_types[lhs->node_type_i];
    const type_t* const rhs_type = &parser->par_types[rhs->node_type_i];

    const int lhs_first_tok_i = ast_node_first_token(parser, lhs);
    const int rhs_last_tok_i = ast_node_last_token(parser, rhs);

    const loc_t lhs_first_tok_loc = parser->par_token_locs[lhs_first_tok_i];

    const loc_pos_t lhs_first_tok_loc_pos =
        lex_pos(&parser->par_lexer, lhs_first_tok_loc.loc_start);

    const res_t res = RES_NON_MATCHING_TYPES;
    fprintf(stderr, res_to_str[res], (parser->par_is_tty ? color_grey : ""),
            parser->par_file_name0, lhs_first_tok_loc_pos.pos_line,
            lhs_first_tok_loc_pos.pos_column,
            (parser->par_is_tty ? color_reset : ""),
            type_to_str[lhs_type->ty_kind], type_to_str[rhs_type->ty_kind]);

    parser_print_source_on_error(parser, lhs_first_tok_i, rhs_last_tok_i);

    return res;
}

static bool parser_match(parser_t* parser, token_id_t id,
                         int* return_token_index) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, (int)0, "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%d");

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
    PG_ASSERT_COND(parser->par_tok_i, <, (int)buf_size(parser->par_token_ids),
                   "%d");

    *return_token_index = parser->par_tok_i - 1;

    log_debug("matched %s, now current token: %s at tok_i=%d",
              token_id_t_to_str[id], token_id_t_to_str[parser_current(parser)],
              parser->par_tok_i);

    return true;
}

static res_t parser_parse_primary(parser_t* parser, int* new_primary_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_primary_node_i, !=, NULL, "%p");

    int tok_i = INT32_MAX;

    if (parser_match(parser, LEX_TOKEN_ID_TRUE, &tok_i) ||
        parser_match(parser, LEX_TOKEN_ID_FALSE, &tok_i)) {
        buf_push(parser->par_types,
                 ((type_t){.ty_size = 1, .ty_kind = TYPE_BOOL}));
        const int type_i = buf_size(parser->par_types) - 1;

        const ast_node_t new_node = NODE_BOOL(tok_i, type_i);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, LEX_TOKEN_ID_STRING, &tok_i)) {
        buf_push(parser->par_types,
                 ((type_t){.ty_size = 1, .ty_kind = TYPE_STRING}));
        const int type_i = buf_size(parser->par_types) - 1;

        const char* source = NULL;
        int source_len = 0;
        parser_tok_source(parser, tok_i, &source, &source_len);
        const obj_t obj = OBJ_GLOBAL_VAR(type_i, tok_i, source, source_len);
        buf_push(parser->par_objects, obj);
        const int obj_i = buf_size(parser->par_objects) - 1;

        const ast_node_t new_node = NODE_STRING(obj_i, type_i);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = (int)buf_size(parser->par_nodes) - 1;

        log_debug("new object: type=TYPE_STRING tok_i=%d source_len=%d",
                  obj.obj_tok_i, source_len);

        return RES_OK;
    }
    if (parser_match(parser, LEX_TOKEN_ID_I64, &tok_i)) {
        buf_push(parser->par_types,
                 ((type_t){.ty_size = 8, .ty_kind = TYPE_I64}));
        const int type_i = buf_size(parser->par_types) - 1;

        const int64_t val = parse_tok_to_i64(parser, tok_i);
        const ast_node_t new_node = NODE_I64(tok_i, type_i, val);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, LEX_TOKEN_ID_CHAR, &tok_i)) {
        buf_push(parser->par_types,
                 ((type_t){.ty_size = 1, .ty_kind = TYPE_CHAR}));
        const int type_i = buf_size(parser->par_types) - 1;

        const int64_t val = parse_tok_to_char(parser, tok_i);
        const ast_node_t new_node = NODE_CHAR(tok_i, type_i, val);
        buf_push(parser->par_nodes, new_node);
        *new_primary_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return RES_NONE;
}

static res_t parser_parse_addition(parser_t* parser, int* new_node_i) {
    res_t res = RES_NONE;

    int lhs_i = INT32_MAX;
    if ((res = parser_parse_primary(parser, &lhs_i)) != RES_OK) return res;
    const int lhs_type_i = parser->par_nodes[lhs_i].node_type_i;
    const type_t lhs_type = parser->par_types[lhs_type_i];
    *new_node_i = lhs_i;
    log_debug("new_node_i=%d", *new_node_i);

    if (parser_match(parser, LEX_TOKEN_ID_PLUS, new_node_i)) {
        int rhs_i = INT32_MAX;
        if ((res = parser_parse_addition(parser, &rhs_i)) != RES_OK) return res;
        const int rhs_type_i = parser->par_nodes[rhs_i].node_type_i;
        const type_t rhs_type = parser->par_types[rhs_type_i];

        if (lhs_type.ty_kind != rhs_type.ty_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        buf_push(parser->par_types, lhs_type);
        const ast_node_t new_node = NODE_ADD(lhs_i, rhs_i, lhs_type_i);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;
        log_debug("new_node_i=%d", *new_node_i);

        return RES_OK;
    }

    return res;
}

static res_t parser_parse_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_addition(parser, new_node_i);
}
static res_t parser_expect_token(parser_t* parser, token_id_t expected,
                                 int* token) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, (int)0, "%d");
    PG_ASSERT_COND((void*)token, !=, NULL, "%p");

    if (!parser_match(parser, expected, token)) {
        log_debug("expected token not found: %s", token_id_t_to_str[expected]);
        return parser_err_unexpected_token(parser, expected);
    }
    return RES_OK;
}

static res_t parser_parse_builtin_print(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int keyword_print_i = INT32_MAX;
    res_t res = RES_NONE;
    if (parser_match(parser, LEX_TOKEN_ID_BUILTIN_PRINT, &keyword_print_i)) {
        int lparen = 0;
        if ((res = parser_expect_token(parser, LEX_TOKEN_ID_LPAREN, &lparen)) !=
            RES_OK)
            return res;

        int arg_i = 0;
        if ((res = parser_parse_expr(parser, &arg_i)) != RES_OK) return res;

        int rparen = 0;
        if ((res = parser_expect_token(parser, LEX_TOKEN_ID_RPAREN, &rparen)) !=
            RES_OK)
            return res;

        buf_push(parser->par_types,
                 ((type_t){.ty_size = 1, .ty_kind = TYPE_BUILTIN_PRINT}));
        const int type_i = buf_size(parser->par_types) - 1;
        const ast_node_t new_node =
            NODE_PRINT(arg_i, keyword_print_i, rparen, type_i);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    return RES_NONE;
}

static res_t parser_parse_stmt(parser_t* parser, int* new_node_i) {
    return parser_parse_builtin_print(parser, new_node_i);
}

static res_t parser_parse(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_token_ids, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, (int)0, "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_token_ids), >, parser->par_tok_i,
                   "%d");

    int new_node_i = INT32_MAX;
    res_t res = RES_NONE;

    if ((res = parser_parse_stmt(parser, &new_node_i)) == RES_OK) {
        ast_node_dump(parser->par_nodes, new_node_i, 0);
        buf_push(parser->par_stmt_nodes, new_node_i);

    } else
        return res;

    while (!parser_is_at_end(parser)) {
        if ((res = parser_parse_stmt(parser, &new_node_i)) == RES_OK) {
            ast_node_dump(parser->par_nodes, new_node_i, 0);
            buf_push(parser->par_stmt_nodes, new_node_i);

            continue;
        }
        log_debug("failed to parse builtin_print: res=%d tok_i=%d", res,
                  parser->par_tok_i);

        const token_id_t current = parser_current(parser);
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
