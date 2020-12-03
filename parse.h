#pragma once

#include <stdarg.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"
#include "lex.h"

static const int TYPE_UNIT_I = 0;  // see parser_init
static const int TYPE_ANY_I = 1;   // see parser_init

typedef struct {
    const char* par_file_name0;
    int par_tok_i, par_scope_i, par_fn_i;  // Current token/scope/function
    node_t* par_nodes;                     // Arena of all nodes
    lexer_t par_lexer;
    int* par_node_decls;  // Declarations that need to be generated first e.g.
                          // functions
    type_t* par_types;
    int par_offset;  // Local variable stack offset inside the current function
    bool par_is_tty;
} parser_t;

static res_t parser_parse_expr(parser_t* parser, int* new_node_i);
static res_t parser_parse_stmt(parser_t* parser, int* new_node_i);
static res_t parser_parse_control_structure_body(parser_t* parser,
                                                 int* new_node_i);
static res_t parser_parse_block(parser_t* parser, int* new_node_i);
static res_t parser_parse_builtin_println(parser_t* parser, int* new_node_i);
static void parser_tok_source(const parser_t* parser, int tok_i,
                              const char** source, int* source_len);
static void parser_print_source_on_error(const parser_t* parser,
                                         int first_tok_i, int last_tok_i);
static res_t parser_parse_call_suffix(parser_t* parser, int* new_node_i);

static node_t* parser_current_block(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    return &parser->par_nodes[parser->par_scope_i];
}

static int parser_enter_scope(parser_t* parser, int block_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const int parent_scope_i = parser->par_scope_i;
    parser->par_scope_i = block_node_i;

    log_debug("%d -> %d", parent_scope_i, parser->par_scope_i);
    return parent_scope_i;
}

static void parser_leave_scope(parser_t* parser, int parent_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    log_debug("%d <- %d", parent_node_i, parser->par_scope_i);
    parser->par_scope_i = parent_node_i;
}

static int parser_node_find_fn_decl_for_call(const parser_t* parser,
                                             int node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const node_t* const node = &parser->par_nodes[node_i];
    switch (node->node_kind) {
        case NODE_VAR:
            return parser_node_find_fn_decl_for_call(
                parser, node->node_n.node_var.va_var_node_i);
        case NODE_FN_DECL:
            return node_i;
        default:
            log_debug("kind: %s", node_kind_to_str[node->node_kind]);
            UNIMPLEMENTED();
    }
}

// TODO: optimize, currently it is O(n*m) where n= # of stmt and m = # of var
// def per scope
static res_t parser_resolve_var(const parser_t* parser, int tok_i,
                                int* def_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)def_node_i, !=, NULL, "%p");

    const char* var_source = NULL;
    int var_source_len = 0;
    parser_tok_source(parser, tok_i, &var_source, &var_source_len);

    int current_scope_i = parser->par_scope_i;
    while (current_scope_i >= 0) {
        const node_t* block = &parser->par_nodes[current_scope_i];
        const block_t b = block->node_n.node_block;

        log_debug("resolving var %.*s in scope %d", var_source_len, var_source,
                  current_scope_i);

        // Reverse order to allow for shadowing
        for (int i = (int)buf_size(b.bl_nodes_i) - 1; i >= 0; i--) {
            const int stmt_i = b.bl_nodes_i[i];
            const node_t* const stmt = &parser->par_nodes[stmt_i];
            const char* def_source = NULL;
            int def_source_len = 0;

            if (stmt->node_kind == NODE_VAR_DEF) {
                parser_tok_source(parser,
                                  stmt->node_n.node_var_def.vd_name_tok_i,
                                  &def_source, &def_source_len);
            } else if (stmt->node_kind == NODE_FN_DECL) {
                parser_tok_source(parser,
                                  stmt->node_n.node_fn_decl.fd_name_tok_i,
                                  &def_source, &def_source_len);
            } else
                continue;

            log_debug("considering var def: name=`%.*s` kind=%s scope=%d",
                      def_source_len, def_source,
                      node_kind_to_str[stmt->node_kind], current_scope_i);

            if (def_source_len == var_source_len &&
                memcmp(def_source, var_source, var_source_len) == 0) {
                *def_node_i = stmt_i;

                log_debug("resolved var: name=`%.*s` kind=%s scope=%d",
                          def_source_len, def_source,
                          node_kind_to_str[stmt->node_kind], current_scope_i);

                return RES_OK;
            }
        }

        current_scope_i = b.bl_parent_scope_i;
    }

    log_debug("var `%.*s` could not be resolved", var_source_len, var_source);
    return RES_NONE;
}

static res_t parser_err_assigning_val(const parser_t* parser, int assign_tok_i,
                                      const var_def_t* var_def) {
    const loc_t vd_loc_start =
        parser->par_lexer.lex_locs[var_def->vd_first_tok_i];
    const loc_t assign_loc_start = parser->par_lexer.lex_locs[assign_tok_i];

    fprintf(stderr,
            "%s%s:%d:%d:%sTrying to assign a variable declared with `val`\n",
            (parser->par_is_tty ? color_grey : ""), parser->par_file_name0,
            assign_loc_start.loc_line, assign_loc_start.loc_column,
            (parser->par_is_tty ? color_reset : ""));

    parser_print_source_on_error(parser, assign_tok_i, assign_tok_i);
    fprintf(stderr, "%s%s:%d:%d:%sDeclared here:\n",
            (parser->par_is_tty ? color_grey : ""), parser->par_file_name0,
            vd_loc_start.loc_line, vd_loc_start.loc_column,
            (parser->par_is_tty ? color_reset : ""));
    parser_print_source_on_error(parser, var_def->vd_first_tok_i,
                                 var_def->vd_first_tok_i);
    return RES_ASSIGNING_VAL;
}

static res_t parser_err_missing_rhs(const parser_t* parser, int first_tok_i,
                                    int last_tok_i) {
    const loc_t first_tok_loc = parser->par_lexer.lex_locs[first_tok_i];

    fprintf(stderr, "%s%s:%d:%d:%sMissing right hand-side operand\n",
            (parser->par_is_tty ? color_grey : ""), parser->par_file_name0,
            first_tok_loc.loc_line, first_tok_loc.loc_column,
            (parser->par_is_tty ? color_reset : ""));

    parser_print_source_on_error(parser, first_tok_i, last_tok_i);
    return RES_ERR;  // FIXME?
}

static int parser_make_type(parser_t* parser,
                            type_kind_t type_kind) {  // Returns type_i
                                                      // TODO: deduplicate?
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    type_t type = {.ty_kind = type_kind, .ty_size = 0};

    switch (type_kind) {
        case TYPE_LONG:
        case TYPE_STRING:
        case TYPE_ANY: {
            type.ty_size = 8;
            break;
        }
        case TYPE_INT: {
            type.ty_size = 4;
            break;
        }
        case TYPE_SHORT: {
            type.ty_size = 2;
            break;
        }
        case TYPE_BOOL:
        case TYPE_BYTE:
        case TYPE_CHAR: {
            type.ty_size = 1;
            break;
        }
        case TYPE_UNIT: {
            type.ty_size = 0;
            break;
        }
            // User defined type
        default:
            UNIMPLEMENTED();
    }

    buf_push(parser->par_types, type);

    return buf_size(parser->par_types) - 1;
}

static bool parser_check_keyword(const parser_t* parser,
                                 const char* source_start, const char suffix[],
                                 int suffix_len, type_kind_t* type_kind,
                                 type_kind_t expected) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)type_kind, !=, NULL, "%p");
    PG_ASSERT_COND((void*)(source_start + suffix_len), <,
                   (void*)(parser->par_lexer.lex_source +
                           parser->par_lexer.lex_source_len),
                   "%p");

    const int remaining_len =
        (parser->par_lexer.lex_source + parser->par_lexer.lex_source_len) -
        (source_start);

    if (remaining_len >= suffix_len &&
        memcmp(source_start, suffix, suffix_len) == 0) {
        *type_kind = expected;
        return true;
    } else
        return false;
}

