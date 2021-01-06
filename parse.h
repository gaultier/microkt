#pragma once

#include <stdarg.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "lex.h"

static const int TYPE_UNIT_I = 1;    // see parser_init
static const int TYPE_ANY_I = 2;     // see parser_init
static const int TYPE_LONG_I = 3;    // see parser_init
static const int TYPE_INT_I = 4;     // see parser_init
static const int TYPE_BOOL_I = 5;    // see parser_init
static const int TYPE_CHAR_I = 6;    // see parser_init
static const int TYPE_BYTE_I = 7;    // see parser_init
static const int TYPE_SHORT_I = 8;   // see parser_init
static const int TYPE_STRING_I = 9;  // see parser_init

// User Defined Type (UDF)
typedef struct {
    int ud_type_i, ud_name_len;
    char* ud_name;  // Allocated, non nul terminated
} udf_t;

typedef struct {
    const char* par_file_name0;
    int par_tok_i, par_scope_i, par_fn_i, par_class_i,
        par_main_fn_i;      // Current token/scope/function/class/main function
    mkt_node_t* par_nodes;  // Arena of all nodes
    lexer_t par_lexer;
    int* par_node_decls;  // Declarations e.g. functions
    mkt_type_t* par_types;
    udf_t* par_udfs;
} parser_t;

static mkt_res_t parser_parse_expr(parser_t* parser, int* new_node_i);
static mkt_res_t parser_parse_stmt(parser_t* parser, int* new_node_i);
static mkt_res_t parser_parse_control_structure_body(parser_t* parser,
                                                     int* new_node_i);
static mkt_res_t parser_parse_block(parser_t* parser, int* new_node_i);
static mkt_res_t parser_parse_builtin_println(parser_t* parser,
                                              int* new_node_i);
static void parser_tok_source(const parser_t* parser, int tok_i,
                              const char** source, int* source_len);
static void parser_print_source_on_error(const parser_t* parser,
                                         int first_tok_i, int last_tok_i);
static mkt_res_t parser_parse_call_suffix(parser_t* parser, int lhs_i,
                                          int* new_node_i);
static mkt_res_t parser_parse_syscall(parser_t* parser, int* new_node_i);
static mkt_res_t parser_parse_declaration(parser_t* parser, int* new_node_i);

static mkt_node_t* parser_current_block(parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");

    return &parser->par_nodes[parser->par_scope_i];
}

static int parser_scope_begin(parser_t* parser, int block_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");

    const int parent_scope_i = parser->par_scope_i;
    parser->par_scope_i = block_node_i;

    log_debug("%d -> %d", parent_scope_i, parser->par_scope_i);
    return parent_scope_i;
}

static void parser_scope_end(parser_t* parser, int parent_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(parent_node_i, >=, 0, "%d");

    log_debug("%d <- %d", parent_node_i, parser->par_scope_i);
    parser->par_scope_i = parent_node_i;
}

static mkt_res_t parser_node_find_callable(const parser_t* parser, int no_i,
                                           int* node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(no_i, >=, 0, "%d");
    CHECK((void*)node_i, !=, NULL, "%p");

    const mkt_node_t* const node = &parser->par_nodes[no_i];
    switch (node->no_kind) {
        case NODE_VAR:
            TRY_OK(parser_node_find_callable(
                parser, node->no_n.no_var.va_var_node_i, node_i));
            return RES_OK;
        case NODE_FN_DECL:
            *node_i = no_i;
            return RES_OK;
        case NODE_CLASS_DECL:
            *node_i = no_i;
            return RES_OK;
        default:
            log_debug("kind: %s", mkt_node_kind_to_str[node->no_kind]);
            return RES_ERR;
    }
}

static mkt_res_t parser_resolve_member(const parser_t* parser, int tok_i,
                                       const mkt_class_decl_t* class_decl,
                                       int* def_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)class_decl, !=, NULL, "%p");
    CHECK((void*)def_node_i, !=, NULL, "%p");
    CHECK(tok_i, >=, 0, "%d");

    const char* member_source = NULL;
    int member_source_len = 0;
    parser_tok_source(parser, tok_i, &member_source, &member_source_len);
    CHECK((void*)member_source, !=, NULL, "%p");
    CHECK(member_source_len, >=, 0, "%d");

    for (int i = 0; i < (int)buf_size(class_decl->cl_members); i++) {
        const int m_i = class_decl->cl_members[i];
        const mkt_node_t* const m_node = &parser->par_nodes[m_i];

        if (m_node->no_kind != NODE_VAR_DEF) continue;

        const mkt_var_def_t var_def = m_node->no_n.no_var_def;
        const int tok_i = var_def.vd_name_tok_i;
        const char* m_source = NULL;
        int m_source_len = 0;
        parser_tok_source(parser, tok_i, &m_source, &m_source_len);
        CHECK((void*)m_source, !=, NULL, "%p");
        CHECK(m_source_len, >=, 0, "%d");

        if (m_source_len == member_source_len &&
            memcmp(m_source, member_source, m_source_len) == 0) {
            *def_node_i = m_i;
            return RES_OK;
        }
    }

    return RES_NONE;
}

// TODO: optimize, currently it is O(n*m) where n= # of stmt and m = # of var
// def per scope
static mkt_res_t parser_resolve_var(const parser_t* parser, int tok_i,
                                    int* def_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)def_node_i, !=, NULL, "%p");
    CHECK(tok_i, >=, 0, "%d");

    const char* var_source = NULL;
    int var_source_len = 0;
    parser_tok_source(parser, tok_i, &var_source, &var_source_len);
    CHECK((void*)var_source, !=, NULL, "%p");
    CHECK(var_source_len, >=, 0, "%d");

    int current_scope_i = parser->par_scope_i;
    while (current_scope_i >= 0) {
        CHECK(current_scope_i, <, (int)buf_size(parser->par_nodes), "%d");

        const mkt_node_t* block = &parser->par_nodes[current_scope_i];
        const mkt_block_t b = block->no_n.no_block;

        log_debug("resolving var %.*s in scope %d", var_source_len, var_source,
                  current_scope_i);

        for (int i = 0; i < (int)buf_size(b.bl_nodes_i); i++) {
            const int stmt_i = b.bl_nodes_i[i];
            CHECK(stmt_i, >=, 0, "%d");
            CHECK(stmt_i, <, (int)buf_size(parser->par_nodes), "%d");

            const mkt_node_t* const stmt = &parser->par_nodes[stmt_i];
            const char* def_source = NULL;
            int def_source_len = 0;

            if (stmt->no_kind == NODE_VAR_DEF)
                parser_tok_source(parser, stmt->no_n.no_var_def.vd_name_tok_i,
                                  &def_source, &def_source_len);
            else if (stmt->no_kind == NODE_FN_DECL)
                parser_tok_source(parser, stmt->no_n.no_fn_decl.fd_name_tok_i,
                                  &def_source, &def_source_len);
            else if (stmt->no_kind == NODE_CLASS_DECL)
                parser_tok_source(parser,
                                  stmt->no_n.no_class_decl.cl_name_tok_i,
                                  &def_source, &def_source_len);
            else
                continue;

            CHECK((void*)def_source, !=, NULL, "%p");
            CHECK(def_source_len, >=, 0, "%d");

            log_debug("considering var def: name=`%.*s` kind=%s scope=%d",
                      def_source_len, def_source,
                      mkt_node_kind_to_str[stmt->no_kind], current_scope_i);

            if (def_source_len == var_source_len &&
                memcmp(def_source, var_source, var_source_len) == 0) {
                *def_node_i = stmt_i;

                log_debug("resolved var: name=`%.*s` kind=%s scope=%d",
                          def_source_len, def_source,
                          mkt_node_kind_to_str[stmt->no_kind], current_scope_i);

                return RES_OK;
            }
        }

        current_scope_i = b.bl_parent_scope_i;
    }

    log_debug("var `%.*s` could not be resolved", var_source_len, var_source);
    return RES_NONE;
}

static void parser_class_begin(parser_t* parser, int first_tok_i,
                               int name_tok_i, int* new_node_i,
                               int* old_class_i, int* body_node_i,
                               int* parent_scope_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");
    CHECK((void*)old_class_i, !=, NULL, "%p");
    CHECK((void*)body_node_i, !=, NULL, "%p");
    CHECK((void*)parent_scope_i, !=, NULL, "%p");

    buf_push(parser->par_types,
             ((mkt_type_t){.ty_kind = TYPE_USER, .ty_size = 0 /* FIXME */}));
    const int type_i = buf_size(parser->par_types) - 1;

    buf_push(parser->par_nodes,
             ((mkt_node_t){.no_type_i = buf_size(parser->par_types) - 1,
                           .no_kind = NODE_CLASS_DECL,
                           .no_n = {.no_class_decl = {
                                        .cl_first_tok_i = first_tok_i,
                                        .cl_name_tok_i = name_tok_i,
                                        .cl_last_tok_i = -1,
                                        .cl_flags = CLASS_FLAGS_PUBLIC,
                                    }}}));

    *old_class_i = parser->par_class_i;
    parser->par_class_i = *new_node_i = buf_size(parser->par_nodes) - 1;
    buf_push(parser->par_node_decls, *new_node_i);

    int class_name_len = 0;
    const char* class_name = NULL;
    parser_tok_source(parser, name_tok_i, &class_name, &class_name_len);
    buf_push(parser->par_udfs,
             ((udf_t){.ud_type_i = type_i,
                      .ud_name_len = class_name_len,
                      .ud_name = strndup(class_name, class_name_len)}));

    if (parser->par_scope_i >= 0) {
        CHECK(parser->par_scope_i, <, (int)buf_size(parser->par_nodes), "%d");
        buf_push(
            parser->par_nodes[parser->par_scope_i].no_n.no_block.bl_nodes_i,
            *new_node_i);
    }

    buf_push(parser->par_nodes,
             ((mkt_node_t){.no_kind = NODE_BLOCK,
                           .no_type_i = TYPE_UNIT_I,
                           .no_n = {.no_block = {.bl_first_tok_i = -1,
                                                 .bl_last_tok_i = -1,
                                                 .bl_nodes_i = NULL,
                                                 .bl_parent_scope_i =
                                                     parser->par_scope_i}}}));
    *body_node_i =
        parser->par_nodes[*new_node_i].no_n.no_class_decl.cl_body_node_i =
            buf_size(parser->par_nodes) - 1;
    *parent_scope_i = parser_scope_begin(parser, *body_node_i);
}

static mkt_res_t parser_err_assigning_val(const parser_t* parser,
                                          int assign_tok_i,
                                          const mkt_var_def_t* var_def) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)var_def, !=, NULL, "%p");
    CHECK(assign_tok_i, >=, 0, "%d");
    CHECK(assign_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_loc_t vd_loc_start =
        parser->par_lexer.lex_locs[var_def->vd_first_tok_i];
    const mkt_loc_t assign_loc_start = parser->par_lexer.lex_locs[assign_tok_i];

    fprintf(stderr,
            "%s%s:%d:%d:%sTrying to assign a variable declared with `val`\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            assign_loc_start.loc_line, assign_loc_start.loc_column,
            mkt_colors[is_tty][COL_RESET]);

    parser_print_source_on_error(parser, assign_tok_i, assign_tok_i);
    fprintf(stderr, "%s%s:%d:%d:%sDeclared here:\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            vd_loc_start.loc_line, vd_loc_start.loc_column,
            mkt_colors[is_tty][COL_RESET]);
    parser_print_source_on_error(parser, var_def->vd_first_tok_i,
                                 var_def->vd_first_tok_i);
    return RES_ASSIGNING_VAL;
}

static mkt_res_t parser_err_missing_rhs(const parser_t* parser, int first_tok_i,
                                        int last_tok_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(first_tok_i, >=, 0, "%d");
    CHECK(first_tok_i, <, parser->par_lexer.lex_source_len, "%d");
    CHECK(last_tok_i, >=, first_tok_i, "%d");
    CHECK(last_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_loc_t first_tok_loc = parser->par_lexer.lex_locs[first_tok_i];

    fprintf(stderr, "%s%s:%d:%d:%sMissing right hand-side operand\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            first_tok_loc.loc_line, first_tok_loc.loc_column,
            mkt_colors[is_tty][COL_RESET]);

    parser_print_source_on_error(parser, first_tok_i, last_tok_i);
    return RES_ERR;  // FIXME?
}

static bool parser_check_keyword(const parser_t* parser,
                                 const char* source_start, const char suffix[],
                                 int suffix_len) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)source_start, !=, NULL, "%p");
    CHECK((void*)(source_start + suffix_len), <,
          (void*)(parser->par_lexer.lex_source +
                  parser->par_lexer.lex_source_len),
          "%p");

    const int remaining_len =
        (parser->par_lexer.lex_source + parser->par_lexer.lex_source_len) -
        (source_start);

    if (remaining_len >= suffix_len &&
        memcmp(source_start, suffix, suffix_len) == 0) {
        return true;
    } else
        return false;
}

static bool parser_parse_identifier_to_type_kind(const parser_t* parser,
                                                 int tok_i, int* type_i) {
    CHECK((void*)parser, !=, NULL, "%p");

    const char* source = NULL;
    int source_len = 0;
    parser_tok_source(parser, tok_i, &source, &source_len);
    CHECK((void*)source, !=, NULL, "%p");
    CHECK(source_len, <=, parser->par_lexer.lex_source_len, "%d");

    if (source_len <= 2) goto udf;

    switch (source[0]) {
        case 'A':
            if (parser_check_keyword(parser, source + 1, "ny", 2)) {
                *type_i = TYPE_ANY_I;
                return true;
            }

            break;
        case 'B': {
            switch (source[1]) {
                case 'o':
                    if (parser_check_keyword(parser, source + 2, "ol", 2)) {
                        *type_i = TYPE_BOOL_I;
                        return true;
                    }

                    break;
                case 'y':
                    if (parser_check_keyword(parser, source + 2, "te", 2)) {
                        *type_i = TYPE_BYTE_I;
                        return true;
                    }

                    break;
                default:
                    break;
            }
            break;
        }
        case 'C':
            if (parser_check_keyword(parser, source + 1, "har", 3)) {
                *type_i = TYPE_CHAR_I;
                return true;
            }

            break;
        case 'I':
            if (parser_check_keyword(parser, source + 1, "nt", 2)) {
                *type_i = TYPE_INT_I;
                return true;
            }

            break;
        case 'L':
            if (parser_check_keyword(parser, source + 1, "ong", 3)) {
                *type_i = TYPE_LONG_I;
                return true;
            }

            break;
        case 'S': {
            switch (source[1]) {
                case 'h':
                    if (parser_check_keyword(parser, source + 2, "ort", 3)) {
                        *type_i = TYPE_SHORT_I;
                        return true;
                    }

                    break;
                case 't':
                    if (parser_check_keyword(parser, source + 2, "ring", 4)) {
                        *type_i = TYPE_STRING_I;
                        return true;
                    }

                    break;
                default:
                    break;
            }
            break;
        }
        case 'U': {
            switch (source[1]) {
                case 'n':
                    if (parser_check_keyword(parser, source + 2, "it", 2)) {
                        *type_i = TYPE_UNIT_I;
                        return true;
                    }

                    break;
                default:
                    break;
            }
        }
        default:
            break;
    }

udf:
    // Try to find UDF
    for (int i = 0; i < (int)buf_size(parser->par_udfs); i++) {
        const udf_t* const udf = &parser->par_udfs[i];
        if (source_len == udf->ud_name_len &&
            memcmp(source, udf->ud_name, source_len) == 0) {
            *type_i = udf->ud_type_i;
            return true;
        }
    }

    return false;
}

static mkt_res_t parser_init(const char* file_name0, const char* source,
                             int source_len, parser_t* parser) {
    CHECK((void*)file_name0, !=, NULL, "%p");
    CHECK((void*)source, !=, NULL, "%p");
    CHECK((void*)parser, !=, NULL, "%p");

    parser->par_file_name0 = file_name0;
    parser->par_main_fn_i = parser->par_class_i = parser->par_scope_i =
        parser->par_fn_i = -1;

    CHECK((void*)parser->par_node_decls, ==, NULL, "%p");
    buf_grow(parser->par_node_decls, 10);

    CHECK((void*)parser->par_nodes, ==, NULL, "%p");
    buf_grow(parser->par_nodes, 100);

    CHECK((void*)parser->par_udfs, ==, NULL, "%p");
    buf_grow(parser->par_udfs, 100);

    TRY_OK(lex_init(file_name0, source, source_len, &parser->par_lexer));

    // Add synthetic tokens for synthetic root class
    buf_push(parser->par_lexer.lex_tokens,
             ((mkt_token_t){.tok_id = TOK_ID_IDENTIFIER,
                            .tok_pos_range = {.pr_start = 0, .pr_end = 0}}));
    buf_push(parser->par_lexer.lex_tok_pos_ranges,
             ((mkt_pos_range_t){.pr_start = 0, .pr_end = 0}));
    buf_push(parser->par_lexer.lex_locs,
             ((mkt_loc_t){.loc_line = 1, .loc_column = 1}));

    // Add root class
    int new_node_i = -1, old_class_i = -1, body_node_i = -1,
        parent_scope_i = -1;
    CHECK((void*)parser->par_types, ==, NULL, "%p");
    buf_grow(parser->par_types, 100);
    parser_class_begin(parser, 0, -1, &new_node_i, &old_class_i, &body_node_i,
                       &parent_scope_i);

    // Pre-allocate common types
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_UNIT,
                                }));  // Hence TYPE_UNIT_I = 1
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_ANY,
                                }));  // Hence TYPE_ANY_I = 2
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_LONG,
                                    .ty_size = 8,
                                }));  // Hence TYPE_LONG_I = 3
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_INT,
                                    .ty_size = 4,
                                }));  // Hence TYPE_INT_I = 4
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_BOOL,
                                    .ty_size = 1,
                                }));  // Hence TYPE_BOOL_I = 5
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_CHAR,
                                    .ty_size = 1,
                                }));  // Hence TYPE_CHAR_I = 6
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_BYTE,
                                    .ty_size = 1,
                                }));  // Hence TYPE_BYTE_I = 7
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_SHORT,
                                    .ty_size = 2,
                                }));  // Hence TYPE_SHORT_I = 8
    buf_push(parser->par_types, ((mkt_type_t){
                                    .ty_kind = TYPE_STRING,
                                    .ty_size = 8,
                                }));  // Hence TYPE_STRING_I = 9

    return RES_OK;
}

static long long int parse_tok_to_long(const parser_t* parser, int tok_i,
                                       int* type_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)parser->par_lexer.lex_tok_pos_ranges, !=, NULL, "%p");
    CHECK((void*)parser->par_lexer.lex_source, !=, NULL, "%p");
    CHECK(tok_i, >=, 0, "%d");
    CHECK(tok_i, <, parser->par_lexer.lex_source_len, "%d");

    static char string0[25];
    memset(string0, 0, sizeof(string0));

    const mkt_pos_range_t pos_range =
        parser->par_lexer.lex_tok_pos_ranges[tok_i];
    const char* const string =
        &parser->par_lexer.lex_source[pos_range.pr_start];
    CHECK((void*)string, !=, NULL, "%p");

    const int string_len = pos_range.pr_end - pos_range.pr_start;

    CHECK(string_len, >, 0, "%d");
    CHECK(string_len, <, (int)sizeof(string0), "%d");

    memcpy(string0, string, (size_t)string_len);

    *type_i = string[string_len - 1] == 'L' ? TYPE_LONG_I : TYPE_INT_I;

    return strtoll(string0, NULL, 10);
}

static long long int parse_tok_to_char(const parser_t* parser, int tok_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(tok_i, >=, 0, "%d");
    CHECK(tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_pos_range_t pos_range =
        parser->par_lexer.lex_tok_pos_ranges[tok_i];
    const char* const string =
        &parser->par_lexer.lex_source[pos_range.pr_start + 1];
    CHECK((void*)string, !=, NULL, "%p");

    int string_len = pos_range.pr_end - pos_range.pr_start - 2;

    CHECK(string_len, >, 0, "%d");
    CHECK(string_len, <, 2, "%d");  // TODO: expand

    return string[0];
}

static void node_dump(const parser_t* parser, int no_i, int indent) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(no_i, >=, 0, "%d");
    CHECK(no_i, <, (int)buf_size(parser->par_nodes), "%d");

#if WITH_LOGS == 0
    IGNORE(parser);
    IGNORE(no_i);
    IGNORE(indent);
#else
    const mkt_node_t* const node = &parser->par_nodes[no_i];
    switch (node->no_kind) {
        case NODE_BUILTIN_PRINTLN: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);
            node_dump(parser, node->no_n.no_builtin_println.bp_arg_i,
                      indent + 2);
            break;
        }
        case NODE_SYSCALL: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);

            const mkt_syscall_t syscall = node->no_n.no_syscall;
            for (int i = 0; i < (int)buf_size(syscall.sy_arg_nodes_i); i++)
                node_dump(parser, syscall.sy_arg_nodes_i[i], indent + 2);

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
        case NODE_MEMBER_GET:
        case NODE_ADD: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);
            node_dump(parser, node->no_n.no_binary.bi_lhs_i, indent + 2);
            node_dump(parser, node->no_n.no_binary.bi_rhs_i, indent + 2);

            break;
        }
        case NODE_IF: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);
            node_dump(parser, node->no_n.no_if.if_node_cond_i, indent + 2);
            node_dump(parser, node->no_n.no_if.if_node_then_i, indent + 2);

            if (node->no_n.no_if.if_node_else_i >= 0)
                node_dump(parser, node->no_n.no_if.if_node_else_i, indent + 2);

            break;
        }
        case NODE_RETURN:
        case NODE_NOT: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);
            if (node->no_n.no_unary.un_node_i >= 0)
                node_dump(parser, node->no_n.no_unary.un_node_i, indent + 2);

            break;
        }
        case NODE_KEYWORD_BOOL:
        case NODE_LONG:
        case NODE_CHAR:
        case NODE_STRING: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);
            break;
        }
        case NODE_BLOCK: {
            const mkt_block_t block = node->no_n.no_block;
            log_debug_with_indent(
                indent, "node #%d %s type=%s parent_scope=%d", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind],
                block.bl_parent_scope_i);

            for (int i = 0; i < (int)buf_size(block.bl_nodes_i); i++)
                node_dump(parser, block.bl_nodes_i[i], indent + 2);

            break;
        }
        case NODE_VAR_DEF: {
            const mkt_var_def_t var_def = node->no_n.no_var_def;
            const mkt_pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[var_def.vd_name_tok_i];

            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const int name_len = pos_range.pr_end - pos_range.pr_start;
            log_debug_with_indent(
                indent, "node #%d %s type=%s name=%.*s offset=%d flags=%d",
                no_i, mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind],
                name_len, name, var_def.vd_stack_offset, var_def.vd_flags);

            if (var_def.vd_init_node_i >= 0)
                node_dump(parser, var_def.vd_init_node_i, indent + 2);

            break;
        }
        case NODE_VAR: {
            const mkt_var_t var = node->no_n.no_var;
            const mkt_node_t* const no_var_def =
                &parser->par_nodes[var.va_var_node_i];
            const mkt_var_def_t var_def = no_var_def->no_n.no_var_def;
            const mkt_pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[var_def.vd_name_tok_i];

            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const int name_len = pos_range.pr_end - pos_range.pr_start;

            log_debug_with_indent(
                indent, "node #%d %s type=%s name=%.*s offset=%d", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind],
                name_len, name, var_def.vd_stack_offset);

            break;
        }
        case NODE_WHILE: {
            log_debug_with_indent(
                indent, "node #%d %s type=%s", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind]);

            node_dump(parser, node->no_n.no_while.wh_cond_i, indent + 2);
            node_dump(parser, node->no_n.no_while.wh_body_i, indent + 2);
            return;
        }
        case NODE_FN_DECL: {
            const mkt_fn_decl_t fn_decl = node->no_n.no_fn_decl;
            const int arity = buf_size(fn_decl.fd_arg_nodes_i);
            const mkt_pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[fn_decl.fd_name_tok_i];
            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const int name_len = pos_range.pr_end - pos_range.pr_start;
            log_debug_with_indent(
                indent, "node #%d %s `%.*s` type=%s arity=%d body_i=%d", no_i,
                mkt_node_kind_to_str[node->no_kind], name_len, name,
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind],
                arity, fn_decl.fd_body_node_i);
            return;
        }
        case NODE_CALL: {
            const mkt_call_t call = node->no_n.no_call;
            log_debug_with_indent(
                indent, "node #%d %s type=%s arity=%d", no_i,
                mkt_node_kind_to_str[node->no_kind],
                mkt_type_to_str[parser->par_types[node->no_type_i].ty_kind],
                (int)buf_size(call.ca_arg_nodes_i));

            node_dump(parser, call.ca_lhs_node_i, indent + 2);
            return;
        }
        case NODE_CLASS_DECL: {
            const mkt_class_decl_t class_decl = node->no_n.no_class_decl;
            const char* src = NULL;
            int src_len = 0;
            if (class_decl.cl_name_tok_i >= 0)
                parser_tok_source(parser, class_decl.cl_name_tok_i, &src,
                                  &src_len);
            else {
                src_len = strlen(parser->par_file_name0);
                src = parser->par_file_name0;
            }

            log_debug_with_indent(indent, "node #%d %s `%.*s`", no_i,
                                  mkt_node_kind_to_str[node->no_kind], src_len,
                                  src);

            for (int i = 0; i < (int)buf_size(class_decl.cl_members); i++)
                node_dump(parser, class_decl.cl_members[i], indent + 2);

            return;
        }
        case NODE_INSTANCE: {
            const mkt_instance_t instance = node->no_n.no_instance;
            CHECK(instance.in_class, >=, 0, "%d");

            const mkt_class_decl_t class_decl =
                parser->par_nodes[instance.in_class].no_n.no_class_decl;
            const char* src = NULL;
            int src_len = 0;
            if (class_decl.cl_name_tok_i >= 0)
                parser_tok_source(parser, class_decl.cl_name_tok_i, &src,
                                  &src_len);
            else {
                src_len = strlen(parser->par_file_name0);
                src = parser->par_file_name0;
            }

            log_debug_with_indent(indent, "node #%d %s `%.*s`", no_i,
                                  mkt_node_kind_to_str[node->no_kind], src_len,
                                  src);

            return;
        }
    }