static bool parser_parse_identifier_to_type_kind(const parser_t* parser,
                                                 int tok_i,
                                                 type_kind_t* type_kind) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const char* source = NULL;
    int source_len = 0;
    parser_tok_source(parser, tok_i, &source, &source_len);

    if (source_len <= 2) return RES_NONE;

    switch (source[0]) {
        case 'A':
            return parser_check_keyword(parser, source + 1, "ny", 2, type_kind,
                                        TYPE_ANY);
        case 'B': {
            switch (source[1]) {
                case 'o':
                    return parser_check_keyword(parser, source + 2, "ol", 2,
                                                type_kind, TYPE_BOOL);
                case 'y':
                    return parser_check_keyword(parser, source + 2, "te", 2,
                                                type_kind, TYPE_BYTE);
                default:
                    return false;
            }
        }
        case 'C':
            return parser_check_keyword(parser, source + 1, "har", 3, type_kind,
                                        TYPE_CHAR);
        case 'I':
            return parser_check_keyword(parser, source + 1, "nt", 2, type_kind,
                                        TYPE_INT);
        case 'L':
            return parser_check_keyword(parser, source + 1, "ong", 3, type_kind,
                                        TYPE_LONG);
        case 'S': {
            switch (source[1]) {
                case 'h':
                    return parser_check_keyword(parser, source + 2, "ort", 3,
                                                type_kind, TYPE_SHORT);
                case 't':
                    return parser_check_keyword(parser, source + 2, "ring", 4,
                                                type_kind, TYPE_STRING);
                default:
                    return false;
            }
        }
        case 'U': {
            switch (source[1]) {
                case 'n':
                    return parser_check_keyword(parser, source + 2, "it", 2,
                                                type_kind, TYPE_UNIT);
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

static parser_t parser_init(const char* file_name0, const char* source,
                            int source_len) {
    PG_ASSERT_COND((void*)file_name0, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");

    lexer_t lexer = lex_init(source, source_len);

    type_t* types = NULL;
    buf_grow(types, 100);

    // Add root main function
    buf_push(lexer.lex_tokens,
             ((token_t){.tok_id = TOK_ID_IDENTIFIER,
                        .tok_pos_range = {.pr_start = 0, .pr_end = 0}}));
    buf_push(lexer.lex_tok_pos_ranges,
             ((pos_range_t){.pr_start = 0, .pr_end = 0}));
    buf_push(lexer.lex_locs, ((loc_t){.loc_line = 1, .loc_column = 1}));
    const int fn_main_name_tok_i = buf_size(lexer.lex_tokens) - 1;
    node_t* nodes = NULL;
    buf_grow(nodes, 100);
    // Add initial scope
    buf_push(nodes,
             ((node_t){.node_kind = NODE_BLOCK,
                       .node_type_i = TYPE_UNIT_I,
                       .node_n = {.node_block = {.bl_first_tok_i = -1,
                                                 .bl_last_tok_i = -1,
                                                 .bl_nodes_i = NULL,
                                                 .bl_parent_scope_i = -1}}}));
    buf_push(nodes,
             ((node_t){.node_type_i = TYPE_UNIT_I,
                       .node_kind = NODE_FN_DECL,
                       .node_n = {.node_fn_decl = {
                                      .fd_first_tok_i = fn_main_name_tok_i,
                                      .fd_name_tok_i = fn_main_name_tok_i,
                                      .fd_last_tok_i = fn_main_name_tok_i,
                                      .fd_body_node_i = 0,
                                      .fd_flags = FN_FLAGS_SYNTHETIC |
                                                  FN_FLAGS_PUBLIC}}}));
    int* node_decls = NULL;
    buf_grow(node_decls, 10);
    buf_push(node_decls,
             buf_size(nodes) - 1);  // Last node is the main function

    parser_t parser = {
        .par_file_name0 = file_name0,
        .par_node_decls = node_decls,
        .par_nodes = nodes,
        .par_lexer = lexer,
        .par_is_tty = isatty(2),
        .par_types = types,
        .par_fn_i = node_decls[0],
        .par_scope_i = 0,
    };
    parser_make_type(&parser, TYPE_UNIT);  // Hence TYPE_UNIT_I = 0
    parser_make_type(&parser, TYPE_ANY);   // Hence TYPE_ANY_I = 1

    return parser;
}

static long long int parse_tok_to_long(const parser_t* parser, int tok_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_tok_pos_ranges, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_source, !=, NULL, "%p");

    static char string0[25];

    const pos_range_t pos_range = parser->par_lexer.lex_tok_pos_ranges[tok_i];
    const char* const string =
        &parser->par_lexer.lex_source[pos_range.pr_start];
    const int string_len = pos_range.pr_end - pos_range.pr_start;

    PG_ASSERT_COND(string_len, >, (int)0, "%d");
    PG_ASSERT_COND(string_len, <, (int)25, "%d");

    // TOOD: limit in the lexer the length of a number literal
    memset(string0, 0, sizeof(string0));
    memcpy(string0, string, (size_t)string_len);

    return strtoll(string0, NULL, 10);
}

static long long int parse_tok_to_char(const parser_t* parser, int tok_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const pos_range_t pos_range = parser->par_lexer.lex_tok_pos_ranges[tok_i];
    const char* const string =
        &parser->par_lexer.lex_source[pos_range.pr_start + 1];
    int string_len = pos_range.pr_end - pos_range.pr_start - 2;

    PG_ASSERT_COND(string_len, >, (int)0, "%d");
    PG_ASSERT_COND(string_len, <, (int)2, "%d");  // TODO: expand

    return string[0];
}

static void node_dump(const parser_t* parser, int node_i, int indent) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const node_t* node = &parser->par_nodes[node_i];
    switch (node->node_kind) {
        case NODE_BUILTIN_PRINTLN: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind]);
            node_dump(parser, node->node_n.node_builtin_println.bp_arg_i,
                      indent + 2);
            break;
        }
        case NODE_LT:
        case NODE_LE:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_MULTIPLY:
        case NODE_DIVIDE:
        case NODE_MODULO:
        case NODE_SUBTRACT:
        case NODE_ASSIGN:
        case NODE_ADD: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind]);
            node_dump(parser, node->node_n.node_binary.bi_lhs_i, indent + 2);
            node_dump(parser, node->node_n.node_binary.bi_rhs_i, indent + 2);

            break;
        }
        case NODE_IF: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind]);
            node_dump(parser, node->node_n.node_if.if_node_cond_i, indent + 2);
            node_dump(parser, node->node_n.node_if.if_node_then_i, indent + 2);

            if (node->node_n.node_if.if_node_else_i >= 0)
                node_dump(parser, node->node_n.node_if.if_node_else_i,
                          indent + 2);

            break;
        }
        case NODE_RETURN:
        case NODE_NOT: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind]);
            if (node->node_n.node_unary.un_node_i >= 0)
                node_dump(parser, node->node_n.node_unary.un_node_i,
                          indent + 2);

            break;
        }
        case NODE_KEYWORD_BOOL:
        case NODE_LONG:
        case NODE_CHAR:
        case NODE_STRING: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind]);
            break;
        }
        case NODE_BLOCK: {
            const block_t block = node->node_n.node_block;
            log_debug_with_indent(
                indent, "node #%d %s type=%s parent_scope=%d", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind],
                block.bl_parent_scope_i);

            for (int i = 0; i < (int)buf_size(block.bl_nodes_i); i++)
                node_dump(parser, block.bl_nodes_i[i], indent + 2);

            break;
        }
        case NODE_VAR_DEF: {
#ifdef WITH_LOGS
            const var_def_t var_def = node->node_n.node_var_def;
            const pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[var_def.vd_name_tok_i];

            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const int name_len = pos_range.pr_end - pos_range.pr_start;
            log_debug_with_indent(
                indent, "node #%d %s type=%s name=%.*s offset=%d flags=%d",
                node_i, node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind],
                name_len, name, var_def.vd_stack_offset, var_def.vd_flags);

            if (var_def.vd_init_node_i >= 0)
                node_dump(parser, var_def.vd_init_node_i, indent + 2);

#endif
            break;
        }
        case NODE_VAR: {
#if WITH_LOGS
            const var_t var = node->node_n.node_var;
            const node_t* const node_var_def =
                &parser->par_nodes[var.va_var_node_i];
            const var_def_t var_def = node_var_def->node_n.node_var_def;
            const pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[var_def.vd_name_tok_i];

            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const int name_len = pos_range.pr_end - pos_range.pr_start;
#endif

            log_debug_with_indent(
                indent, "node #%d %s type=%s name=%.*s offset=%d", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind],
                name_len, name, var_def.vd_stack_offset);

            break;
        }
        case NODE_WHILE: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind]);

            node_dump(parser, node->node_n.node_while.wh_cond_i, indent + 2);
            node_dump(parser, node->node_n.node_while.wh_body_i, indent + 2);
            return;
        }
        case NODE_FN_DECL: {
#ifdef WITH_LOGS
            const fn_decl_t fn_decl = node->node_n.node_fn_decl;
            const int arity = buf_size(fn_decl.fd_arg_nodes_i);
            const pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[fn_decl.fd_name_tok_i];
            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const int name_len = pos_range.pr_end - pos_range.pr_start;
#endif
            log_debug_with_indent(
                indent, "node #%d %s `%.*s` type=%s arity=%d body_i=%d", node_i,
                node_kind_to_str[node->node_kind], name_len, name,
                type_to_str[parser->par_types[node->node_type_i].ty_kind],
                arity, fn_decl.fd_body_node_i);
            return;
        }
        case NODE_CALL: {
#ifdef WITH_LOGS
            const call_t call = node->node_n.node_call;
            log_debug_with_indent(
                indent, "node #%d %s type=%s arity=%d", node_i,
                node_kind_to_str[node->node_kind],
                type_to_str[parser->par_types[node->node_type_i].ty_kind],
                (int)buf_size(call.ca_arg_nodes_i));

            node_dump(parser, call.ca_lhs_node_i, indent + 2);
#endif
            return;
        }
    }
}