#endif
}

static int node_first_token(const parser_t* parser, const mkt_node_t* node) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)node, !=, NULL, "%p");

    switch (node->no_kind) {
        case NODE_BUILTIN_PRINTLN:
            return node->no_n.no_builtin_println.bp_keyword_print_i;
        case NODE_SYSCALL:
            return node->no_n.no_syscall.sy_first_tok_i;
        case NODE_STRING:
            return node->no_n.no_string.st_tok_i;
        case NODE_KEYWORD_BOOL:
        case NODE_CHAR:
        case NODE_LONG:
            return node->no_n.no_num.nu_tok_i;
        case NODE_LT:
        case NODE_LE:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_MULTIPLY:
        case NODE_DIVIDE:
        case NODE_MODULO:
        case NODE_SUBTRACT:
        case NODE_ASSIGN:
        case NODE_MEMBER_GET:
        case NODE_ADD: {
            const mkt_node_t* const lhs =
                &parser->par_nodes[node->no_n.no_binary.bi_lhs_i];
            return node_first_token(parser, lhs);
        }
        case NODE_RETURN:
        case NODE_NOT: {
            return node->no_n.no_unary.un_first_tok_i;
        }
        case NODE_IF:
            return node->no_n.no_if.if_first_tok_i;
        case NODE_BLOCK:
            return node->no_n.no_block.bl_first_tok_i;
        case NODE_VAR_DEF:
            return node->no_n.no_var_def.vd_first_tok_i;
        case NODE_VAR:
            return node->no_n.no_var.va_tok_i;
        case NODE_WHILE:
            return node->no_n.no_while.wh_first_tok_i;
        case NODE_FN_DECL:
            return node->no_n.no_fn_decl.fd_first_tok_i;
        case NODE_CLASS_DECL:
            return node->no_n.no_class_decl.cl_first_tok_i;
        case NODE_CALL:
            return node->no_n.no_call.ca_first_tok_i;
        case NODE_INSTANCE:
            return node->no_n.no_instance.in_first_tok_i;
    }
    log_debug("node kind=%d", node->no_kind);
    UNREACHABLE();
}

static int node_last_token(const parser_t* parser, const mkt_node_t* node) {
    CHECK((void*)parser, !=, NULL, "%p");
    if (node == NULL) return -1;

    switch (node->no_kind) {
        case NODE_BUILTIN_PRINTLN:
            return node->no_n.no_builtin_println.bp_rparen_i;
        case NODE_SYSCALL:
            return node->no_n.no_syscall.sy_last_tok_i;
        case NODE_STRING:
            return node->no_n.no_string.st_tok_i;
        case NODE_KEYWORD_BOOL:
        case NODE_LONG:
        case NODE_CHAR:
            return node->no_n.no_num.nu_tok_i;
        case NODE_LT:
        case NODE_LE:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_MULTIPLY:
        case NODE_DIVIDE:
        case NODE_MODULO:
        case NODE_SUBTRACT:
        case NODE_ASSIGN:
        case NODE_MEMBER_GET:
        case NODE_ADD: {
            const mkt_node_t* const rhs =
                &parser->par_nodes[node->no_n.no_binary.bi_rhs_i];
            return node_first_token(parser, rhs);
        }
        case NODE_RETURN:
        case NODE_NOT: {
            return node->no_n.no_unary.un_last_tok_i;
        }
        case NODE_IF:
            return node->no_n.no_if.if_last_tok_i;
        case NODE_BLOCK:
            return node->no_n.no_block.bl_last_tok_i;
        case NODE_VAR_DEF:
            return node->no_n.no_var_def.vd_last_tok_i;
        case NODE_VAR:
            return node->no_n.no_var.va_tok_i;
        case NODE_WHILE:
            return node->no_n.no_while.wh_last_tok_i;
        case NODE_FN_DECL:
            return node->no_n.no_fn_decl.fd_last_tok_i;
        case NODE_CLASS_DECL:
            return node->no_n.no_class_decl.cl_last_tok_i;
        case NODE_CALL:
            return node->no_n.no_call.ca_last_tok_i;
        case NODE_INSTANCE:
            return node->no_n.no_instance.in_last_tok_i;
    }
    log_debug("node kind=%d", node->no_kind);
    UNREACHABLE();
}

static void parser_tok_source(const parser_t* parser, int tok_i,
                              const char** source, int* source_len) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)source, !=, NULL, "%p");
    CHECK((void*)source_len, !=, NULL, "%p");
    CHECK(tok_i, <, parser->par_lexer.lex_source_len, "%d");

    if (tok_i == -1) {
        *source = parser->par_file_name0;
        *source_len = strlen(parser->par_file_name0);
        return;
    }

    const mkt_token_id_t tok = parser->par_lexer.lex_tokens[tok_i].tok_id;
    const mkt_pos_range_t pos_range =
        parser->par_lexer.lex_tok_pos_ranges[tok_i];

    // Without quotes for char/string
    if (tok == TOK_ID_CHAR) {
        *source = &parser->par_lexer
                       .lex_source[(tok == TOK_ID_STRING || tok == TOK_ID_CHAR)
                                       ? pos_range.pr_start + 1
                                       : pos_range.pr_start];
        *source_len = pos_range.pr_end - pos_range.pr_start - 2;
    } else if (tok == TOK_ID_STRING) {
        const mkt_pos_range_t pos_range =
            parser->par_lexer.lex_tok_pos_ranges[tok_i];
        const bool multiline =
            parser->par_lexer.lex_source[pos_range.pr_end - 1] == '"' &&
            parser->par_lexer.lex_source[pos_range.pr_end - 2] == '"' &&
            pos_range.pr_end - pos_range.pr_start >= 3 &&
            parser->par_lexer.lex_source[pos_range.pr_end - 3] == '"';

        *source =
            &parser->par_lexer.lex_source[multiline ? pos_range.pr_start + 3
                                                    : pos_range.pr_start + 1];

        *source_len =
            pos_range.pr_end - pos_range.pr_start - (multiline ? 6 : 2);
    } else {
        *source = &parser->par_lexer.lex_source[pos_range.pr_start];
        *source_len = pos_range.pr_end - pos_range.pr_start;
    }

    CHECK((void*)*source, !=, NULL, "%p");
    CHECK(*source_len, >=, 0, "%d");
    CHECK(*source_len, <=, parser->par_lexer.lex_source_len, "%d");
}

static bool parser_is_at_end(const parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(parser->par_tok_i, >=, 0, "%d");

    return parser->par_tok_i >= (int)buf_size(parser->par_lexer.lex_tokens);
}

static mkt_token_id_t parser_current(const parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(parser->par_tok_i, >=, 0, "%d");
    CHECK(parser->par_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    return parser->par_lexer.lex_tokens[parser->par_tok_i].tok_id;
}

static mkt_token_id_t parser_previous(const parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(parser->par_tok_i, >, 1, "%d");
    CHECK(parser->par_tok_i, <, (int)buf_size(parser->par_lexer.lex_tokens),
          "%d");

    return parser->par_lexer.lex_tokens[parser->par_tok_i - 1].tok_id;
}
static void parser_advance_until_after(parser_t* parser, mkt_token_id_t id) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(parser->par_tok_i, >=, 0, "%d");
    CHECK((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, 0, "%d");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, parser->par_tok_i,
          "%d");

    while (!parser_is_at_end(parser) && parser_current(parser) != id) {
        CHECK(parser->par_tok_i, <, (int)buf_size(parser->par_lexer.lex_tokens),
              "%d");
        parser->par_tok_i += 1;
        CHECK(parser->par_tok_i, <, (int)buf_size(parser->par_lexer.lex_tokens),
              "%d");
    }

    parser->par_tok_i += 1;
}

static mkt_token_id_t parser_peek(parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, 0, "%d");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, parser->par_tok_i,
          "%d");

    while (parser->par_tok_i < (int)buf_size(parser->par_lexer.lex_tokens)) {
        const mkt_token_id_t id =
            parser->par_lexer.lex_tokens[parser->par_tok_i].tok_id;
        if (id == TOK_ID_COMMENT) {
            parser->par_tok_i += 1;
            continue;
        }

        return id;
    }
    UNREACHABLE();
}

static mkt_token_id_t parser_peek_next(parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    CHECK(parser->par_tok_i, >=, 0, "%d");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, 0, "%d");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, parser->par_tok_i,
          "%d");

    int i = parser->par_tok_i;
    while (i < (int)buf_size(parser->par_lexer.lex_tokens) - 1) {
        const mkt_token_id_t id = parser->par_lexer.lex_tokens[i + 1].tok_id;
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
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(first_tok_i, >=, 0, "%d");
    CHECK(first_tok_i, <, parser->par_lexer.lex_source_len, "%d");
    CHECK(last_tok_i, >=, first_tok_i, "%d");
    CHECK(last_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_pos_range_t first_tok_pos_range =
        parser->par_lexer.lex_tok_pos_ranges[first_tok_i];
    const mkt_loc_t first_tok_loc = parser->par_lexer.lex_locs[first_tok_i];
    const mkt_pos_range_t last_tok_pos_range =
        parser->par_lexer.lex_tok_pos_ranges[last_tok_i];
    const mkt_loc_t last_tok_loc = parser->par_lexer.lex_locs[last_tok_i];

    const int first_line = first_tok_loc.loc_line;
    const int last_line = last_tok_loc.loc_line;
    CHECK(first_line, <=, last_line, "%d");

    int first_line_start_tok_i = first_tok_i;
    while (true) {
        first_line_start_tok_i--;

        if (first_line_start_tok_i < 0 ||
            parser->par_lexer.lex_locs[first_line_start_tok_i].loc_line <
                first_line) {
            first_line_start_tok_i++;

            CHECK(first_line_start_tok_i, >=, 0, "%d");
            CHECK(first_line_start_tok_i, <,
                  (int)parser->par_lexer.lex_source_len, "%d");

            break;
        }
    }

    mkt_pos_range_t first_line_start_tok_pos =
        parser->par_lexer.lex_tok_pos_ranges[first_line_start_tok_i];

    int last_line_start_tok_i = last_tok_i;
    while (true) {
        last_line_start_tok_i++;

        if (last_line_start_tok_i >=
                (int)buf_size(parser->par_lexer.lex_locs) ||
            last_line <
                parser->par_lexer.lex_locs[last_line_start_tok_i].loc_line) {
            last_line_start_tok_i--;

            CHECK(last_line_start_tok_i, >=, 0, "%d");
            CHECK(last_line_start_tok_i, >=, first_line_start_tok_i, "%d");
            CHECK(last_line_start_tok_i, <,
                  (int)parser->par_lexer.lex_source_len, "%d");

            break;
        }
    }
    CHECK(last_line_start_tok_i, >=, first_line_start_tok_i, "%d");

    mkt_pos_range_t last_line_start_tok_pos =
        parser->par_lexer.lex_tok_pos_ranges[last_line_start_tok_i];

    const char* source =
        &parser->par_lexer.lex_source[first_line_start_tok_pos.pr_start];
    int source_len =
        last_line_start_tok_pos.pr_end - first_line_start_tok_pos.pr_start;
    CHECK(source_len, >, 0, "%d");
    CHECK(source_len, <=, parser->par_lexer.lex_source_len, "%d");

    trim_end(&source, &source_len);
    CHECK(source_len, >, 0, "%d");
    CHECK(is_space(source[source_len - 1]), ==, false, "%d");

    static char prefix[MAXPATHLEN + 50] = "\0";
    snprintf(prefix, sizeof(prefix), "%s:%d:%d:", parser->par_file_name0,
             first_tok_loc.loc_line, first_tok_loc.loc_column);
    int prefix_len = strlen(prefix);

    fprintf(stderr, "%s%s%s%.*s\n", mkt_colors[is_tty][COL_GRAY], prefix,
            mkt_colors[is_tty][COL_RESET], (int)source_len, source);

    const int source_before_without_squiggly_len =
        first_tok_pos_range.pr_start - first_line_start_tok_pos.pr_start;
    CHECK(source_before_without_squiggly_len, <=,
          parser->par_lexer.lex_source_len, "%d");

    for (int i = 0; i < prefix_len + source_before_without_squiggly_len; i++)
        fprintf(stderr, " ");

    fprintf(stderr, "%s", mkt_colors[is_tty][COL_RED]);

    const int squiggly_len =
        last_tok_pos_range.pr_end - first_tok_pos_range.pr_start;
    CHECK(squiggly_len, <=, parser->par_lexer.lex_source_len, "%d");

    for (int i = 0; i < squiggly_len; i++) fprintf(stderr, "^");

    fprintf(stderr, "%s", mkt_colors[is_tty][COL_RESET]);

    fprintf(stderr, "\n");
}

static mkt_res_t parser_err_unexpected_token(const parser_t* parser,
                                             mkt_token_id_t expected) {
    CHECK((void*)parser, !=, NULL, "%p");

    const mkt_loc_t loc_start = parser->par_lexer.lex_locs[parser->par_tok_i];

    fprintf(stderr, "%s%s:%d:%d:%sUnexpected token. Expected `%s`, got `%s`\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            loc_start.loc_line, loc_start.loc_column,
            mkt_colors[is_tty][COL_RESET], mkt_token_id_to_str[expected],
            mkt_token_id_to_str[parser_current(parser)]);

    parser_print_source_on_error(parser, parser->par_tok_i, parser->par_tok_i);

    return RES_UNEXPECTED_TOKEN;
}

static bool parser_match(parser_t* parser, int* return_token_index,
                         int id_count, ...) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)return_token_index, !=, NULL, "%p");
    CHECK((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, 0, "%d");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, parser->par_tok_i,
          "%d");
    CHECK(id_count, >=, 0, "%d");

    const mkt_token_id_t current_id = parser_peek(parser);

    va_list ap;
    va_start(ap, id_count);

    for (; id_count; id_count--) {
        mkt_token_id_t id = va_arg(ap, mkt_token_id_t);

        if (parser_is_at_end(parser)) return false;

        if (id != current_id) continue;

        parser_advance_until_after(parser, id);
        CHECK(parser->par_tok_i, <, (int)buf_size(parser->par_lexer.lex_tokens),
              "%d");

        *return_token_index = parser->par_tok_i - 1;

        return true;
    }
    va_end(ap);

    return false;
}