static int node_first_token(const parser_t* parser, const node_t* node) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");

    switch (node->node_kind) {
        case NODE_BUILTIN_PRINTLN:
            return node->node_n.node_builtin_println.bp_keyword_print_i;
        case NODE_STRING:
            return node->node_n.node_string;
        case NODE_KEYWORD_BOOL:
        case NODE_CHAR:
        case NODE_LONG:
            return node->node_n.node_num.nu_tok_i;
        case NODE_LT:
        case NODE_LE:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_MULTIPLY:
        case NODE_DIVIDE:
        case NODE_MODULO:
        case NODE_SUBTRACT:
        case NODE_ASSIGN:
        case NODE_ADD: {
            const node_t* const lhs =
                &parser->par_nodes[node->node_n.node_binary.bi_lhs_i];
            return node_first_token(parser, lhs);
        }
        case NODE_RETURN:
        case NODE_NOT: {
            return node->node_n.node_unary.un_first_tok_i;
        }
        case NODE_IF:
            return node->node_n.node_if.if_first_tok_i;
        case NODE_BLOCK:
            return node->node_n.node_block.bl_first_tok_i;
        case NODE_VAR_DEF:
            return node->node_n.node_var_def.vd_first_tok_i;
        case NODE_VAR:
            return node->node_n.node_var.va_tok_i;
        case NODE_WHILE:
            return node->node_n.node_while.wh_first_tok_i;
        case NODE_FN_DECL:
            return node->node_n.node_fn_decl.fd_first_tok_i;
        case NODE_CALL:
            return node->node_n.node_call.ca_first_tok_i;
    }
    log_debug("node kind=%d", node->node_kind);
    UNREACHABLE();
}

static int node_last_token(const parser_t* parser, const node_t* node) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    if (node == NULL) return -1;

    switch (node->node_kind) {
        case NODE_BUILTIN_PRINTLN:
            return node->node_n.node_builtin_println.bp_rparen_i;
        case NODE_STRING:
            return node->node_n.node_string;
        case NODE_KEYWORD_BOOL:
        case NODE_LONG:
        case NODE_CHAR:
            return node->node_n.node_num.nu_tok_i;
        case NODE_LT:
        case NODE_LE:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_MULTIPLY:
        case NODE_DIVIDE:
        case NODE_MODULO:
        case NODE_SUBTRACT:
        case NODE_ASSIGN:
        case NODE_ADD: {
            const node_t* const rhs =
                &parser->par_nodes[node->node_n.node_binary.bi_rhs_i];
            return node_first_token(parser, rhs);
        }
        case NODE_RETURN:
        case NODE_NOT: {
            return node->node_n.node_unary.un_last_tok_i;
        }
        case NODE_IF:
            return node->node_n.node_if.if_last_tok_i;
        case NODE_BLOCK:
            return node->node_n.node_block.bl_last_tok_i;
        case NODE_VAR_DEF:
            return node->node_n.node_var_def.vd_last_tok_i;
        case NODE_VAR:
            return node->node_n.node_var.va_tok_i;
        case NODE_WHILE:
            return node->node_n.node_while.wh_last_tok_i;
        case NODE_FN_DECL:
            return node->node_n.node_fn_decl.fd_last_tok_i;
        case NODE_CALL:
            return node->node_n.node_call.ca_last_tok_i;
    }
    log_debug("node kind=%d", node->node_kind);
    UNREACHABLE();
}

static void parser_tok_source(const parser_t* parser, int tok_i,
                              const char** source, int* source_len) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)source_len, !=, NULL, "%p");

    const token_id_t tok = parser->par_lexer.lex_tokens[tok_i].tok_id;
    const pos_range_t pos_range = parser->par_lexer.lex_tok_pos_ranges[tok_i];

    // Without quotes for char/string
    *source = &parser->par_lexer
                   .lex_source[(tok == TOK_ID_STRING || tok == TOK_ID_CHAR)
                                   ? pos_range.pr_start + 1
                                   : pos_range.pr_start];
    *source_len = (tok == TOK_ID_STRING || tok == TOK_ID_CHAR)
                      ? (pos_range.pr_end - pos_range.pr_start - 2)
                      : (pos_range.pr_end - pos_range.pr_start);
}

static bool parser_is_at_end(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    return parser->par_tok_i >= (int)buf_size(parser->par_lexer.lex_tokens);
}

static token_id_t parser_current(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    return parser->par_lexer.lex_tokens[parser->par_tok_i].tok_id;
}

static token_id_t parser_previous(const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_tok_i, >, 1, "%d");
    PG_ASSERT_COND(parser->par_tok_i, <,
                   (int)buf_size(parser->par_lexer.lex_tokens), "%d");

    return parser->par_lexer.lex_tokens[parser->par_tok_i - 1].tok_id;
}
static void parser_advance_until_after(parser_t* parser, token_id_t id) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >, (int)0,
                   "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >,
                   parser->par_tok_i, "%d");

    while (!parser_is_at_end(parser) && parser_current(parser) != id) {
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (int)buf_size(parser->par_lexer.lex_tokens), "%d");
        parser->par_tok_i += 1;
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (int)buf_size(parser->par_lexer.lex_tokens), "%d");
    }

    parser->par_tok_i += 1;
}

static token_id_t parser_peek(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >, (int)0,
                   "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >,
                   parser->par_tok_i, "%d");

    while (parser->par_tok_i < (int)buf_size(parser->par_lexer.lex_tokens)) {
        const token_id_t id =
            parser->par_lexer.lex_tokens[parser->par_tok_i].tok_id;
        if (id == TOK_ID_COMMENT) {
            parser->par_tok_i += 1;
            continue;
        }

        return id;
    }
    UNREACHABLE();
}

static token_id_t parser_peek_next(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >, (int)0,
                   "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >,
                   parser->par_tok_i, "%d");

    int i = parser->par_tok_i;
    while (i < (int)buf_size(parser->par_lexer.lex_tokens) - 1) {
        const token_id_t id = parser->par_lexer.lex_tokens[i + 1].tok_id;
        if (id == TOK_ID_COMMENT) {
            i++;
            continue;
        }

        return id;
    }
    return TOK_ID_EOF;
}

static void parser_print_source_on_error(const parser_t* parser,
                                         int first_tok_i, int last_tok_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const pos_range_t first_tok_pos_range =
        parser->par_lexer.lex_tok_pos_ranges[first_tok_i];
    const loc_t first_tok_loc = parser->par_lexer.lex_locs[first_tok_i];
    const pos_range_t last_tok_pos_range =
        parser->par_lexer.lex_tok_pos_ranges[last_tok_i];
    const loc_t last_tok_loc = parser->par_lexer.lex_locs[last_tok_i];

    const int first_line = first_tok_loc.loc_line;
    const int last_line = last_tok_loc.loc_line;
    PG_ASSERT_COND(first_line, <=, last_line, "%d");

    int first_line_start_tok_i = first_tok_i;
    while (true) {
        if (first_line_start_tok_i < 0 ||
            parser->par_lexer.lex_locs[first_line_start_tok_i].loc_line <
                first_line) {
            first_line_start_tok_i++;
            break;
        }

        first_line_start_tok_i--;
    }

    pos_range_t first_line_start_tok_pos =
        parser->par_lexer.lex_tok_pos_ranges[first_line_start_tok_i];

    int last_line_start_tok_i = last_tok_i;
    while (true) {
        if (last_line_start_tok_i >=
                (int)buf_size(parser->par_lexer.lex_locs) ||
            last_line <
                parser->par_lexer.lex_locs[last_line_start_tok_i].loc_line) {
            last_line_start_tok_i--;
            break;
        }

        last_line_start_tok_i++;
    }

    pos_range_t last_line_start_tok_pos =
        parser->par_lexer.lex_tok_pos_ranges[last_line_start_tok_i];

    const char* source =
        &parser->par_lexer.lex_source[first_line_start_tok_pos.pr_start];
    int source_len =
        last_line_start_tok_pos.pr_end - first_line_start_tok_pos.pr_start;
    trim_end(&source, &source_len);

    static char prefix[MAXPATHLEN + 50] = "\0";
    snprintf(prefix, sizeof(prefix), "%s:%d:%d:", parser->par_file_name0,
             first_tok_loc.loc_line, first_tok_loc.loc_column);
    int prefix_len = strlen(prefix);

    fprintf(stderr, "%s%s%s%.*s\n", parser->par_is_tty ? color_grey : "",
            prefix, parser->par_is_tty ? color_reset : "", (int)source_len,
            source);

    const int source_before_without_squiggly_len =
        first_tok_pos_range.pr_start - first_line_start_tok_pos.pr_start;

    for (int i = 0; i < prefix_len + source_before_without_squiggly_len; i++)
        fprintf(stderr, " ");

    if (parser->par_is_tty) fprintf(stderr, "%s", color_red);

    const int squiggly_len =
        last_tok_pos_range.pr_end - first_tok_pos_range.pr_start;
    for (int i = 0; i < squiggly_len; i++) fprintf(stderr, "^");
    if (parser->par_is_tty) fprintf(stderr, "%s", color_reset);

    fprintf(stderr, "\n");
}

static res_t parser_err_unexpected_token(const parser_t* parser,
                                         token_id_t expected) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const res_t res = RES_UNEXPECTED_TOKEN;

    const loc_t loc_start = parser->par_lexer.lex_locs[parser->par_tok_i];

    fprintf(stderr, "%s%s:%d:%d:%sUnexpected token. Expected `%s`, got `%s`\n",
            (parser->par_is_tty ? color_grey : ""), parser->par_file_name0,
            loc_start.loc_line, loc_start.loc_column,
            (parser->par_is_tty ? color_reset : ""), token_id_to_str[expected],
            token_id_to_str[parser_current(parser)]);

    parser_print_source_on_error(parser, parser->par_tok_i, parser->par_tok_i);

    return res;
}

static bool parser_match(parser_t* parser, int* return_token_index,
                         int id_count, ...) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >, (int)0,
                   "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >,
                   parser->par_tok_i, "%d");

    const token_id_t current_id = parser_peek(parser);

    va_list ap;
    va_start(ap, id_count);

    for (; id_count; id_count--) {
        token_id_t id = va_arg(ap, token_id_t);

        if (parser_is_at_end(parser)) return false;

        if (id != current_id) continue;

        parser_advance_until_after(parser, id);
        PG_ASSERT_COND(parser->par_tok_i, <,
                       (int)buf_size(parser->par_lexer.lex_tokens), "%d");

        *return_token_index = parser->par_tok_i - 1;

        return true;
    }
    va_end(ap);

    return false;
}

static res_t parser_err_non_matching_types(const parser_t* parser,
                                           int lhs_node_i, int rhs_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const node_t* const lhs = &parser->par_nodes[lhs_node_i];
    const type_kind_t lhs_type_kind =
        parser->par_types[lhs->node_type_i].ty_kind;
    const int lhs_first_tok_i = node_first_token(parser, lhs);

    int rhs_type_kind = TYPE_BOOL;
    int rhs_last_tok_i = node_last_token(parser, lhs);
    if (rhs_node_i >= 0) {
        const node_t* const rhs = &parser->par_nodes[rhs_node_i];
        rhs_type_kind = parser->par_types[rhs->node_type_i].ty_kind;
        rhs_last_tok_i = node_last_token(parser, rhs);
    }

    const loc_t lhs_first_tok_loc = parser->par_lexer.lex_locs[lhs_first_tok_i];

    const res_t res = RES_NON_MATCHING_TYPES;
    fprintf(stderr, "%s%s:%d:%d:%sTypes do not match. Expected %s, got %s\n",
            (parser->par_is_tty ? color_grey : ""), parser->par_file_name0,
            lhs_first_tok_loc.loc_line, lhs_first_tok_loc.loc_column,
            (parser->par_is_tty ? color_reset : ""), type_to_str[rhs_type_kind],
            type_to_str[lhs_type_kind]);

    parser_print_source_on_error(parser, lhs_first_tok_i, rhs_last_tok_i);

    return res;
}

static res_t parser_err_unexpected_type(const parser_t* parser, int lhs_node_i,
                                        type_kind_t expected_type_kind) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const node_t* const lhs = &parser->par_nodes[lhs_node_i];

    const type_kind_t lhs_type_kind =
        parser->par_types[lhs->node_type_i].ty_kind;

    const int lhs_first_tok_i = node_first_token(parser, lhs);
    const int lhs_last_tok_i = node_last_token(parser, lhs);

    const loc_t lhs_first_tok_loc = parser->par_lexer.lex_locs[lhs_first_tok_i];

    const res_t res = RES_NON_MATCHING_TYPES;
    fprintf(stderr, "%s%s:%d:%d:%sTypes do not match. Expected %s, got %s\n",
            (parser->par_is_tty ? color_grey : ""), parser->par_file_name0,
            lhs_first_tok_loc.loc_line, lhs_first_tok_loc.loc_column,
            (parser->par_is_tty ? color_reset : ""),
            type_to_str[expected_type_kind], type_to_str[lhs_type_kind]);

    parser_print_source_on_error(parser, lhs_first_tok_i, lhs_last_tok_i);

    return res;
}

static res_t parser_parse_if_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int first_tok_i = -1, last_tok_i = -1, dummy = -1;
    if (!parser_match(parser, &first_tok_i, 1, TOK_ID_IF))
        return parser_err_unexpected_token(parser, TOK_ID_IF);

    res_t res = RES_NONE;

    if (!parser_match(parser, &first_tok_i, 1, TOK_ID_LPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

    int node_cond_i = -1, node_then_i = -1, node_else_i = -1;
    if ((res = parser_parse_expr(parser, &node_cond_i)) != RES_OK) {
        log_debug("failed to parse if-cond %d", res);
        return res;
    }
    PG_ASSERT_COND(node_cond_i, >=, 0, "%d");

    const node_t* const node_cond = &parser->par_nodes[node_cond_i];
    const type_kind_t cond_type_kind =
        parser->par_types[node_cond->node_type_i].ty_kind;
    if (cond_type_kind != TYPE_BOOL) {
        log_debug("if-cond type is not bool, got %s",
                  type_to_str[cond_type_kind]);
        return parser_err_unexpected_type(parser, node_cond_i, TYPE_BOOL);
    }

    if (!parser_match(parser, &dummy, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    const int current_scope_i = parser->par_scope_i;
    if ((res = parser_parse_control_structure_body(parser, &node_then_i)) !=
        RES_OK) {
        log_debug("failed to parse if-branch %d", res);
        return res;
    }
    PG_ASSERT_COND(node_then_i, >=, 0, "%d");

    const node_t* const node_then = &parser->par_nodes[node_then_i];
    const int then_type_i = node_then->node_type_i;
    const type_kind_t then_type_kind = parser->par_types[then_type_i].ty_kind;

    // Else is optional

    if (parser_match(parser, &dummy, 1, TOK_ID_ELSE)) {
        if ((res = parser_parse_control_structure_body(parser, &node_else_i)) !=
            RES_OK) {
            log_debug("failed to parse else-branch %d", res);
            return res;
        }

        const node_t* const node_else = &parser->par_nodes[node_else_i];
        const type_kind_t else_type_kind =
            parser->par_types[node_else->node_type_i].ty_kind;
        // Unit gets a pass for now (until we have assignements)
        if (then_type_kind != else_type_kind && then_type_kind != TYPE_UNIT &&
            else_type_kind != TYPE_UNIT) {
            log_debug("if branch types don't match, got %s and %s",
                      type_to_str[then_type_kind], type_to_str[else_type_kind]);
            return parser_err_non_matching_types(parser, node_then_i,
                                                 node_else_i);
        }
    }
    const node_t new_node = NODE_IF(then_type_i, first_tok_i, last_tok_i,
                                    node_cond_i, node_then_i, node_else_i);

    buf_push(parser->par_nodes, new_node);
    *new_node_i = (int)buf_size(parser->par_nodes) - 1;

    // Reset current scope
    parser->par_scope_i = current_scope_i;

    return RES_OK;
}

static res_t parser_parse_jump_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int tok_i = -1;
    res_t res = RES_NONE;
    if (parser_match(parser, &tok_i, 1, TOK_ID_RETURN)) {
        int expr_node_i = -1;
        res = parser_parse_expr(parser, &expr_node_i);
        if (res != RES_OK && res != RES_NONE) return res;

        const int type_i = expr_node_i >= 0
                               ? parser->par_nodes[expr_node_i].node_type_i
                               : TYPE_UNIT_I;

        const node_t* const expr =
            expr_node_i >= 0 ? &parser->par_nodes[expr_node_i] : NULL;
        buf_push(parser->par_nodes,
                 ((node_t){
                     .node_type_i = type_i,
                     .node_kind = NODE_RETURN,
                     .node_n = {.node_unary = {.un_first_tok_i = tok_i,
                                               .un_last_tok_i = node_last_token(
                                                   parser, expr),
                                               .un_node_i = expr_node_i}}}));
        *new_node_i = buf_size(parser->par_nodes) - 1;
        const type_kind_t type_kind = parser->par_types[type_i].ty_kind;
        log_debug("New return %d type=%s", *new_node_i, type_to_str[type_kind]);

        parser->par_nodes[parser->par_fn_i].node_n.node_fn_decl.fd_flags |=
            FN_FLAGS_SEEN_RETURN;

        return RES_OK;
    }

    return RES_NONE;
}

static res_t parser_parse_primary_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int tok_i = -1;
    res_t res = RES_NONE;

    // FIXME: hack
    if (parser_peek(parser) == TOK_ID_BUILTIN_PRINTLN)
        return parser_parse_builtin_println(parser, new_node_i);

    int lparen_tok_i = -1, rparen_tok_i = -1;
    if (parser_match(parser, &lparen_tok_i, 1, TOK_ID_LPAREN)) {
        res = parser_parse_expr(parser, new_node_i);
        if (res == RES_NONE)
            return parser_err_missing_rhs(parser, lparen_tok_i,
                                          parser->par_tok_i);
        else if (res != RES_OK)
            return res;

        if (!parser_match(parser, &rparen_tok_i, 1, TOK_ID_RPAREN))
            return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 2, TOK_ID_TRUE, TOK_ID_FALSE)) {
        const int type_i = parser_make_type(parser, TYPE_BOOL);

        const pos_range_t pos_range =
            parser->par_lexer.lex_tok_pos_ranges[tok_i];
        const char* const source =
            &parser->par_lexer.lex_source[pos_range.pr_start];
        // The source is either `true` or `false` hence the len is either 4
        // or 5
        const int8_t val = (memcmp("true", source, 4) == 0);
        const node_t new_node = NODE_LONG(tok_i, type_i, val);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_STRING)) {
        const int type_i = parser_make_type(parser, TYPE_STRING);

        const node_t node = {.node_kind = NODE_STRING,
                             .node_type_i = type_i,
                             .node_n = {.node_string = tok_i}};
        buf_push(parser->par_nodes, node);
        *new_node_i = buf_size(parser->par_nodes) - 1;

        buf_push(parser->par_node_decls, *new_node_i);

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_LONG)) {
        const int type_i = parser_make_type(parser, TYPE_LONG);

        const long long int val = parse_tok_to_long(parser, tok_i);
        const node_t new_node = NODE_LONG(tok_i, type_i, val);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_CHAR)) {
        const int type_i = parser_make_type(parser, TYPE_CHAR);

        const long long int val = parse_tok_to_char(parser, tok_i);
        const node_t new_node = NODE_CHAR(tok_i, type_i, val);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_IDENTIFIER)) {
        int node_def_i = -1;
        if (parser_resolve_var(parser, tok_i, &node_def_i) != RES_OK) {
            return RES_UNKNOWN_VAR;
        }

        const node_t* const node_def = &parser->par_nodes[node_def_i];
        const int type_i = node_def->node_type_i;

        const node_t new_node = NODE_VAR(type_i, tok_i, node_def_i);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_peek(parser) == TOK_ID_IF)
        return parser_parse_if_expr(parser, new_node_i);
    if (parser_peek(parser) == TOK_ID_RETURN)
        return parser_parse_jump_expr(parser, new_node_i);

    return RES_NONE;  // TODO
}