static mkt_res_t parser_err_non_matching_types(const parser_t* parser,
                                               int lhs_node_i, int rhs_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(lhs_node_i, >=, 0, "%d");
    CHECK(lhs_node_i, <, (int)buf_size(parser->par_nodes), "%d");
    CHECK(rhs_node_i, >=, 0, "%d");
    CHECK(rhs_node_i, <, (int)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const lhs = &parser->par_nodes[lhs_node_i];
    CHECK((void*)lhs, !=, NULL, "%p");

    CHECK(lhs->no_type_i, >=, 0, "%d");
    CHECK(lhs->no_type_i, <, (int)buf_size(parser->par_types), "%d");
    const mkt_type_kind_t lhs_type_kind =
        parser->par_types[lhs->no_type_i].ty_kind;

    const int lhs_first_tok_i = node_first_token(parser, lhs);
    CHECK(lhs_first_tok_i, >=, 0, "%d");
    CHECK(lhs_first_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_node_t* const rhs = &parser->par_nodes[rhs_node_i];
    CHECK((void*)rhs, !=, NULL, "%p");

    CHECK(rhs->no_type_i, >=, 0, "%d");
    CHECK(rhs->no_type_i, <, (int)buf_size(parser->par_types), "%d");
    const int rhs_type_kind = parser->par_types[rhs->no_type_i].ty_kind;

    const int rhs_last_tok_i = node_last_token(parser, rhs);
    CHECK(rhs_last_tok_i, >=, 0, "%d");
    CHECK(rhs_last_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_loc_t lhs_first_tok_loc =
        parser->par_lexer.lex_locs[lhs_first_tok_i];

    fprintf(stderr, "%s%s:%d:%d:%sTypes do not match. Expected %s, got %s\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            lhs_first_tok_loc.loc_line, lhs_first_tok_loc.loc_column,
            mkt_colors[is_tty][COL_RESET], mkt_type_to_str[rhs_type_kind],
            mkt_type_to_str[lhs_type_kind]);

    parser_print_source_on_error(parser, lhs_first_tok_i, rhs_last_tok_i);

    return RES_NON_MATCHING_TYPES;
}

static mkt_res_t parser_err_unexpected_type(
    const parser_t* parser, int lhs_node_i,
    mkt_type_kind_t expected_type_kind) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(lhs_node_i, >=, 0, "%d");
    CHECK(lhs_node_i, <, (int)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const lhs = &parser->par_nodes[lhs_node_i];
    CHECK((void*)lhs, !=, NULL, "%p");

    CHECK(lhs->no_type_i, >=, 0, "%d");
    CHECK(lhs->no_type_i, <, (int)buf_size(parser->par_types), "%d");
    const mkt_type_kind_t lhs_type_kind =
        parser->par_types[lhs->no_type_i].ty_kind;

    const int lhs_first_tok_i = node_first_token(parser, lhs);
    CHECK(lhs_first_tok_i, >=, 0, "%d");
    CHECK(lhs_first_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const int lhs_last_tok_i = node_last_token(parser, lhs);
    CHECK(lhs_last_tok_i, >=, 0, "%d");
    CHECK(lhs_last_tok_i, <, parser->par_lexer.lex_source_len, "%d");

    const mkt_loc_t lhs_first_tok_loc =
        parser->par_lexer.lex_locs[lhs_first_tok_i];

    fprintf(stderr, "%s%s:%d:%d:%sTypes do not match. Expected %s, got %s\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            lhs_first_tok_loc.loc_line, lhs_first_tok_loc.loc_column,
            mkt_colors[is_tty][COL_RESET], mkt_type_to_str[expected_type_kind],
            mkt_type_to_str[lhs_type_kind]);

    parser_print_source_on_error(parser, lhs_first_tok_i, lhs_last_tok_i);

    return RES_NON_MATCHING_TYPES;
}

static mkt_res_t parser_parse_if_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int first_tok_i = -1, last_tok_i = -1, dummy = -1;
    if (!parser_match(parser, &first_tok_i, 1, TOK_ID_IF))
        return parser_err_unexpected_token(parser, TOK_ID_IF);

    if (!parser_match(parser, &dummy, 1, TOK_ID_LPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

    int no_cond_i = -1, no_then_i = -1, no_else_i = -1;
    TRY_OK(parser_parse_expr(parser, &no_cond_i));
    CHECK(no_cond_i, >=, 0, "%d");
    CHECK(no_cond_i, <, (int)buf_size(parser->par_nodes), "%d");
    const mkt_node_t* const no_cond = &parser->par_nodes[no_cond_i];

    CHECK(no_cond->no_type_i, >=, 0, "%d");
    CHECK(no_cond->no_type_i, <, (int)buf_size(parser->par_types), "%d");
    const mkt_type_kind_t cond_type_kind =
        parser->par_types[no_cond->no_type_i].ty_kind;

    if (cond_type_kind != TYPE_BOOL) {
        log_debug("if-cond type is not bool, got %s",
                  mkt_type_to_str[cond_type_kind]);
        return parser_err_unexpected_type(parser, no_cond_i, TYPE_BOOL);
    }

    if (!parser_match(parser, &dummy, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    const int current_scope_i = parser->par_scope_i;
    TRY_OK(parser_parse_control_structure_body(parser, &no_then_i));
    CHECK(no_then_i, >=, 0, "%d");
    CHECK(no_then_i, <, (int)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const no_then = &parser->par_nodes[no_then_i];
    const int then_type_i = no_then->no_type_i;
    CHECK(then_type_i, >=, 0, "%d");
    CHECK(then_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t then_type_kind =
        parser->par_types[then_type_i].ty_kind;

    // Else is optional

    if (parser_match(parser, &dummy, 1, TOK_ID_ELSE)) {
        TRY_OK(parser_parse_control_structure_body(parser, &no_else_i));
        CHECK(no_else_i, >=, 0, "%d");
        CHECK(no_else_i, <, (int)buf_size(parser->par_nodes), "%d");

        const mkt_node_t* const no_else = &parser->par_nodes[no_else_i];

        CHECK(no_else->no_type_i, >=, 0, "%d");
        CHECK(no_else->no_type_i, <, (int)buf_size(parser->par_types), "%d");
        const mkt_type_kind_t else_type_kind =
            parser->par_types[no_else->no_type_i].ty_kind;

        // Unit gets a pass for now (until we have assignements)
        if (then_type_kind != else_type_kind && then_type_kind != TYPE_UNIT &&
            else_type_kind != TYPE_UNIT) {
            log_debug("if branch types don't match, got %s and %s",
                      mkt_type_to_str[then_type_kind],
                      mkt_type_to_str[else_type_kind]);
            return parser_err_non_matching_types(parser, no_then_i, no_else_i);
        }
    }

    buf_push(
        parser->par_nodes,
        ((mkt_node_t){
            .no_kind = NODE_IF,
            .no_type_i = then_type_i,
            .no_n = {.no_if = ((mkt_if_t){.if_first_tok_i = first_tok_i,
                                          .if_last_tok_i = last_tok_i,
                                          .if_node_cond_i = no_cond_i,
                                          .if_node_then_i = no_then_i,
                                          .if_node_else_i = no_else_i})}}));
    *new_node_i = (int)buf_size(parser->par_nodes) - 1;

    // Reset current scope
    parser->par_scope_i = current_scope_i;

    return RES_OK;
}

static mkt_res_t parser_parse_jump_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int tok_i = -1;
    mkt_res_t res = RES_NONE;
    if (parser_match(parser, &tok_i, 1, TOK_ID_RETURN)) {
        int expr_node_i = -1;
        res = parser_parse_expr(parser, &expr_node_i);
        if (res != RES_OK && res != RES_NONE) return res;

        const int type_i = expr_node_i >= 0
                               ? parser->par_nodes[expr_node_i].no_type_i
                               : TYPE_UNIT_I;
        CHECK(type_i, >=, 0, "%d");
        CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

        const mkt_node_t* const expr =
            expr_node_i >= 0 ? &parser->par_nodes[expr_node_i] : NULL;
        buf_push(parser->par_nodes,
                 ((mkt_node_t){
                     .no_type_i = type_i,
                     .no_kind = NODE_RETURN,
                     .no_n = {.no_unary = {.un_first_tok_i = tok_i,
                                           .un_last_tok_i =
                                               node_last_token(parser, expr),
                                           .un_node_i = expr_node_i}}}));
        *new_node_i = buf_size(parser->par_nodes) - 1;
        log_debug("New return %d type=%s", *new_node_i,
                  mkt_type_to_str[parser->par_types[type_i].ty_kind]);

        parser->par_nodes[parser->par_fn_i].no_n.no_fn_decl.fd_flags |=
            FN_FLAGS_SEEN_RETURN;

        return RES_OK;
    }

    return RES_NONE;
}

static mkt_res_t parser_parse_primary_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int tok_i = -1;
    mkt_res_t res = RES_NONE;

    // FIXME: hack
    if (parser_peek(parser) == TOK_ID_BUILTIN_PRINTLN)
        return parser_parse_builtin_println(parser, new_node_i);

    if (parser_peek(parser) == TOK_ID_SYSCALL)
        return parser_parse_syscall(parser, new_node_i);

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
        CHECK(tok_i, >=, 0, "%d");
        CHECK(tok_i, <, parser->par_lexer.lex_source_len, "%d");
        const mkt_pos_range_t pos_range =
            parser->par_lexer.lex_tok_pos_ranges[tok_i];

        CHECK(pos_range.pr_start, >=, 0, "%d");
        CHECK(pos_range.pr_start, <, parser->par_lexer.lex_source_len, "%d");
        const char* const source =
            &parser->par_lexer.lex_source[pos_range.pr_start];
        // The source is either `true` or `false` hence the len is either 4
        // or 5
        const int8_t val = (memcmp("true", source, 4) == 0);
        buf_push(parser->par_nodes,
                 ((mkt_node_t){.no_kind = NODE_LONG,
                               .no_type_i = TYPE_BOOL_I,
                               .no_n = {.no_num = (number_t){.nu_tok_i = tok_i,
                                                             .nu_val = val}}}));
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_STRING)) {
        const mkt_pos_range_t pos_range =
            parser->par_lexer.lex_tok_pos_ranges[tok_i];
        const bool multiline =
            parser->par_lexer.lex_source[pos_range.pr_end - 1] == '"' &&
            parser->par_lexer.lex_source[pos_range.pr_end - 2] == '"' &&
            pos_range.pr_end - pos_range.pr_start >= 3 &&
            parser->par_lexer.lex_source[pos_range.pr_end - 3] == '"';

        const mkt_node_t node = {
            .no_kind = NODE_STRING,
            .no_type_i = TYPE_STRING_I,
            .no_n = {
                .no_string = {.st_tok_i = tok_i, .st_multiline = multiline}}};
        buf_push(parser->par_nodes, node);
        *new_node_i = buf_size(parser->par_nodes) - 1;

        buf_push(parser->par_node_decls, *new_node_i);

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_LONG)) {
        int type_i = TYPE_ANY_I;
        const long long int val = parse_tok_to_long(parser, tok_i, &type_i);
        CHECK(type_i, >=, 0, "%d");
        CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

        buf_push(parser->par_nodes,
                 ((mkt_node_t){.no_kind = NODE_LONG,
                               .no_type_i = type_i,
                               .no_n = {.no_num = (number_t){.nu_tok_i = tok_i,
                                                             .nu_val = val}}}));
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_CHAR)) {
        const long long int val = parse_tok_to_char(parser, tok_i);
        buf_push(parser->par_nodes,
                 ((mkt_node_t){.no_kind = NODE_CHAR,
                               .no_type_i = TYPE_CHAR_I,
                               .no_n = {.no_num = (number_t){.nu_tok_i = tok_i,
                                                             .nu_val = val}}}));
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_match(parser, &tok_i, 1, TOK_ID_IDENTIFIER)) {
        int no_def_i = -1;
        if (parser_resolve_var(parser, tok_i, &no_def_i) != RES_OK) {
            const char* src = NULL;
            int src_len = 0;
            parser_tok_source(parser, tok_i, &src, &src_len);
            const mkt_loc_t loc = parser->par_lexer.lex_locs[tok_i];
            fprintf(stderr, "%s%s:%d:%sUndefined variable %.*s\n",
                    mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                    loc.loc_line, mkt_colors[is_tty][COL_RESET], src_len, src);
            parser_print_source_on_error(parser, tok_i, tok_i);
            return RES_UNKNOWN_VAR;
        }

        const mkt_node_t* const no_def = &parser->par_nodes[no_def_i];
        CHECK((void*)no_def, !=, NULL, "%p");

        const int type_i = no_def->no_type_i;
        CHECK(type_i, >=, 0, "%d");
        CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

        buf_push(
            parser->par_nodes,
            ((mkt_node_t){.no_kind = NODE_VAR,
                          .no_type_i = type_i,
                          .no_n = {.no_var = {.va_tok_i = tok_i,
                                              .va_var_node_i = no_def_i}}}));
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    if (parser_peek(parser) == TOK_ID_IF)
        return parser_parse_if_expr(parser, new_node_i);
    if (parser_peek(parser) == TOK_ID_RETURN)
        return parser_parse_jump_expr(parser, new_node_i);

    return RES_NONE;  // TODO
}

static mkt_res_t parser_parse_navigation_suffix(parser_t* parser, int lhs_i,
                                                int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) != TOK_ID_DOT) return RES_NONE;

    int dummy = -1;
    CHECK(parser_match(parser, &dummy, 1, TOK_ID_DOT), ==, true, "%d");

    int rhs_i = -1;
    int member_tok_i = -1;
    if (!parser_match(parser, &member_tok_i, 1, TOK_ID_IDENTIFIER)) {
        UNIMPLEMENTED();
    }

    const mkt_node_t* const lhs = &parser->par_nodes[lhs_i];
    const mkt_type_t lhs_type = parser->par_types[lhs->no_type_i];

    if (lhs->no_kind != NODE_INSTANCE) {
        const char* rhs_src = NULL;
        int rhs_src_len = 0;
        parser_tok_source(parser, member_tok_i, &rhs_src, &rhs_src_len);

        const mkt_loc_t loc = parser->par_lexer.lex_locs[member_tok_i];
        fprintf(stderr,
                "%s%s:%d:%sTrying to access member %.*s of non-instance type "
                "(%s)\n",
                mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                loc.loc_line, mkt_colors[is_tty][COL_RESET], rhs_src_len,
                rhs_src, mkt_type_to_str[lhs_type.ty_kind]);
        parser_print_source_on_error(parser, member_tok_i, member_tok_i);
        return RES_UNKNOWN_VAR;
    }

    const mkt_instance_t instance = lhs->no_n.no_instance;
    const mkt_class_decl_t class_decl =
        parser->par_nodes[instance.in_class].no_n.no_class_decl;
    if (parser_resolve_member(parser, member_tok_i, &class_decl, &rhs_i) !=
        RES_OK) {
        const char* src = NULL;
        int src_len = 0;
        parser_tok_source(parser, member_tok_i, &src, &src_len);
        const mkt_loc_t loc = parser->par_lexer.lex_locs[member_tok_i];
        fprintf(stderr, "%s%s:%d:%sUndefined member %.*s\n",
                mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                loc.loc_line, mkt_colors[is_tty][COL_RESET], src_len, src);
        parser_print_source_on_error(parser, member_tok_i, member_tok_i);
        return RES_UNKNOWN_VAR;
    }

    const mkt_node_t* const rhs = &parser->par_nodes[rhs_i];
    CHECK((void*)rhs, !=, NULL, "%p");

    const int type_i = rhs->no_type_i;
    CHECK(type_i, >=, 0, "%d");
    CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

    buf_push(parser->par_nodes, ((mkt_node_t){.no_kind = NODE_MEMBER_GET,
                                              .no_type_i = rhs->no_type_i,
                                              .no_n = {.no_binary = {
                                                           .bi_lhs_i = lhs_i,
                                                           .bi_rhs_i = rhs_i,
                                                       }}}));
    *new_node_i = buf_size(parser->par_nodes) - 1;

    return RES_OK;
}

static mkt_res_t parser_parse_postfix_unary_suffix(parser_t* parser, int lhs_i,
                                                   int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    TRY_NONE(parser_parse_call_suffix(parser, lhs_i, new_node_i));

    return parser_parse_navigation_suffix(parser, lhs_i, new_node_i);
}

static mkt_res_t parser_parse_postfix_unary_expr(parser_t* parser,
                                                 int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    TRY_OK(parser_parse_primary_expr(parser, new_node_i));

    mkt_res_t res = RES_NONE;
    while ((res = parser_parse_postfix_unary_suffix(parser, *new_node_i,
                                                    new_node_i)) == RES_OK) {
    }
    if (res == RES_NONE) return RES_OK;

    return res;
}

static mkt_res_t parser_parse_prefix_unary_expr(parser_t* parser,
                                                int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int tok_i = -1;
    if (parser_match(parser, &tok_i, 1, TOK_ID_NOT)) {
        int no_i = -1;

        TRY_OK(parser_parse_postfix_unary_expr(parser, &no_i));

        const int type_i = parser->par_nodes[no_i].no_type_i;
        CHECK(type_i, >=, 0, "%d");
        CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");
        const mkt_type_kind_t type_kind = parser->par_types[type_i].ty_kind;
        if (type_kind != TYPE_BOOL) {
            return parser_err_unexpected_type(parser, no_i, TYPE_BOOL);
        }

        CHECK(no_i, >=, 0, "%d");
        CHECK(no_i, <, (int)buf_size(parser->par_nodes), "%d");
        const mkt_node_t* const node = &parser->par_nodes[no_i];

        buf_push(parser->par_nodes,
                 ((mkt_node_t){
                     .no_type_i = type_i,
                     .no_kind = NODE_NOT,
                     .no_n = {.no_unary = {.un_first_tok_i = tok_i,
                                           .un_last_tok_i =
                                               node_last_token(parser, node),
                                           .un_node_i = no_i}}}));
        *new_node_i = no_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return parser_parse_postfix_unary_expr(parser, new_node_i);
}

static mkt_res_t parser_parse_as_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_prefix_unary_expr(parser, new_node_i);
}

static mkt_res_t parser_parse_multiplicative_expr(parser_t* parser,
                                                  int* new_node_i) {
    mkt_res_t res = RES_NONE;

    int lhs_i = -1;
    TRY_OK(parser_parse_as_expr(parser, &lhs_i));
    CHECK(lhs_i, >=, 0, "%d");
    CHECK(lhs_i, <, (int)buf_size(parser->par_nodes), "%d");

    const int lhs_type_i = parser->par_nodes[lhs_i].no_type_i;
    CHECK(lhs_type_i, >=, 0, "%d");
    CHECK(lhs_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;

    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 3, TOK_ID_STAR, TOK_ID_SLASH,
                        TOK_ID_PERCENT)) {
        if (lhs_type_kind != TYPE_LONG && lhs_type_kind != TYPE_INT &&
            lhs_type_kind != TYPE_SHORT && lhs_type_kind != TYPE_BYTE) {
            log_debug("non matching types: lhs should be numerical, was: %s",
                      mkt_type_to_str[lhs_type_kind]);
            return parser_err_unexpected_type(parser, lhs_i, TYPE_LONG);
        }

        const int tok_i = parser->par_tok_i - 1;
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        res = parser_parse_as_expr(parser, &rhs_i);
        if (res == RES_NONE)
            return parser_err_missing_rhs(parser, tok_i, parser->par_tok_i);
        else if (res != RES_OK)
            return res;

        CHECK(rhs_i, >=, 0, "%d");
        CHECK(rhs_i, <, (int)buf_size(parser->par_nodes), "%d");

        const int rhs_type_i = parser->par_nodes[rhs_i].no_type_i;
        CHECK(rhs_type_i, >=, 0, "%d");
        CHECK(rhs_type_i, <, (int)buf_size(parser->par_types), "%d");

        const mkt_type_kind_t rhs_type_kind =
            parser->par_types[rhs_type_i].ty_kind;

        if (rhs_type_kind != TYPE_LONG && rhs_type_kind != TYPE_INT &&
            rhs_type_kind != TYPE_SHORT && rhs_type_kind != TYPE_BYTE) {
            log_debug("non matching types: rhs should be numerical, was: %s",
                      mkt_type_to_str[rhs_type_kind]);
            return parser_err_unexpected_type(parser, lhs_i, TYPE_LONG);
        }

        if (lhs_type_kind != rhs_type_kind) {
            log_debug("non matching types: lhs=%s rhs=%s",
                      mkt_type_to_str[lhs_type_kind],
                      mkt_type_to_str[rhs_type_kind]);
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);
        }

        buf_push(parser->par_nodes,
                 ((mkt_node_t){
                     .no_kind = (tok_id == TOK_ID_STAR)
                                    ? NODE_MULTIPLY
                                    : (tok_id == TOK_ID_SLASH ? NODE_DIVIDE
                                                              : NODE_MODULO),
                     .no_type_i = lhs_type_i,
                     .no_n = {.no_binary = ((mkt_binary_t){
                                  .bi_lhs_i = lhs_i, .bi_rhs_i = rhs_i})}}));
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return RES_OK;
}

static mkt_res_t parser_parse_additive_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int lhs_i = -1;
    TRY_OK(parser_parse_multiplicative_expr(parser, &lhs_i));

    CHECK(lhs_i, >=, 0, "%d");
    CHECK(lhs_i, <, (int)buf_size(parser->par_nodes), "%d");

    const int lhs_type_i = parser->par_nodes[lhs_i].no_type_i;
    CHECK(lhs_type_i, >=, 0, "%d");
    CHECK(lhs_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 2, TOK_ID_PLUS, TOK_ID_MINUS)) {
        const int tok_i = parser->par_tok_i - 1;
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;

        mkt_res_t res = parser_parse_multiplicative_expr(parser, &rhs_i);
        if (res == RES_NONE)
            return parser_err_missing_rhs(parser, tok_i, parser->par_tok_i);
        else if (res != RES_OK)
            return res;

        CHECK(rhs_i, >=, 0, "%d");
        CHECK(rhs_i, <, (int)buf_size(parser->par_nodes), "%d");

        const int rhs_type_i = parser->par_nodes[rhs_i].no_type_i;
        CHECK(rhs_type_i, >=, 0, "%d");
        CHECK(rhs_type_i, <, (int)buf_size(parser->par_types), "%d");

        const mkt_type_kind_t rhs_type_kind =
            parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        buf_push(
            parser->par_nodes,
            ((mkt_node_t){
                .no_kind = tok_id == TOK_ID_PLUS ? NODE_ADD : NODE_SUBTRACT,
                .no_type_i = lhs_type_i,
                .no_n = {.no_binary = ((mkt_binary_t){.bi_lhs_i = lhs_i,
                                                      .bi_rhs_i = rhs_i})}}));
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return RES_OK;
}