static res_t parser_parse_postfix_unary_expr(parser_t* parser,
                                             int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    res_t res = parser_parse_primary_expr(parser, new_node_i);
    if (res != RES_OK) return res;

    // Optional
    res = parser_parse_call_suffix(parser, new_node_i);
    if (res == RES_NONE) return RES_OK;

    if (res != RES_OK) return res;

    return RES_OK;
}

static res_t parser_parse_prefix_unary_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    res_t res = RES_NONE;

    int tok_i = -1;
    if (parser_match(parser, &tok_i, 1, TOK_ID_NOT)) {
        int node_i = -1;

        if ((res = parser_parse_postfix_unary_expr(parser, &node_i)) !=
            RES_OK) {
            return res;
        }

        const int type_i = parser->par_nodes[node_i].node_type_i;
        const type_kind_t type_kind = parser->par_types[type_i].ty_kind;
        if (type_kind != TYPE_BOOL) {
            return parser_err_unexpected_type(parser, node_i, TYPE_BOOL);
        }

        const node_t* const node = &parser->par_nodes[node_i];

        buf_push(parser->par_nodes,
                 ((node_t){
                     .node_type_i = type_i,
                     .node_kind = NODE_NOT,
                     .node_n = {.node_unary = {.un_first_tok_i = tok_i,
                                               .un_last_tok_i = node_last_token(
                                                   parser, node),
                                               .un_node_i = node_i}}}));
        *new_node_i = node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return parser_parse_postfix_unary_expr(parser, new_node_i);
}

static res_t parser_parse_as_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_prefix_unary_expr(parser, new_node_i);
}

static res_t parser_parse_multiplicative_expr(parser_t* parser,
                                              int* new_node_i) {
    res_t res = RES_NONE;

    int lhs_i = -1;
    if ((res = parser_parse_as_expr(parser, &lhs_i)) != RES_OK) return res;
    PG_ASSERT_COND(lhs_i, >=, 0, "%d");

    const int lhs_type_i = parser->par_nodes[lhs_i].node_type_i;

    const type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;

    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 3, TOK_ID_STAR, TOK_ID_SLASH,
                        TOK_ID_PERCENT)) {
        if (lhs_type_kind != TYPE_LONG && lhs_type_kind != TYPE_INT &&
            lhs_type_kind != TYPE_SHORT && lhs_type_kind != TYPE_BYTE) {
            log_debug("non matching types: lhs should be numerical, was: %s",
                      type_to_str[lhs_type_kind]);
            return parser_err_unexpected_type(parser, lhs_i, TYPE_LONG);
        }

        const int tok_i = parser->par_tok_i - 1;
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        res = parser_parse_as_expr(parser, &rhs_i);
        if (res == RES_NONE) {
            return parser_err_missing_rhs(parser, tok_i, parser->par_tok_i);
        } else if (res != RES_OK) {
            return res;
        }
        PG_ASSERT_COND(rhs_i, >=, 0, "%d");

        const int rhs_type_i = parser->par_nodes[rhs_i].node_type_i;
        const type_kind_t rhs_type_kind = parser->par_types[rhs_type_i].ty_kind;

        if (rhs_type_kind != TYPE_LONG && rhs_type_kind != TYPE_INT &&
            rhs_type_kind != TYPE_SHORT && rhs_type_kind != TYPE_BYTE) {
            log_debug("non matching types: rhs should be numerical, was: %s",
                      type_to_str[rhs_type_kind]);
            return parser_err_unexpected_type(parser, lhs_i, TYPE_LONG);
        }

        if (lhs_type_kind != rhs_type_kind) {
            log_debug("non matching types: lhs=%s rhs=%s",
                      type_to_str[lhs_type_kind], type_to_str[rhs_type_kind]);
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);
        }

        const node_t new_node = NODE_BINARY(
            (tok_id == TOK_ID_STAR)
                ? NODE_MULTIPLY
                : (tok_id == TOK_ID_SLASH ? NODE_DIVIDE : NODE_MODULO),
            lhs_i, rhs_i, lhs_type_i);

        buf_push(parser->par_nodes, new_node);
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return res;
}

static res_t parser_parse_additive_expr(parser_t* parser, int* new_node_i) {
    res_t res = RES_NONE;

    int lhs_i = -1;
    if ((res = parser_parse_multiplicative_expr(parser, &lhs_i)) != RES_OK)
        return res;
    const int lhs_type_i = parser->par_nodes[lhs_i].node_type_i;
    const type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 2, TOK_ID_PLUS, TOK_ID_MINUS)) {
        const int tok_i = parser->par_tok_i - 1;
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        res = parser_parse_multiplicative_expr(parser, &rhs_i);
        if (res == RES_NONE) {
            return parser_err_missing_rhs(parser, tok_i, parser->par_tok_i);
        } else if (res != RES_OK) {
            return res;
        }

        const int rhs_type_i = parser->par_nodes[rhs_i].node_type_i;
        const type_kind_t rhs_type_kind = parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        const node_t new_node =
            NODE_BINARY(tok_id == TOK_ID_PLUS ? NODE_ADD : NODE_SUBTRACT, lhs_i,
                        rhs_i, lhs_type_i);

        buf_push(parser->par_nodes, new_node);
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return res;
}

static res_t parser_parse_range_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_additive_expr(parser, new_node_i);
}

static res_t parser_parse_infix_fn_call(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_range_expr(parser, new_node_i);
}

static res_t parser_parse_elvis_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_infix_fn_call(parser, new_node_i);
}

static res_t parser_parse_infix_op(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_elvis_expr(parser, new_node_i);
}

static res_t parser_parse_value_arg(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_expr(parser, new_node_i);
}

static res_t parser_parse_value_args(parser_t* parser, int** arg_nodes_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)arg_nodes_i, !=, NULL, "%p");

    if (parser_peek(parser) != TOK_ID_LPAREN) return RES_NONE;

    int dummy = -1;
    parser_match(parser, &dummy, 1, TOK_ID_LPAREN);

    if (parser_match(parser, &dummy, 1, TOK_ID_RPAREN)) return RES_OK;

    do {
        int new_node_i = -1;
        res_t res = parser_parse_value_arg(parser, &new_node_i);
        if (res == RES_OK) {
            buf_push(*arg_nodes_i, new_node_i);
        } else if (res == RES_NONE)
            break;
        else
            return res;
    } while (parser_match(parser, &dummy, 1, TOK_ID_COMMA));

    if (!parser_match(parser, &dummy, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    return RES_OK;
}

static res_t parser_parse_call_suffix(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int* arg_nodes_i = NULL;
    res_t res = parser_parse_value_args(parser, &arg_nodes_i);
    if (res != RES_OK) return res;

    const int type_i = parser->par_nodes[*new_node_i].node_type_i;

    const int fn_decl_node_i =
        parser_node_find_fn_decl_for_call(parser, *new_node_i);
    const node_t* const fn_decl_node = &parser->par_nodes[fn_decl_node_i];
    const fn_decl_t fn_decl = fn_decl_node->node_n.node_fn_decl;
    const int declared_arity = buf_size(fn_decl.fd_arg_nodes_i);
    const int found_arity = buf_size(arg_nodes_i);
    if (declared_arity != found_arity) UNIMPLEMENTED();  // TODO: err

    const int current_scope_i =
        parser_enter_scope(parser, fn_decl.fd_body_node_i);
    for (int i = 0; i < (int)buf_size(fn_decl.fd_arg_nodes_i); i++) {
        const int decl_arg_i = fn_decl.fd_arg_nodes_i[i];
        const node_t* const decl_arg = &parser->par_nodes[decl_arg_i];
        const type_kind_t decl_type_kind =
            parser->par_types[decl_arg->node_type_i].ty_kind;
        const int found_arg_i = arg_nodes_i[i];
        const node_t* const found_arg = &parser->par_nodes[found_arg_i];
        const type_kind_t found_type_kind =
            parser->par_types[found_arg->node_type_i].ty_kind;

        if (decl_type_kind != found_type_kind)
            return parser_err_non_matching_types(parser, decl_arg_i,
                                                 found_arg_i);

        buf_push(parser->par_nodes,
                 ((node_t){
                     .node_kind = NODE_ASSIGN,
                     .node_type_i = TYPE_UNIT_I,
                     .node_n = {.node_binary = {.bi_lhs_i = decl_arg_i,
                                                .bi_rhs_i = arg_nodes_i[i]}}}));
    }
    parser_leave_scope(parser, current_scope_i);

    buf_push(
        parser->par_nodes,
        ((node_t){.node_type_i = type_i,
                  .node_kind = NODE_CALL,
                  .node_n = {.node_call = {.ca_arg_nodes_i = arg_nodes_i,
                                           .ca_lhs_node_i = *new_node_i}}}));
    *new_node_i = buf_size(parser->par_nodes) - 1;

    return RES_OK;
}

static res_t parser_parse_generical_call_like_comparison(parser_t* parser,
                                                         int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_infix_op(parser, new_node_i);
}

static res_t parser_parse_comparison(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    res_t res = RES_NONE;

    int lhs_i = -1;
    if ((res = parser_parse_generical_call_like_comparison(parser, &lhs_i)) !=
        RES_OK)
        return res;
    const int lhs_type_i = parser->par_nodes[lhs_i].node_type_i;
    const type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 4, TOK_ID_LT, TOK_ID_LE, TOK_ID_GT,
                        TOK_ID_GE)) {
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        if ((res = parser_parse_generical_call_like_comparison(
                 parser, &rhs_i)) != RES_OK)
            return res;

        const int rhs_type_i = parser->par_nodes[rhs_i].node_type_i;
        const type_kind_t rhs_type_kind = parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        const int type_i = parser_make_type(parser, TYPE_BOOL);

        node_t new_node;

        if (tok_id == TOK_ID_LT)
            new_node = NODE_BINARY(NODE_LT, lhs_i, rhs_i, type_i);
        else if (tok_id == TOK_ID_LE)
            new_node = NODE_BINARY(NODE_LE, lhs_i, rhs_i, type_i);
        else if (tok_id == TOK_ID_GE)
            new_node = NODE_BINARY(NODE_LE, rhs_i, lhs_i, type_i);
        else if (tok_id == TOK_ID_GT)
            new_node = NODE_BINARY(NODE_LT, rhs_i, lhs_i, type_i);
        else
            UNREACHABLE();

        buf_push(parser->par_nodes, new_node);
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return res;
}

static res_t parser_parse_equality(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    res_t res = RES_NONE;

    int lhs_i = -1;
    if ((res = parser_parse_comparison(parser, &lhs_i)) != RES_OK) return res;
    const int lhs_type_i = parser->par_nodes[lhs_i].node_type_i;
    const type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 2, TOK_ID_EQ_EQ, TOK_ID_NEQ)) {
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        if ((res = parser_parse_comparison(parser, &rhs_i)) != RES_OK)
            return res;

        const int rhs_type_i = parser->par_nodes[rhs_i].node_type_i;
        const type_kind_t rhs_type_kind = parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        const int type_i = parser_make_type(parser, TYPE_BOOL);

        node_t new_node;

        new_node = NODE_BINARY(tok_id == TOK_ID_EQ_EQ ? NODE_EQ : NODE_NEQ,
                               lhs_i, rhs_i, type_i);

        buf_push(parser->par_nodes, new_node);
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return res;
}

static res_t parser_parse_conjunction(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_equality(parser, new_node_i);
}

static res_t parser_parse_disjunction(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_conjunction(parser, new_node_i);
}

static res_t parser_parse_expr(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_disjunction(parser, new_node_i);
}

static res_t parser_parse_stmts(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    res_t res = RES_NONE;
    while (1) {
        int new_node_i = -1;
        res = parser_parse_stmt(parser, &new_node_i);
        if (res == RES_OK) {
            buf_push(parser->par_nodes[parser->par_scope_i]
                         .node_n.node_block.bl_nodes_i,
                     new_node_i);
            continue;
        } else if (res == RES_NONE)
            return RES_OK;
        else {
            log_debug("failed to parse stmts: res=%d", res);
            return res;
        }
    }
}

static res_t parser_parse_block(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    buf_push(parser->par_nodes,
             ((node_t){.node_kind = NODE_BLOCK,
                       .node_type_i = TYPE_ANY_I,
                       .node_n = {.node_block = {.bl_first_tok_i = -1,
                                                 .bl_last_tok_i = -1,
                                                 .bl_nodes_i = NULL,
                                                 .bl_parent_scope_i =
                                                     parser->par_scope_i}}}));
    *new_node_i = buf_size(parser->par_nodes) - 1;
    const int parent_scope_i = parser_enter_scope(parser, *new_node_i);

    if (!parser_match(
            parser,
            &parser->par_nodes[*new_node_i].node_n.node_block.bl_first_tok_i, 1,
            TOK_ID_LCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_LCURLY);

    res_t res = RES_NONE;
    if ((res = parser_parse_stmts(parser)) != RES_OK) {
        log_debug("failed to parse expr in optional curlies %d", res);
        return res;
    }

    if (!parser_match(
            parser,
            &parser->par_nodes[*new_node_i].node_n.node_block.bl_last_tok_i, 1,
            TOK_ID_RCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_RCURLY);

    const int* const nodes_i =
        parser->par_nodes[parser->par_scope_i].node_n.node_block.bl_nodes_i;
    const int last_node_i =
        buf_size(nodes_i) > 0 ? nodes_i[buf_size(nodes_i) - 1] : -1;
    const int type_i = last_node_i >= 0
                           ? parser->par_nodes[last_node_i].node_type_i
                           : TYPE_UNIT_I;

    parser->par_nodes[*new_node_i].node_type_i = type_i;

    parser_leave_scope(parser, parent_scope_i);

    const type_kind_t type_kind = parser->par_types[type_i].ty_kind;
    log_debug("block=%d parent=%d last_node_i=%d type=%s last_tok_i=%d",
              *new_node_i, parent_scope_i, last_node_i, type_to_str[type_kind],
              parser->par_nodes[*new_node_i].node_n.node_block.bl_last_tok_i);

    return res;
}

static res_t parser_parse_control_structure_body(parser_t* parser,
                                                 int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) == TOK_ID_LCURLY)
        return parser_parse_block(parser, new_node_i);

    return parser_parse_stmt(parser, new_node_i);
}