static mkt_res_t parser_parse_range_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_additive_expr(parser, new_node_i);
}

static mkt_res_t parser_parse_infix_fn_call(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_range_expr(parser, new_node_i);
}

static mkt_res_t parser_parse_elvis_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_infix_fn_call(parser, new_node_i);
}

static mkt_res_t parser_parse_infix_op(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_elvis_expr(parser, new_node_i);
}

static mkt_res_t parser_parse_value_arg(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_expr(parser, new_node_i);
}

static mkt_res_t parser_parse_value_args(parser_t* parser, int* last_tok_i,
                                         int** arg_nodes_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)arg_nodes_i, !=, NULL, "%p");
    CHECK((void*)last_tok_i, !=, NULL, "%p");

    if (parser_peek(parser) != TOK_ID_LPAREN) return RES_NONE;

    int dummy = -1;
    parser_match(parser, &dummy, 1, TOK_ID_LPAREN);

    if (parser_match(parser, &dummy, 1, TOK_ID_RPAREN)) return RES_OK;

    do {
        int new_node_i = -1;
        mkt_res_t res = parser_parse_value_arg(parser, &new_node_i);
        if (res == RES_OK)
            buf_push(*arg_nodes_i, new_node_i);
        else if (res == RES_NONE)
            break;
        else
            return res;
    } while (parser_match(parser, &dummy, 1, TOK_ID_COMMA));

    if (!parser_match(parser, last_tok_i, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    return RES_OK;
}

static mkt_res_t parser_parse_call_suffix(parser_t* parser, int lhs_i,
                                          int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int* arg_nodes_i = NULL;
    int last_tok_i = -1;
    mkt_res_t res = parser_parse_value_args(parser, &last_tok_i, &arg_nodes_i);
    if (res != RES_OK) return res;

    CHECK(*new_node_i, >=, 0, "%d");
    CHECK(*new_node_i, <, (int)buf_size(parser->par_nodes), "%d");
    const int type_i = parser->par_nodes[*new_node_i].no_type_i;
    CHECK(type_i, >=, 0, "%d");
    CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

    int callable_node_i = -1;

    res = parser_node_find_callable(parser, *new_node_i, &callable_node_i);
    const int first_tok_i =
        node_first_token(parser, &parser->par_nodes[*new_node_i]);
    if (res != RES_OK) {
        const char* src = NULL;
        int src_len = 0;
        parser_tok_source(parser, first_tok_i, &src, &src_len);
        const mkt_loc_t loc = parser->par_lexer.lex_locs[first_tok_i];
        const mkt_node_kind_t node_kind =
            parser->par_nodes[*new_node_i].no_kind;
        fprintf(stderr,
                "%s%s:%d:%s Trying to call a non-callable entity: %.*s (%s)\n",
                mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                loc.loc_line, mkt_colors[is_tty][COL_RESET], src_len, src,
                mkt_node_kind_to_str[node_kind]);
        parser_print_source_on_error(parser, first_tok_i, first_tok_i);
        return RES_ERR;
    }
    CHECK(callable_node_i, >=, 0, "%d");
    CHECK(callable_node_i, <, (int)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const callable_decl_node =
        &parser->par_nodes[callable_node_i];
    CHECK((void*)callable_decl_node, !=, NULL, "%p");
    if (callable_decl_node->no_kind == NODE_FN_DECL) {
        const mkt_fn_decl_t fn_decl = callable_decl_node->no_n.no_fn_decl;
        const int declared_arity = buf_size(fn_decl.fd_arg_nodes_i);
        const int found_arity = buf_size(arg_nodes_i);
        if (declared_arity != found_arity) UNIMPLEMENTED();  // TODO: err

        const int current_scope_i =
            parser_scope_begin(parser, fn_decl.fd_body_node_i);

        for (int i = 0; i < (int)buf_size(fn_decl.fd_arg_nodes_i); i++) {
            const int decl_arg_i = fn_decl.fd_arg_nodes_i[i];
            const mkt_node_t* const decl_arg = &parser->par_nodes[decl_arg_i];
            const mkt_type_kind_t decl_type_kind =
                parser->par_types[decl_arg->no_type_i].ty_kind;
            const int found_arg_i = arg_nodes_i[i];
            const mkt_node_t* const found_arg = &parser->par_nodes[found_arg_i];
            const mkt_type_kind_t found_type_kind =
                parser->par_types[found_arg->no_type_i].ty_kind;

            if (decl_type_kind != found_type_kind)
                return parser_err_non_matching_types(parser, decl_arg_i,
                                                     found_arg_i);

            buf_push(parser->par_nodes,
                     ((mkt_node_t){
                         .no_kind = NODE_ASSIGN,
                         .no_type_i = TYPE_UNIT_I,
                         .no_n = {.no_binary = {.bi_lhs_i = decl_arg_i,
                                                .bi_rhs_i = arg_nodes_i[i]}}}));
        }
        parser_scope_end(parser, current_scope_i);
        buf_push(
            parser->par_nodes,
            ((mkt_node_t){.no_type_i = type_i,
                          .no_kind = NODE_CALL,
                          .no_n = {.no_call = {.ca_arg_nodes_i = arg_nodes_i,
                                               .ca_lhs_node_i = lhs_i}}}));
    } else if (callable_decl_node->no_kind == NODE_CLASS_DECL) {
        buf_push(parser->par_nodes,
                 ((mkt_node_t){.no_kind = NODE_INSTANCE,
                               .no_type_i = callable_decl_node->no_type_i,
                               .no_n = {.no_instance = {
                                            .in_class = callable_node_i,
                                            .in_first_tok_i = first_tok_i,
                                            .in_last_tok_i = last_tok_i,
                                        }}}));
    } else
        UNREACHABLE();

    *new_node_i = buf_size(parser->par_nodes) - 1;

    return RES_OK;
}

static mkt_res_t parser_parse_generical_call_like_comparison(parser_t* parser,
                                                             int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_infix_op(parser, new_node_i);
}

static mkt_res_t parser_parse_comparison(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int lhs_i = -1;
    TRY_OK(parser_parse_generical_call_like_comparison(parser, &lhs_i));

    CHECK(lhs_i, >=, 0, "%d");
    CHECK(lhs_i, <, (int)buf_size(parser->par_nodes), "%d");

    const int lhs_type_i = parser->par_nodes[lhs_i].no_type_i;
    CHECK(lhs_type_i, >=, 0, "%d");
    CHECK(lhs_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 4, TOK_ID_LT, TOK_ID_LE, TOK_ID_GT,
                        TOK_ID_GE)) {
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        TRY_OK(parser_parse_generical_call_like_comparison(parser, &rhs_i));

        CHECK(rhs_i, >=, 0, "%d");
        CHECK(rhs_i, <, (int)buf_size(parser->par_nodes), "%d");

        const int rhs_type_i = parser->par_nodes[rhs_i].no_type_i;
        CHECK(rhs_type_i, >=, 0, "%d");
        CHECK(rhs_type_i, <, (int)buf_size(parser->par_types), "%d");

        const mkt_type_kind_t rhs_type_kind =
            parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        if (tok_id == TOK_ID_LT)
            buf_push(
                parser->par_nodes,
                ((mkt_node_t){
                    .no_kind = NODE_LT,
                    .no_type_i = TYPE_BOOL_I,
                    .no_n = {.no_binary = ((mkt_binary_t){
                                 .bi_lhs_i = lhs_i, .bi_rhs_i = rhs_i})}}));
        else if (tok_id == TOK_ID_LE)
            buf_push(
                parser->par_nodes,
                ((mkt_node_t){
                    .no_kind = NODE_LE,
                    .no_type_i = TYPE_BOOL_I,
                    .no_n = {.no_binary = ((mkt_binary_t){
                                 .bi_lhs_i = lhs_i, .bi_rhs_i = rhs_i})}}));
        else if (tok_id == TOK_ID_GE)
            buf_push(
                parser->par_nodes,
                ((mkt_node_t){
                    .no_kind = NODE_LE,
                    .no_type_i = TYPE_BOOL_I,
                    .no_n = {.no_binary = ((mkt_binary_t){
                                 .bi_lhs_i = rhs_i, .bi_rhs_i = lhs_i})}}));
        else if (tok_id == TOK_ID_GT)
            buf_push(
                parser->par_nodes,
                ((mkt_node_t){
                    .no_kind = NODE_LT,
                    .no_type_i = TYPE_BOOL_I,
                    .no_n = {.no_binary = ((mkt_binary_t){
                                 .bi_lhs_i = rhs_i, .bi_rhs_i = lhs_i})}}));
        else
            UNREACHABLE();

        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return RES_OK;
}

static mkt_res_t parser_parse_equality(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int lhs_i = -1;
    TRY_OK(parser_parse_comparison(parser, &lhs_i));

    CHECK(lhs_i, >=, 0, "%d");
    CHECK(lhs_i, <, (int)buf_size(parser->par_nodes), "%d");

    const int lhs_type_i = parser->par_nodes[lhs_i].no_type_i;
    CHECK(lhs_type_i, >=, 0, "%d");
    CHECK(lhs_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t lhs_type_kind = parser->par_types[lhs_type_i].ty_kind;
    *new_node_i = lhs_i;

    while (parser_match(parser, new_node_i, 2, TOK_ID_EQ_EQ, TOK_ID_NEQ)) {
        const int tok_id = parser_previous(parser);

        int rhs_i = -1;
        TRY_OK(parser_parse_comparison(parser, &rhs_i));

        CHECK(rhs_i, >=, 0, "%d");
        CHECK(rhs_i, <, (int)buf_size(parser->par_nodes), "%d");

        const int rhs_type_i = parser->par_nodes[rhs_i].no_type_i;
        CHECK(rhs_type_i, >=, 0, "%d");
        CHECK(rhs_type_i, <, (int)buf_size(parser->par_types), "%d");

        const mkt_type_kind_t rhs_type_kind =
            parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind)
            return parser_err_non_matching_types(parser, lhs_i, rhs_i);

        buf_push(parser->par_nodes,
                 ((mkt_node_t){
                     .no_kind = tok_id == TOK_ID_EQ_EQ ? NODE_EQ : NODE_NEQ,
                     .no_type_i = TYPE_BOOL_I,
                     .no_n = {.no_binary = ((mkt_binary_t){
                                  .bi_lhs_i = lhs_i, .bi_rhs_i = rhs_i})}}));
        *new_node_i = lhs_i = (int)buf_size(parser->par_nodes) - 1;
    }

    return RES_OK;
}

static mkt_res_t parser_parse_conjunction(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_equality(parser, new_node_i);
}

static mkt_res_t parser_parse_disjunction(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_conjunction(parser, new_node_i);
}

static mkt_res_t parser_parse_expr(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    return parser_parse_disjunction(parser, new_node_i);
}

static mkt_res_t parser_parse_stmts(parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");

    mkt_res_t res = RES_NONE;
    while (1) {
        int new_node_i = -1;
        res = parser_parse_stmt(parser, &new_node_i);
        if (res == RES_OK) {
            buf_push(
                parser->par_nodes[parser->par_scope_i].no_n.no_block.bl_nodes_i,
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

static mkt_res_t parser_parse_block(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    buf_push(parser->par_nodes,
             ((mkt_node_t){.no_kind = NODE_BLOCK,
                           .no_type_i = TYPE_ANY_I,
                           .no_n = {.no_block = {.bl_first_tok_i = -1,
                                                 .bl_last_tok_i = -1,
                                                 .bl_nodes_i = NULL,
                                                 .bl_parent_scope_i =
                                                     parser->par_scope_i}}}));
    *new_node_i = buf_size(parser->par_nodes) - 1;
    const int parent_scope_i = parser_scope_begin(parser, *new_node_i);

    if (!parser_match(
            parser,
            &parser->par_nodes[*new_node_i].no_n.no_block.bl_first_tok_i, 1,
            TOK_ID_LCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_LCURLY);

    TRY_OK(parser_parse_stmts(parser));

    if (!parser_match(
            parser, &parser->par_nodes[*new_node_i].no_n.no_block.bl_last_tok_i,
            1, TOK_ID_RCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_RCURLY);

    const int* const nodes_i =
        parser->par_nodes[parser->par_scope_i].no_n.no_block.bl_nodes_i;
    const int last_node_i =
        buf_size(nodes_i) > 0 ? nodes_i[buf_size(nodes_i) - 1] : -1;
    CHECK(last_node_i, <, (int)buf_size(parser->par_nodes), "%d");

    const int type_i = last_node_i >= 0
                           ? parser->par_nodes[last_node_i].no_type_i
                           : TYPE_UNIT_I;
    CHECK(type_i, >=, 0, "%d");
    CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

    parser->par_nodes[*new_node_i].no_type_i = type_i;

    parser_scope_end(parser, parent_scope_i);

    log_debug("block=%d parent=%d last_node_i=%d type=%s last_tok_i=%d",
              *new_node_i, parent_scope_i, last_node_i,
              mkt_type_to_str[parser->par_types[type_i].ty_kind],
              parser->par_nodes[*new_node_i].no_n.no_block.bl_last_tok_i);

    return RES_OK;
}

static mkt_res_t parser_parse_control_structure_body(parser_t* parser,
                                                     int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) == TOK_ID_LCURLY)
        return parser_parse_block(parser, new_node_i);

    return parser_parse_stmt(parser, new_node_i);
}

static mkt_res_t parser_parse_syscall(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int keyword_i = -1;
    mkt_res_t res = RES_NONE;
    if (parser_match(parser, &keyword_i, 1, TOK_ID_SYSCALL)) {
        int* arg_nodes_i = NULL;
        int last_tok_i = 0;
        res = parser_parse_value_args(parser, &last_tok_i, &arg_nodes_i);
        if (res == RES_NONE) {
            fprintf(stderr, "Missing syscall arguments");
            return RES_MISSING_PARAM;
        } else if (res != RES_OK)
            return res;

        if (buf_size(arg_nodes_i) == 0) {
            fprintf(stderr, "Syscall without arguments invalid");
            return RES_MISSING_PARAM;
        }

        buf_push(parser->par_nodes,
                 ((mkt_node_t){
                     .no_kind = NODE_SYSCALL,
                     .no_type_i = TYPE_UNIT_I,  // FIXME
                     .no_n = {.no_syscall = {.sy_first_tok_i = keyword_i,
                                             .sy_last_tok_i = last_tok_i,
                                             .sy_arg_nodes_i = arg_nodes_i}}}));
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    return RES_NONE;
}

static mkt_res_t parser_parse_builtin_println(parser_t* parser,
                                              int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int keyword_print_i = -1;
    mkt_res_t res = RES_NONE;
    // Temporary
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

        buf_push(
            parser->par_nodes,
            ((mkt_node_t){.no_kind = NODE_BUILTIN_PRINTLN,
                          .no_type_i = TYPE_UNIT_I,
                          .no_n = {.no_builtin_println = {
                                       .bp_arg_i = arg_i,
                                       .bp_keyword_print_i = keyword_print_i,
                                       .bp_rparen_i = rparen}}}));
        *new_node_i = (int)buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }
    return RES_NONE;
}

static mkt_res_t parser_parse_assignment(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    mkt_res_t res = RES_NONE;
    int dummy = -1, expr_node_i = -1, lhs_tok_i = -1;
    if (parser_peek(parser) == TOK_ID_IDENTIFIER &&
        parser_peek_next(parser) == TOK_ID_EQ) {
        parser_match(parser, &lhs_tok_i, 1, TOK_ID_IDENTIFIER);

        int no_def_i = -1;
        if (parser_resolve_var(parser, lhs_tok_i, &no_def_i) != RES_OK) {
            return RES_UNKNOWN_VAR;
        }
        CHECK(no_def_i, >=, 0, "%d");
        CHECK(no_def_i, <, (int)buf_size(parser->par_nodes), "%d");

        const mkt_node_t* const no_def = &parser->par_nodes[no_def_i];
        if (no_def->no_kind != NODE_VAR_DEF) {
            UNIMPLEMENTED();  // err
        }
        CHECK((void*)no_def, !=, NULL, "%p");

        const mkt_var_def_t var_def = no_def->no_n.no_var_def;
        if (var_def.vd_flags & MKT_VAR_FLAGS_VAL)
            return parser_err_assigning_val(parser, lhs_tok_i, &var_def);

        const int type_i = no_def->no_type_i;
        CHECK(type_i, >=, 0, "%d");
        CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

        buf_push(
            parser->par_nodes,
            ((mkt_node_t){.no_kind = NODE_VAR,
                          .no_type_i = type_i,
                          .no_n = {.no_var = {.va_tok_i = lhs_tok_i,
                                              .va_var_node_i = no_def_i}}}));
        int lhs_node_i = (int)buf_size(parser->par_nodes) - 1;

        parser_match(parser, &dummy, 1, TOK_ID_EQ);

        res = parser_parse_expr(parser, &expr_node_i);
        if (res == RES_NONE) {
            log_debug("Missing assignment rhs %d", lhs_node_i);
            return RES_EXPECTED_PRIMARY;
        } else if (res != RES_OK)
            return res;

        const int lhs_type_i = parser->par_nodes[lhs_node_i].no_type_i;
        CHECK(lhs_type_i, >=, 0, "%d");
        CHECK(lhs_type_i, <, (int)buf_size(parser->par_types), "%d");

        const int rhs_type_i = parser->par_nodes[expr_node_i].no_type_i;
        CHECK(rhs_type_i, >=, 0, "%d");
        CHECK(rhs_type_i, <, (int)buf_size(parser->par_types), "%d");

        const mkt_type_kind_t lhs_type_kind =
            parser->par_types[lhs_type_i].ty_kind;
        const mkt_type_kind_t rhs_type_kind =
            parser->par_types[rhs_type_i].ty_kind;

        if (lhs_type_kind != rhs_type_kind) {
            return parser_err_non_matching_types(parser, expr_node_i,
                                                 lhs_node_i);
        }

        buf_push(parser->par_nodes,
                 ((mkt_node_t){.no_kind = NODE_ASSIGN,
                               .no_type_i = type_i,
                               .no_n = {.no_binary = ((mkt_binary_t){
                                            .bi_lhs_i = lhs_node_i,
                                            .bi_rhs_i = expr_node_i})}}));
        *new_node_i = buf_size(parser->par_nodes) - 1;

        return RES_OK;
    }

    return RES_NONE;
}

static mkt_res_t parser_parse_property_declaration(parser_t* parser,
                                                   int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    if (!(parser_peek(parser) == TOK_ID_VAL ||
          parser_peek(parser) == TOK_ID_VAR))
        return RES_NONE;

    int first_tok_i = -1, name_tok_i = -1, type_tok_i = -1, dummy = -1,
        last_tok_i = -1, init_node_i = -1;

    unsigned short flags = 0;
    if (parser_match(parser, &first_tok_i, 1, TOK_ID_VAR))
        flags = MKT_VAR_FLAGS_VAR;
    else if (parser_match(parser, &first_tok_i, 1, TOK_ID_VAL))
        flags = MKT_VAR_FLAGS_VAL;
    else
        return RES_NONE;

    if (!parser_match(parser, &name_tok_i, 1, TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

    if (!parser_match(parser, &dummy, 1, TOK_ID_COLON))
        return parser_err_unexpected_token(parser, TOK_ID_COLON);

    if (!parser_match(parser, &type_tok_i, 1, TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);
    CHECK(type_tok_i, >=, 0, "%d");

    if (!parser_match(parser, &dummy, 1, TOK_ID_EQ))
        return parser_err_unexpected_token(parser, TOK_ID_EQ);

    mkt_res_t res = RES_NONE;
    res = parser_parse_expr(parser, &init_node_i);
    if (res == RES_NONE) {
        log_debug("var def without initial value%s", "");
        UNIMPLEMENTED();  // TODO make meaningful error
    }
    if (res != RES_OK) return res;

    int type_i = -1;
    if (!parser_parse_identifier_to_type_kind(parser, type_tok_i, &type_i)) {
        const char* src = NULL;
        int src_len = 0;
        parser_tok_source(parser, type_tok_i, &src, &src_len);

        const mkt_loc_t loc = parser->par_lexer.lex_locs[type_tok_i];

        fprintf(stderr, "%s%s:%d:%d:%s Unknown type %.*s\n",
                mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                loc.loc_line, loc.loc_column, mkt_colors[is_tty][COL_RESET],
                src_len, src);
        parser_print_source_on_error(parser, type_tok_i, type_tok_i);

        return RES_ERR;
    }
    CHECK(type_i, >=, 0, "%d");
    CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_t type = parser->par_types[type_i];
    log_debug("parsed type %s size=%d", mkt_type_to_str[type.type_kind],
              type.ty_size);

    int offset = 0;
    if (parser->par_fn_i >= 0) {
        CHECK(parser->par_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
        parser->par_nodes[parser->par_fn_i].no_n.no_fn_decl.fd_stack_size +=
            type.ty_size;

        offset =
            parser->par_nodes[parser->par_fn_i].no_n.no_fn_decl.fd_stack_size;
    }

    buf_push(
        parser->par_nodes,
        ((mkt_node_t){.no_kind = NODE_VAR_DEF,
                      .no_type_i = type_i,
                      .no_n = {.no_var_def = {.vd_first_tok_i = first_tok_i,
                                              .vd_last_tok_i = last_tok_i,
                                              .vd_name_tok_i = name_tok_i,
                                              .vd_init_node_i = init_node_i,
                                              .vd_stack_offset = offset,
                                              .vd_flags = flags}}}));
    *new_node_i = buf_size(parser->par_nodes) - 1;

    log_debug("new var def=%d current_scope_i=%d flags=%d offset=%d fn=%d",
              *new_node_i, parser->par_scope_i, flags, offset,
              parser->par_fn_i);

    return RES_OK;
}

static mkt_res_t parser_parse_parameter(parser_t* parser, int** new_nodes_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_nodes_i, !=, NULL, "%p");

    int identifier_tok_i = -1, dummy = -1, type_tok_i = -1;

    if (parser_match(parser, &dummy, 1, TOK_ID_RPAREN)) return RES_OK;

    do {
        if (!parser_match(parser, &identifier_tok_i, 1, TOK_ID_IDENTIFIER))
            return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

        if (!parser_match(parser, &dummy, 1, TOK_ID_COLON))
            return parser_err_unexpected_token(parser, TOK_ID_COLON);

        if (!parser_match(parser, &type_tok_i, 1, TOK_ID_IDENTIFIER))
            return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

        int type_i = -1;
        if (!parser_parse_identifier_to_type_kind(parser, type_tok_i, &type_i))
            UNIMPLEMENTED();

        CHECK(type_i, >=, 0, "%d");
        CHECK(type_i, <, (int)buf_size(parser->par_types), "%d");

        const int type_size = parser->par_types[type_i].ty_size;

        CHECK(parser->par_fn_i, >=, 0, "%d");
        CHECK(parser->par_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
        parser->par_nodes[parser->par_fn_i].no_n.no_fn_decl.fd_stack_size +=
            type_size;

        const int offset =
            parser->par_nodes[parser->par_fn_i].no_n.no_fn_decl.fd_stack_size;

        buf_push(parser->par_nodes,
                 ((mkt_node_t){
                     .no_kind = NODE_VAR_DEF,
                     .no_type_i = type_i,
                     .no_n = {.no_var_def = {.vd_name_tok_i = identifier_tok_i,
                                             .vd_first_tok_i = identifier_tok_i,
                                             .vd_last_tok_i = type_tok_i,
                                             .vd_init_node_i = -1,
                                             .vd_stack_offset = offset,
                                             .vd_flags = MKT_VAR_FLAGS_VAR}}}));
        const int new_node_i = buf_size(parser->par_nodes) - 1;
        buf_push(*new_nodes_i, new_node_i);

        const char* source = NULL;
        int source_len = 0;
        parser_tok_source(parser, identifier_tok_i, &source, &source_len);
        CHECK((void*)source, !=, NULL, "%p");
        CHECK(source_len, <=, parser->par_lexer.lex_source_len, "%d");

        log_debug("New fn param: `%.*s` type=%s scope=%d offset=%d", source_len,
                  source, mkt_type_to_str[type_kind], parser->par_scope_i,
                  offset);
    } while (parser_match(parser, &dummy, 1, TOK_ID_COMMA));

    if (!parser_match(parser, &dummy, 1, TOK_ID_RPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_RPAREN);

    return RES_OK;
}

static mkt_res_t parser_parse_fn_value_params(parser_t* parser,
                                              int** new_nodes_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_nodes_i, !=, NULL, "%p");

    int dummy = -1;
    if (!parser_match(parser, &dummy, 1, TOK_ID_LPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

    TRY_OK(parser_parse_parameter(parser, new_nodes_i));

    return RES_OK;
}

static int parser_fn_begin(parser_t* parser, int first_tok_i, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    buf_push(
        parser->par_nodes,
        ((mkt_node_t){.no_type_i = TYPE_UNIT_I,
                      .no_kind = NODE_FN_DECL,
                      .no_n = {.no_fn_decl = {.fd_first_tok_i = first_tok_i,
                                              .fd_name_tok_i = -1,
                                              .fd_last_tok_i = -1,
                                              .fd_body_node_i = -1,
                                              .fd_flags = FN_FLAGS_PRIVATE,
                                              .fd_arg_nodes_i = NULL}}}));
    const int old_fn_i = parser->par_fn_i;
    parser->par_fn_i = *new_node_i = buf_size(parser->par_nodes) - 1;
    buf_push(parser->par_node_decls, *new_node_i);

    CHECK(parser->par_scope_i, >=, 0, "%d");
    CHECK(parser->par_scope_i, <, (int)buf_size(parser->par_nodes), "%d");
    buf_push(parser->par_nodes[parser->par_scope_i].no_n.no_block.bl_nodes_i,
             *new_node_i);

    return old_fn_i;
}

static int parser_block_enter(parser_t* parser, int current_fn_i) {
    CHECK((void*)parser, !=, NULL, "%p");

    buf_push(parser->par_nodes,
             ((mkt_node_t){.no_kind = NODE_BLOCK,
                           .no_type_i = TYPE_UNIT_I,
                           .no_n = {.no_block = {.bl_first_tok_i = -1,
                                                 .bl_last_tok_i = -1,
                                                 .bl_nodes_i = NULL,
                                                 .bl_parent_scope_i =
                                                     parser->par_scope_i}}}));
    const int body_node_i =
        parser->par_nodes[current_fn_i].no_n.no_fn_decl.fd_body_node_i =
            buf_size(parser->par_nodes) - 1;
    return body_node_i;
}

static mkt_res_t parser_parse_fn_declaration(parser_t* parser,
                                             int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) != TOK_ID_FUN) return RES_NONE;

    int first_tok_i = -1, dummy = -1, *arg_nodes_i = NULL;

    CHECK(parser_match(parser, &first_tok_i, 1, TOK_ID_FUN), ==, true, "%d");

    const int old_fn_i = parser_fn_begin(parser, first_tok_i, new_node_i);

    if (!parser_match(
            parser,
            &parser->par_nodes[*new_node_i].no_n.no_fn_decl.fd_name_tok_i, 1,
            TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

    // Qualifies as entrypoint?
    {
        unsigned short* const flags =
            &parser->par_nodes[*new_node_i].no_n.no_fn_decl.fd_flags;

        // TODO: validate flags

        const int name_tok_i =
            parser->par_nodes[*new_node_i].no_n.no_fn_decl.fd_name_tok_i;

        const char* name;
        int name_len = 0;
        parser_tok_source(parser, name_tok_i, &name, &name_len);

        const char name_main[] = "main";
        const int name_main_len = sizeof(name_main) - 1;

        if (name_len == name_main_len &&
            memcmp(name, name_main, name_len) == 0) {
            CHECK(parser->par_fn_i, !=, -1, "%d");
            parser->par_main_fn_i = *new_node_i;
            *flags |= FN_FLAGS_PUBLIC;
            *flags &= ~FN_FLAGS_PRIVATE;
        }
    }

    const int body_node_i = parser_block_enter(parser, *new_node_i);
    const int parent_scope_i = parser_scope_begin(parser, body_node_i);

    TRY_OK(parser_parse_fn_value_params(parser, &arg_nodes_i));

    parser->par_nodes[body_node_i].no_n.no_block.bl_nodes_i = arg_nodes_i;

    for (int i = 0; i < (int)buf_size(arg_nodes_i); i++)
        buf_push(parser->par_nodes[*new_node_i].no_n.no_fn_decl.fd_arg_nodes_i,
                 arg_nodes_i[i]);

    int declared_type_tok_i = -1, declared_type_i = TYPE_UNIT_I;
    if (parser_match(parser, &dummy, 1, TOK_ID_COLON)) {
        if (!parser_match(parser, &declared_type_tok_i, 1, TOK_ID_IDENTIFIER)) {
            return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);
        }
        if (!parser_parse_identifier_to_type_kind(parser, declared_type_tok_i,
                                                  &declared_type_i)) {
            parser_print_source_on_error(parser, declared_type_tok_i,
                                         declared_type_tok_i);
            log_debug(
                "Encountered user type in function signature, not yet "
                "implemented:%s",
                "");
            UNIMPLEMENTED();
        }
    }
    CHECK(declared_type_i, >=, 0, "%d");
    CHECK(declared_type_i, <, (int)buf_size(parser->par_types), "%d");
    parser->par_nodes[*new_node_i].no_type_i = declared_type_i;

    if (!parser_match(
            parser,
            &parser->par_nodes[body_node_i].no_n.no_block.bl_first_tok_i, 1,
            TOK_ID_LCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_LCURLY);

    TRY_OK(parser_parse_stmts(parser));
    parser->par_nodes[body_node_i].no_type_i =
        parser->par_nodes[buf_size(parser->par_nodes) - 1].no_type_i;

    if (!parser_match(
            parser, &parser->par_nodes[body_node_i].no_n.no_block.bl_last_tok_i,
            1, TOK_ID_RCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_RCURLY);

    const int actual_type_i = parser->par_nodes[body_node_i].no_type_i;
    CHECK(actual_type_i, >=, 0, "%d");
    CHECK(actual_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t actual_type =
        parser->par_types[actual_type_i].ty_kind;
    const mkt_type_kind_t declared_type =
        parser->par_types[declared_type_i].ty_kind;

    mkt_fn_decl_t* const fn_decl =
        &parser->par_nodes[*new_node_i].no_n.no_fn_decl;
    const int last_tok_i =
        parser->par_nodes[body_node_i].no_n.no_block.bl_last_tok_i;
    fn_decl->fd_last_tok_i = last_tok_i;

    log_debug("new fn decl=%d flags=%d body_node_i=%d type=%s arity=%d",
              *new_node_i, fn_decl->fd_flags, fn_decl->fd_body_node_i,
              mkt_type_to_str[declared_type],
              (int)buf_size(fn_decl->fd_arg_nodes_i));

    parser_scope_end(parser, parent_scope_i);

    const bool seen_return = fn_decl->fd_flags & FN_FLAGS_SEEN_RETURN;
    if (declared_type != TYPE_UNIT && !seen_return) {
        const mkt_loc_t loc = parser->par_lexer.lex_locs[last_tok_i];

        fprintf(stderr,
                "%s%s:%d:%d:%sThe function has declared to "
                "return %s but has no return\n",
                mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                loc.loc_line, loc.loc_column, mkt_colors[is_tty][COL_RESET],
                mkt_type_to_str[declared_type]);
        parser_print_source_on_error(parser, last_tok_i, last_tok_i);

        return RES_ERR;
    }
    if (declared_type == TYPE_UNIT && actual_type != TYPE_UNIT && !seen_return)
        return RES_OK;

    if (actual_type != declared_type)
        return parser_err_non_matching_types(
            parser, body_node_i,
            *new_node_i);  // TODO: implement custom error function

    parser->par_fn_i = old_fn_i;

    return RES_OK;
}

static mkt_res_t parser_parse_class_declaration(parser_t* parser,
                                                int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO: modifiers

    if (parser_peek(parser) != TOK_ID_CLASS) return RES_NONE;

    int first_tok_i = -1, name_tok_i = -1;
    CHECK(parser_match(parser, &first_tok_i, 1, TOK_ID_CLASS), ==, true, "%d");

    if (!parser_match(parser, &name_tok_i, 1, TOK_ID_IDENTIFIER))
        return parser_err_unexpected_token(parser, TOK_ID_IDENTIFIER);

    int old_class_i = -1, body_node_i = -1, parent_scope_i = -1;
    parser_class_begin(parser, first_tok_i, name_tok_i, new_node_i,
                       &old_class_i, &body_node_i, &parent_scope_i);
    CHECK(old_class_i, >=, 0, "%d");
    CHECK(body_node_i, >=, 0, "%d");
    CHECK(parent_scope_i, >=, 0, "%d");

    if (!parser_match(
            parser,
            &parser->par_nodes[body_node_i].no_n.no_block.bl_first_tok_i, 1,
            TOK_ID_LCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_LCURLY);

    int* members = NULL;
    int member = -1;
    mkt_res_t res = RES_NONE;
    int size = 0;
    while ((res = parser_parse_declaration(parser, &member)) == RES_OK) {
        buf_push(members, member);
        mkt_node_t* const node = &parser->par_nodes[member];
        if (node->no_kind == NODE_VAR_DEF)
            node->no_n.no_var_def.vd_stack_offset = size;
        size += /* runtime header */ sizeof(void*) +
                parser->par_types[node->no_type_i].ty_size;
    }

    // TODO: print error here?
    if (res != RES_NONE) return res;
    {
        mkt_node_t* const class_node = &parser->par_nodes[*new_node_i];
        class_node->no_n.no_class_decl.cl_members = members;
        parser->par_types[class_node->no_type_i].ty_size = size;
    }

    if (!parser_match(
            parser, &parser->par_nodes[body_node_i].no_n.no_block.bl_last_tok_i,
            1, TOK_ID_RCURLY))
        return parser_err_unexpected_token(parser, TOK_ID_RCURLY);

    mkt_class_decl_t* const class_decl =
        &parser->par_nodes[*new_node_i].no_n.no_class_decl;
    const int last_tok_i =
        parser->par_nodes[body_node_i].no_n.no_block.bl_last_tok_i;
    class_decl->cl_last_tok_i = last_tok_i;

    parser_scope_end(parser, parent_scope_i);
    parser->par_class_i = old_class_i;

    return RES_OK;
}

static mkt_res_t parser_parse_declaration(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    TRY_NONE(parser_parse_fn_declaration(parser, new_node_i));
    TRY_NONE(parser_parse_class_declaration(parser, new_node_i));
    TRY_NONE(parser_parse_property_declaration(parser, new_node_i));

    return RES_NONE;
}

static mkt_res_t parser_parse_while_stmt(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    int dummy = -1, first_tok_i = -1, last_tok_i = -1, cond_i = -1, body_i = -1;

    if (!parser_match(parser, &first_tok_i, 1, TOK_ID_WHILE)) return RES_NONE;

    if (!parser_match(parser, &dummy, 1, TOK_ID_LPAREN))
        return parser_err_unexpected_token(parser, TOK_ID_LPAREN);

    mkt_res_t res = parser_parse_expr(parser, &cond_i);
    if (res == RES_NONE) {
        log_debug("Missing while condition%s", "\n");
        return RES_ERR;
    } else if (res != RES_OK)
        return res;

    CHECK(cond_i, >=, 0, "%d");
    CHECK(cond_i, <, (int)buf_size(parser->par_nodes), "%d");

    const int cond_type_i = parser->par_nodes[cond_i].no_type_i;
    CHECK(cond_type_i, >=, 0, "%d");
    CHECK(cond_type_i, <, (int)buf_size(parser->par_types), "%d");

    const mkt_type_kind_t cond_type_kind =
        parser->par_types[cond_type_i].ty_kind;
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

    buf_push(parser->par_nodes,
             ((mkt_node_t){.no_kind = NODE_WHILE,
                           .no_type_i = TYPE_UNIT_I,
                           .no_n = {.no_while = {.wh_first_tok_i = first_tok_i,
                                                 .wh_last_tok_i = last_tok_i,
                                                 .wh_cond_i = cond_i,
                                                 .wh_body_i = body_i}}}));
    *new_node_i = buf_size(parser->par_nodes) - 1;

    log_debug("new while=%d current_scope_i=%d cond=%d body=%d", *new_node_i,
              parser->par_scope_i, cond_i, body_i);

    return RES_OK;
}

static mkt_res_t parser_parse_loop(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    // TODO

    return parser_parse_while_stmt(parser, new_node_i);
}

static mkt_res_t parser_parse_stmt(parser_t* parser, int* new_node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)new_node_i, !=, NULL, "%p");

    if (parser_peek(parser) == TOK_ID_EOF) return RES_NONE;

    TRY_NONE(parser_parse_declaration(parser, new_node_i));
    TRY_NONE(parser_parse_assignment(parser, new_node_i));
    TRY_NONE(parser_parse_loop(parser, new_node_i));

    return parser_parse_expr(parser, new_node_i);
}

static mkt_res_t parser_parse(parser_t* parser) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)parser->par_lexer.lex_tokens, !=, NULL, "%p");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, 0, "%d");
    CHECK((int)buf_size(parser->par_lexer.lex_tokens), >, parser->par_tok_i,
          "%d");

    while (!parser_is_at_end(parser)) {
        int new_node_i = -1;
        const mkt_res_t res = parser_parse_declaration(parser, &new_node_i);
        if (res == RES_OK) {
            buf_push(parser_current_block(parser)->no_n.no_block.bl_nodes_i,
                     new_node_i);
            continue;
        } else if (res == RES_NONE)
            break;
        else
            return res;
    }

    if (parser_peek(parser) != TOK_ID_EOF) {
        CHECK(parser->par_tok_i, <,
              (int)buf_size(parser->par_lexer.lex_tok_pos_ranges), "%d");
        const mkt_pos_range_t pos_range_start =
            parser->par_lexer.lex_tok_pos_ranges[parser->par_tok_i];
        CHECK(parser->par_tok_i, <, (int)buf_size(parser->par_lexer.lex_locs),
              "%d");
        const mkt_loc_t loc = parser->par_lexer.lex_locs[parser->par_tok_i];

        const char* const src_start =
            parser->par_lexer.lex_source + pos_range_start.pr_start;
        const char* const src_end =
            parser->par_lexer.lex_source + parser->par_lexer.lex_source_len;

        CHECK((void*)src_start, <, (void*)src_end, "%p");
        const int src_len = src_end - src_start;

        fprintf(stderr,
                "%s%s:%d:%sExpected top-level declaration, got something else: "
                "%.*s\n",
                mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
                loc.loc_line, mkt_colors[is_tty][COL_RESET], src_len,
                src_start);
        return RES_ERR;
    }

    if (parser->par_main_fn_i >= 0) return RES_OK;

    fprintf(stderr, "%s%s:%sMissing main function, no entrypoint found\n",
            mkt_colors[is_tty][COL_GRAY], parser->par_file_name0,
            mkt_colors[is_tty][COL_RESET]);
    return RES_ERR;
}