static res_t parser_parse_builtin_println(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int keyword_print_i = -1;
    res_t res = RES_NONE;
    if (parser_match(parser, &keyword_print_i, 1, TOK_ID_BUILTIN_PRINTLN)) {
        int lparen = 0;
        if (!parser_match(parser, &lparen, 1, TOK_ID_LPAREN))
            return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

        int arg_i = 0;
        res = parser_parse_expr(parser, &arg_i);
        if (res == RES_NONE) {
            fprintf(stderr, "Missing println parameter\n");
            return RES_MISSING_PARAM;
        } else if (res != RES_OK)
            return res;
        int rparen = 0;
        if (!parser_match(parser, &rparen, 1, TOK_ID_RPAREN))
            return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

        const node_t new_node =
            NODE_PRINTLN(arg_i, keyword_print_i, rparen, TYPE_UNIT_I);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    return RES_NONE;
}

static res_t parser_parse_assignment(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    res_t res = RES_NONE;
    int dummy = -1, expr_node_i = -1, lhs_tok_i = -1;
    if (parser_peek(parser) == TOK_ID_IDENTIFIER &&
        parser_peek_next(parser) == TOK_ID_EQ) {
        parser_match(parser, &lhs_tok_i, 1, TOK_ID_IDENTIFIER);

        int node_def_i = -1;
        if (parser_resolve_var(parser, lhs_tok_i, &node_def_i) != RES_OK) {
            return RES_UNKNOWN_VAR;
        }

        const node_t* const node_def = &parser->par_nodes[node_def_i];
        if (node_def->node_kind != NODE_VAR_DEF) {
            UNIMPLEMENTED();  // err
        }

        const var_def_t var_def = node_def->node_n.node_var_def;
        if (var_def.vd_flags & VAR_FLAGS_VAL)
            return parser_err_assigning_val(parser, lhs_tok_i, &var_def);

        const int type_i = node_def->node_type_i;

        const node_t var_node = NODE_VAR(type_i, lhs_tok_i, node_def_i);
        buf_push(parser->par_nodes, var_node);
        int lhs_node_i = (int)buf_size(parser->par_nodes) - 1;

        parser_match(parser, &dummy, 1, TOK_ID_EQ);

        res = parser_parse_expr(parser, &expr_node_i);
        if (res == RES_NONE) {
            log_debug("Missing assignment rhs %d", lhs_node_i);
            return RES_EXPECTED_PRIMARY;
        } else if (res != RES_OK)
            return res;

        const int lhs_type_i = parser->par_nodes[lhs_node_i].node_type_i;
        const int rhs_type_i = parser->par_nodes[expr_node_i].node_type_i;
        const type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
        const type_kind_t rhs_type_kind = parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind) {
            return parser_err_non_matching_types(parser, expr_node_i,
                                                 lhs_node_i);
        }

        const node_t new_node =
            NODE_BINARY(NODE_ASSIGN, lhs_node_i, expr_node_i, lhs_type_i);
        buf_push(parser->par_nodes, new_node);
        *new_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return RES_NONE;
}

static res_t parser_parse_property_declaration(parser_t* parser,
                                               int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    if (!(parser_peek(parser) == TOK_ID_VAL ||
          parser_peek(parser) == TOK_ID_VAR))
        return RES_NONE;

    int first_tok_i = -1, name_tok_i = -1, type_tok_i = -1, dummy = -1,
        last_tok_i = -1, init_node_i = -1;

    unsigned short flags = 0;
    if (parser_match(parser, &first_tok_i, 1, TOK_ID_VAR))
        flags = VAR_FLAGS_VAR;
    else if (parser_match(parser, &first_tok_i, 1, TOK_ID_VAL))
        flags = VAR_FLAGS_VAL;
    else
        return RES_NONE;

    if (!parser_match(parser, &name_tok_i, 1, TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

    if (!parser_match(parser, &dummy, 1, TOK_ID_COLON))
        return parser_err_unexpected_token(parser, TOK_ID_COLON);

    if (!parser_match(parser, &type_tok_i, 1, TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);
    PG_ASSERT_COND(type_tok_i, >=, 0, "%d");

    if (!parser_match(parser, &dummy, 1, TOK_ID_EQ))
        return parser_err_unexpected_token(parser, TOK_ID_EQ);

    res_t res = RES_NONE;
    res = parser_parse_expr(parser, &init_node_i);
    if (res == RES_NONE) {
        log_debug("var def without initial value%s", "");
        UNIMPLEMENTED();  // TODO make meaningful error
    }
    if (res != RES_OK) return res;

    type_kind_t type_kind = TYPE_ANY;
    if (!parser_parse_identifier_to_type_kind(parser, type_tok_i, &type_kind)) {
        log_debug("user types not yet supported: type_tok_i=%d", type_tok_i);
        UNIMPLEMENTED();
    }
    const int type_i = parser_make_type(parser, type_kind);
    const int type_size = parser->par_types[type_i].ty_size;
    log_debug("parsed type %s size=%d offset=%d", type_to_str[type_kind],
              type_size, parser->par_offset);

    parser->par_offset += type_size;

    const node_t new_node =
        NODE_VAR_DEF(type_i, first_tok_i, name_tok_i, last_tok_i, init_node_i,
                     parser->par_offset, flags);
    buf_push(parser->par_nodes, new_node);
    *new_node_i = buf_size(parser->par_nodes) - 1;

    log_debug("new var def=%d current_scope_i=%d flags=%d offset=%d",
              *new_node_i, parser->par_scope_i, flags, parser->par_offset);

    return RES_OK;
}

static res_t parser_parse_parameter(parser_t* parser, int** new_nodes_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_nodes_i, !=, NULL, "%p");

    int identifier_tok_i = -1, dummy = -1, type_tok_i = -1;

    if (parser_match(parser, &dummy, 1, TOK_ID_RPAREN)) return RES_OK;

    int offset = 0;

    do {
        if (!parser_match(parser, &identifier_tok_i, 1, TOK_ID_IDENTIFIER))
            return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

        if (!parser_match(parser, &dummy, 1, TOK_ID_COLON))
            return parser_err_unexpected_token(parser, TOK_ID_COLON);

        if (!parser_match(parser, &type_tok_i, 1, TOK_ID_IDENTIFIER))
            return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

        type_kind_t type_kind = TYPE_UNIT;
        if (!parser_parse_identifier_to_type_kind(parser, type_tok_i,
                                                  &type_kind))
            UNIMPLEMENTED();

        const int type_i = parser_make_type(parser, type_kind);
        const int type_size = parser->par_types[type_i].ty_size;
        offset += type_size;

        buf_push(parser->par_nodes,
                 ((node_t){.node_kind = NODE_VAR_DEF,
                           .node_type_i = type_i,
                           .node_n = {.node_var_def = {
                                          .vd_name_tok_i = identifier_tok_i,
                                          .vd_first_tok_i = identifier_tok_i,
                                          .vd_last_tok_i = type_tok_i,
                                          .vd_init_node_i = -1,
                                          .vd_stack_offset = offset,
                                          .vd_flags = VAR_FLAGS_VAR}}}));
        const int new_node_i = buf_size(parser->par_nodes) - 1;
        buf_push(*new_nodes_i, new_node_i);

        const char* source = NULL;
        int source_len = 0;
        parser_tok_source(parser, identifier_tok_i, &source, &source_len);
        log_debug("New fn param: `%.*s` type=%s scope=%d offset=%d", source_len,
                  source, type_to_str[type_kind], parser->par_scope_i, offset);
    } while (parser_match(parser, &dummy, 1, TOK_ID_COMMA));

    if (!parser_match(parser, &dummy, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    return RES_OK;
}

static res_t parser_parse_fn_value_params(parser_t* parser, int** new_nodes_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_nodes_i, !=, NULL, "%p");

    int dummy = -1;
    if (!parser_match(parser, &dummy, 1, TOK_ID_LPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

    res_t res = RES_NONE;
    if ((res = parser_parse_parameter(parser, new_nodes_i)) != RES_OK)
        return res;

    return RES_OK;
}

static res_t parser_parse_fn_declaration(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) != TOK_ID_FUN) return RES_NONE;

    int first_tok_i = -1, dummy = -1, *arg_nodes_i = NULL;

    parser_match(parser, &first_tok_i, 1, TOK_ID_FUN);

    buf_push(
        parser->par_nodes,
        ((node_t){.node_type_i = TYPE_UNIT_I,
                  .node_kind = NODE_FN_DECL,
                  .node_n = {.node_fn_decl = {.fd_first_tok_i = first_tok_i,
                                              .fd_name_tok_i = -1,
                                              .fd_last_tok_i = -1,
                                              .fd_body_node_i = -1,
                                              .fd_flags = FN_FLAGS_PRIVATE,
                                              .fd_arg_nodes_i = NULL}}}));
    parser->par_fn_i = *new_node_i = buf_size(parser->par_nodes) - 1;
    buf_push(parser->par_node_decls, *new_node_i);

    buf_push(
        parser->par_nodes[parser->par_scope_i].node_n.node_block.bl_nodes_i,
        *new_node_i);

    if (!parser_match(
            parser,
            &parser->par_nodes[*new_node_i].node_n.node_fn_decl.fd_name_tok_i,
            1, TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

    buf_push(parser->par_nodes,
             ((node_t){.node_kind = NODE_BLOCK,
                       .node_type_i = TYPE_UNIT_I,
                       .node_n = {.node_block = {.bl_first_tok_i = -1,
                                                 .bl_last_tok_i = -1,
                                                 .bl_nodes_i = NULL,
                                                 .bl_parent_scope_i =
                                                     parser->par_scope_i}}}));
    const int body_node_i =
        parser->par_nodes[*new_node_i].node_n.node_fn_decl.fd_body_node_i =
            buf_size(parser->par_nodes) - 1;
    const int parent_scope_i = parser_enter_scope(parser, body_node_i);

    res_t res = RES_NONE;
    if ((res = parser_parse_fn_value_params(parser, &arg_nodes_i)) != RES_OK)
        return res;

    parser->par_nodes[body_node_i].node_n.node_block.bl_nodes_i = arg_nodes_i;

    for (int i = 0; i < (int)buf_size(arg_nodes_i); i++)
        buf_push(
            parser->par_nodes[*new_node_i].node_n.node_fn_decl.fd_arg_nodes_i,
            arg_nodes_i[i]);

    int declared_type_tok_i = -1, declared_type_i = -1;
    type_kind_t declared_type_kind = -1;
    if (parser_match(parser, &dummy, 1, TOK_ID_COLON)) {
        if (!parser_match(parser, &declared_type_tok_i, 1, TOK_ID_IDENTIFIER)) {
            return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);
        }
        if (!parser_parse_identifier_to_type_kind(parser, declared_type_tok_i,
                                                  &declared_type_kind)) {
            parser_print_source_on_error(parser, declared_type_tok_i,
                                         declared_type_tok_i);
            log_debug(
                "Encountered user type in function signature, not yet "
                "implemented:%s",
                "");
            UNIMPLEMENTED();
        }
        declared_type_i = parser_make_type(parser, declared_type_kind);
    } else
        declared_type_i = TYPE_UNIT_I;

    parser->par_nodes[*new_node_i].node_type_i = declared_type_i;

    if (!parser_match(
            parser,
            &parser->par_nodes[body_node_i].node_n.node_block.bl_first_tok_i, 1,
            TOK_ID_LCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_LCURLY);

    res = parser_parse_stmts(parser);
    if (res != RES_OK) return res;
    parser->par_nodes[body_node_i].node_type_i =
        parser->par_nodes[buf_size(parser->par_nodes) - 1].node_type_i;

    if (!parser_match(
            parser,
            &parser->par_nodes[body_node_i].node_n.node_block.bl_last_tok_i, 1,
            TOK_ID_RCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_RCURLY);

    const int actual_type_i = parser->par_nodes[body_node_i].node_type_i;
    const type_kind_t actual_type = parser->par_types[actual_type_i].ty_kind;
    const type_kind_t declared_type =
        parser->par_types[declared_type_i].ty_kind;

    fn_decl_t* const fn_decl =
        &parser->par_nodes[*new_node_i].node_n.node_fn_decl;
    const int last_tok_i =
        parser->par_nodes[body_node_i].node_n.node_block.bl_last_tok_i;
    fn_decl->fd_last_tok_i = last_tok_i;

    log_debug("new fn decl=%d flags=%d body_node_i=%d type=%s arity=%d",
              *new_node_i, fn_decl->fd_flags, fn_decl->fd_body_node_i,
              type_to_str[declared_type],
              (int)buf_size(fn_decl->fd_arg_nodes_i));

    parser_leave_scope(parser, parent_scope_i);

    const bool seen_return = fn_decl->fd_flags & FN_FLAGS_SEEN_RETURN;
    if (declared_type != TYPE_UNIT && !seen_return) {
        const loc_t loc = parser->par_lexer.lex_locs[last_tok_i];

        fprintf(
            stderr,
            "%s%s:%d:%d:%sThe function has declared to "
            "return %s but has no return%s\n",
            parser->par_is_tty ? color_grey : "", parser->par_file_name0,
            loc.loc_line, loc.loc_column, parser->par_is_tty ? color_reset : "",
            type_to_str[declared_type], parser->par_is_tty ? color_reset : "");
        parser_print_source_on_error(parser, last_tok_i, last_tok_i);

        return RES_ERR;
    }
    if (declared_type == TYPE_UNIT && actual_type != TYPE_UNIT && !seen_return)
        return RES_OK;

    if (actual_type != declared_type)
        return parser_err_non_matching_types(
            parser, body_node_i,
            *new_node_i);  // TODO: implement custom error function

    return RES_OK;
}

static res_t parser_parse_declaration(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    res_t res = RES_NONE;

    if ((res = parser_parse_fn_declaration(parser, new_node_i)) != RES_NONE)
        return res;

    return parser_parse_property_declaration(parser, new_node_i);
}

static res_t parser_parse_while_stmt(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    int dummy = -1, first_tok_i = -1, last_tok_i = -1, cond_i = -1, body_i = -1;

    if (!parser_match(parser, &first_tok_i, 1, TOK_ID_WHILE)) return RES_NONE;

    if (!parser_match(parser, &dummy, 1, TOK_ID_LPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

    res_t res = parser_parse_expr(parser, &cond_i);
    if (res == RES_NONE) {
        log_debug("Missing while condition%s", "\n");
        return RES_ERR;
    } else if (res != RES_OK)
        return res;

    const int cond_type_i = parser->par_nodes[cond_i].node_type_i;
    const type_kind_t cond_type_kind = parser->par_types[cond_type_i].ty_kind;
    if (cond_type_kind != TYPE_BOOL) {
        return parser_err_non_matching_types(parser, cond_type_i, -1);
    }

    if (!parser_match(parser, &dummy, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    res = parser_parse_control_structure_body(parser, &body_i);
    if (res == RES_NONE) {
        log_debug("Missing while body%s", "\n");
        return RES_ERR;
    } else if (res != RES_OK)
        return res;

    const node_t new_node =
        NODE_WHILE(TYPE_ANY_I, first_tok_i, last_tok_i, cond_i, body_i);
    buf_push(parser->par_nodes, new_node);
    *new_node_i = buf_size(parser->par_nodes) - 1;

    log_debug("new while=%d current_scope_i=%d cond=%d body=%d", *new_node_i,
              parser->par_scope_i, cond_i, body_i);

    return RES_OK;
}

static res_t parser_parse_loop(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_while_stmt(parser, new_node_i);
}

static res_t parser_parse_stmt(parser_t* parser, int* new_node_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) == TOK_ID_EOF) return RES_NONE;

    res_t res = RES_NONE;
    if ((res = parser_parse_declaration(parser, new_node_i)) != RES_NONE)
        return res;

    if ((res = parser_parse_assignment(parser, new_node_i)) != RES_NONE)
        return res;

    if ((res = parser_parse_loop(parser, new_node_i)) != RES_NONE) return res;

    return parser_parse_expr(parser, new_node_i);
}

static res_t parser_parse(parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >, (int)0,
                   "%d");
    PG_ASSERT_COND((int)buf_size(parser->par_lexer.lex_tokens), >,
                   parser->par_tok_i, "%d");

    int new_node_i = -1;
    res_t res = RES_NONE;

    if ((res = parser_parse_stmt(parser, &new_node_i)) == RES_OK) {
        node_dump(parser, new_node_i, 0);
        buf_push(parser_current_block(parser)->node_n.node_block.bl_nodes_i,
                 new_node_i);

    } else if (res == RES_NONE) {
        fprintf(stderr, "At least one statement is required\n");
        return RES_NEED_AT_LEAST_ONE_STMT;
    } else
        return res;

    while (!parser_is_at_end(parser)) {
        if ((res = parser_parse_stmt(parser, &new_node_i)) == RES_OK) {
            node_dump(parser, new_node_i, 0);
            buf_push(parser_current_block(parser)->node_n.node_block.bl_nodes_i,
                     new_node_i);
            continue;
        } else if (res == RES_NONE)
            return RES_OK;

        const token_id_t current = parser_current(parser);
        if (current == TOK_ID_COMMENT) {
            parser->par_tok_i += 1;
            continue;
        }
        if (current == TOK_ID_EOF)
            return RES_OK;
        else
            return res;
    }
    UNREACHABLE();
}
