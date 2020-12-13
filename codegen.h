#pragma once

#include "ast.h"
#include "common.h"
#include "parse.h"

// TODO: use platform headers for that?
#ifdef __APPLE__
static const char name_prefix[] = "_";
static const long long int syscall_exit = 0x2000001;
#else
static const char name_prefix[] = "";
static const long long int syscall_exit = 60;
#endif

static FILE* output_file = NULL;

static int current_fn_i = 0;

static const char fn_args[6][5] = {
    [0] = "%rdi", [1] = "%rsi", [2] = "%rdx",
    [3] = "%rcx", [4] = "%r8",  [5] = "%r9",
};

static void emit_stmt(const parser_t* parser, int stmt_i);

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
__attribute__((format(printf, 1, 2)))
#endif
static void
println(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    fprintf(output_file, "\n");
}

// The System V ABI requires a 16-bit aligned stack.
// Round up to a multiple of 16
static int emit_align_to_16(int stack_size) {
    CHECK(stack_size, >=, 0, "%d");

    return (stack_size + 16 - 1) / 16 * 16;
}

static void fn_prolog(const parser_t* parser, const fn_decl_t* fn_decl,
                      int aligned_stack_size) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)fn_decl, !=, NULL, "%p");
    CHECK(aligned_stack_size, >=, 0, "%d");
    CHECK(aligned_stack_size % 16, ==, 0, "%d");

    println("# prolog");
    println(".cfi_startproc");
    println("push %%rbp");
    println(".cfi_def_cfa_offset 16");
    println(".cfi_offset %%rbp, -16");
    println("mov %%rsp, %%rbp");

    println("sub $%d, %%rsp\n", aligned_stack_size);
    for (int i = 8; i <= aligned_stack_size; i += 8)
        println("movq $0, -%d(%%rbp)", i);

    for (int i = 0; i < (int)buf_size(fn_decl->fd_arg_nodes_i); i++) {
        const int arg_i = fn_decl->fd_arg_nodes_i[i];
        CHECK(arg_i, >=, 0, "%d");
        CHECK(arg_i, <, (int)buf_size(parser->par_nodes), "%d");

        const node_t* const arg = &parser->par_nodes[arg_i];
        const runtime_val_header header =
            parser->par_types[arg->node_type_i].ty_header;
        const int stack_offset = arg->node_n.node_var_def.vd_stack_offset -
                                 sizeof(runtime_val_header);
        CHECK(stack_offset, >=, 0, "%d");

        CHECK(i, <, 6, "%d");  // FIXME: stack args

        size_t* header_val = (size_t*)&header;
        println("movq $%zu, -%d(%%rbp) # tag: size=%llu color=%u tag=%u ",
                *header_val, stack_offset + (int)sizeof(runtime_val_header),
                header.rv_size, header.rv_color, header.rv_tag);

        println("mov %s, -%d(%%rbp)", fn_args[i], stack_offset);
    }
}

static void fn_epilog(int aligned_stack_size) {
    CHECK(aligned_stack_size, >=, 0, "%d");
    CHECK(aligned_stack_size % 16, ==, 0, "%d");

    println(".L.return.%d:", current_fn_i);
    println("addq $%d, %%rsp", aligned_stack_size);
    println("popq %%rbp");
    println(".cfi_endproc");
    println("ret\n");
}

static void emit_program_epilog() {
    println("\n# exit");
    println("mov $%lld, %%rax", syscall_exit);
    println("mov $0, %%rdi");
    println("syscall");
}

static void emit_push() { println("push %%rax"); }

static void emit_loc(const parser_t* parser, const node_t* const node) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)node, !=, NULL, "%p");

    const loc_t loc =
        parser->par_lexer.lex_locs[node_first_token(parser, node)];
    println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
            parser->par_file_name0, loc.loc_line, loc.loc_column);
}

static void emit_expr(const parser_t* parser, const int expr_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(expr_i, >=, 0, "%d");
    CHECK(expr_i, <, (int)buf_size(parser->par_nodes), "%d");

    const node_t* const expr = &parser->par_nodes[expr_i];

    CHECK(expr->node_type_i, >=, 0, "%d");
    CHECK(expr->node_type_i, <, (int)buf_size(parser->par_types), "%d");
    const type_t* const type = &parser->par_types[expr->node_type_i];

    const char *ax, *di, *dx;
    if (type->ty_header.rv_size == 8) {
        ax = "%rax";
        di = "%rdi";
        dx = "%rdx";
    } else {
        ax = "%eax";
        di = "%edi";
        dx = "%edx";
    }

    switch (expr->node_kind) {
        case NODE_KEYWORD_BOOL: {
            emit_loc(parser, expr);
            println("mov $%d, %s", (int8_t)expr->node_n.node_num.nu_val, ax);
            return;
        }
        case NODE_STRING: {
            const char* source = NULL;
            int source_len = 0;
            parser_tok_source(parser, expr->node_n.node_string.st_tok_i,
                              &source, &source_len);
            CHECK((void*)source, !=, NULL, "%p");
            CHECK(source_len, >=, 0, "%d");
            CHECK(source_len, <, parser->par_lexer.lex_source_len, "%d");

            // HACK: should be set by the parser
            parser->par_types[expr_i].ty_header.rv_size = source_len;

            emit_loc(parser, expr);
            println("mov $%d, %s # string len=%d", source_len, fn_args[0],
                    source_len);
            println("mov %%rsp, %s", fn_args[1]);
            println("mov %%rbp, %s", fn_args[2]);
            println("call %smkt_alloc", name_prefix);

            for (int i = 0; i < source_len; i++)
                println("movb $%d, +%d(%%rax) # string[%d]=`%c`", source[i], i,
                        i, source[i]);

            return;
        }
        case NODE_CHAR: {
            emit_loc(parser, expr);
            println("mov $%d, %s", (char)expr->node_n.node_num.nu_val, ax);
            return;
        }
        case NODE_LONG: {
            emit_loc(parser, expr);
            println("mov $%lld, %s", expr->node_n.node_num.nu_val, ax);
            return;
        }
        case NODE_MODULO: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr);
            println("pop %%rdi");
            println("cqo");  // ?
            println("xor %%rdx, %%rdx");
            println("idiv %%rdi");
            println("mov %%rdx, %%rax");

            return;
        }
        case NODE_DIVIDE: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr);
            println("pop %%rdi");
            println("cqo");  // ?
            println("idiv %%rdi");

            return;
        }
        case NODE_MULTIPLY: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr);
            println("pop %%rdi");
            println("imul %%rdi, %%rax");

            return;
        }
        case NODE_SUBTRACT: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr);
            println("pop %%rdi");
            println("sub %%rdi, %%rax");

            return;
        }
        case NODE_ADD: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr);

            const type_kind_t type_kind =
                parser->par_types[expr->node_type_i].ty_kind;
            if (type_kind == TYPE_STRING) {
                println("mov %%rax, %%rdi");
                println("pop %%rsi");
                println("mov %%rsp, %s", fn_args[2]);
                println("mov %%rbp, %s", fn_args[3]);
                println("call %smkt_string_concat", name_prefix);
            } else {
                println("pop %%rdi");
                println("add %s, %s", di, ax);
            }

            return;
        }
        case NODE_ASSIGN:
            UNIMPLEMENTED();
        case NODE_LT:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_LE: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr);
            println("pop %%rdi");
            println("cmp %%rdi, %%rax");

            if (expr->node_kind == NODE_LT)
                println("setl %%al");
            else if (expr->node_kind == NODE_LE)
                println("setle %%al");
            else if (expr->node_kind == NODE_EQ)
                println("sete %%al");
            else if (expr->node_kind == NODE_NEQ)
                println("setne %%al");
            else
                UNREACHABLE();

            // Required to be able to do later `push %rax
            println("movzb %%al, %%rax");

            return;
        }
        case NODE_RETURN: {
            if (expr->node_n.node_unary.un_node_i >= 0)
                emit_expr(parser, expr->node_n.node_unary.un_node_i);

            emit_loc(parser, expr);
            println("jmp .L.return.%d", current_fn_i);
            return;
        }
        case NODE_NOT: {
            emit_expr(parser, expr->node_n.node_unary.un_node_i);
            emit_loc(parser, expr);
            println("cmp $0, %%rax");
            println("sete %%al");
            return;
        }
        case NODE_IF: {
            const if_t node = expr->node_n.node_if;

            emit_expr(parser, node.if_node_cond_i);
            emit_loc(parser, expr);
            println("cmp $0, %%rax");
            println("je .L.else.%d", expr_i);

            emit_expr(parser, node.if_node_then_i);

            println("jmp .L.end.%d", expr_i);

            println(".L.else.%d:", expr_i);
            if (node.if_node_else_i >= 0)
                emit_expr(parser, node.if_node_else_i);

            println(".L.end.%d:", expr_i);

            return;
        }
        case NODE_BUILTIN_PRINTLN: {
            const builtin_println_t builtin_println =
                expr->node_n.node_builtin_println;
            emit_expr(parser, builtin_println.bp_arg_i);

            const node_t* arg = &parser->par_nodes[builtin_println.bp_arg_i];
            const type_kind_t type =
                parser->par_types[arg->node_type_i].ty_kind;

            emit_loc(parser, expr);
            println("mov %%rax, %%rdi");

            if (type == TYPE_LONG || type == TYPE_INT || type == TYPE_SHORT ||
                type == TYPE_BYTE)
                println("call %smkt_println_int", name_prefix);
            else if (type == TYPE_CHAR)
                println("call %smkt_println_char", name_prefix);
            else if (type == TYPE_BOOL)
                println("call %smkt_println_bool", name_prefix);
            else if (type == TYPE_STRING) {
                println("mov %%rax, %%rdi");
                println("call %smkt_println_string", name_prefix);
            } else {
                log_debug("Type %s unimplemented", type_to_str[type]);
                UNIMPLEMENTED();
            }

            return;
        }
        case NODE_BLOCK: {
            const block_t block = expr->node_n.node_block;

            for (int i = 0; i < (int)buf_size(block.bl_nodes_i); i++) {
                const int stmt_node_i = block.bl_nodes_i[i];
                emit_stmt(parser, stmt_node_i);
            }

            return;
        }
        case NODE_VAR: {
            const var_t var = expr->node_n.node_var;
            const node_t* const node_def =
                &parser->par_nodes[var.va_var_node_i];

            CHECK(node_def->node_type_i, >=, 0, "%d");
            CHECK(node_def->node_type_i, <, (int)buf_size(parser->par_types),
                  "%d");
            const int type_size =
                parser->par_types[node_def->node_type_i].ty_header.rv_size;
            CHECK(type_size, >=, 0, "%d");

            emit_loc(parser, expr);
            if (node_def->node_kind == NODE_VAR_DEF) {
                const var_def_t var_def = node_def->node_n.node_var_def;
                const int offset =
                    var_def.vd_stack_offset - sizeof(runtime_val_header);

                if (type_size == 1)
                    println("mov -%d(%%rbp), %%al", offset);
                else if (type_size == 2)
                    println("mov -%d(%%rbp), %%ax", offset);
                else if (type_size == 4)
                    println("mov -%d(%%rbp), %%eax", offset);
                else
                    println("mov -%d(%%rbp), %%rax", offset);
            } else if (node_def->node_kind == NODE_FN_DECL) {
                CHECK(current_fn_i, >=, 0, "%d");
                CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
                const int caller_current_fn_i = current_fn_i;

                CHECK(current_fn_i, >=, 0, "%d");
                CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
                current_fn_i = var.va_var_node_i;
                CHECK(current_fn_i, >=, 0, "%d");
                CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");

                const fn_decl_t fn_decl = node_def->node_n.node_fn_decl;
                const char* name = NULL;
                int name_len = 0;
                parser_tok_source(parser, fn_decl.fd_name_tok_i, &name,
                                  &name_len);
                CHECK((void*)name, !=, NULL, "%p");
                CHECK(name_len, >=, 0, "%d");
                CHECK(name_len, <, parser->par_lexer.lex_source_len, "%d");

                // TODO: args, etc

                println("call %.*s", name_len, name);

                CHECK(current_fn_i, >=, 0, "%d");
                CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
                current_fn_i = caller_current_fn_i;
                CHECK(current_fn_i, >=, 0, "%d");
                CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
            } else
                UNREACHABLE();

            return;
        }
        case NODE_CALL: {
            const call_t call = expr->node_n.node_call;
            for (int i = 0; i < (int)buf_size(call.ca_arg_nodes_i); i++) {
                emit_expr(parser, call.ca_arg_nodes_i[i]);
                emit_loc(parser, expr);
                println("mov %%rax, %s", fn_args[i]);

                // TODO: preserver registers by spilling
                // TODO: use the stack if # args > 6
            }

            emit_expr(parser, call.ca_lhs_node_i);
            return;
        }

            // Forbidden by the grammer
        case NODE_WHILE:
        case NODE_VAR_DEF:
        case NODE_FN_DECL:
            UNREACHABLE();
    }
    log_debug("node_kind=%s", node_kind_to_str[expr->node_kind]);
    UNREACHABLE();
}

static void emit_stmt(const parser_t* parser, int stmt_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(stmt_i, >=, 0, "%d");
    CHECK(stmt_i, <, (int)buf_size(parser->par_nodes), "%d");

    const node_t* const stmt = &parser->par_nodes[stmt_i];
    const loc_t loc =
        parser->par_lexer.lex_locs[node_first_token(parser, stmt)];
    println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
            parser->par_file_name0, loc.loc_line, loc.loc_column);

    switch (stmt->node_kind) {
        case NODE_BUILTIN_PRINTLN:
        case NODE_BLOCK:
        case NODE_LONG:
        case NODE_CHAR:
        case NODE_STRING:
        case NODE_KEYWORD_BOOL:
        case NODE_LT:
        case NODE_LE:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_MULTIPLY:
        case NODE_MODULO:
        case NODE_DIVIDE:
        case NODE_SUBTRACT:
        case NODE_ADD:
        case NODE_RETURN:
        case NODE_NOT:
        case NODE_IF: {
            emit_expr(parser, stmt_i);
            return;
        }
        case NODE_ASSIGN: {
            const binary_t binary = stmt->node_n.node_binary;
            const node_t* const lhs = &parser->par_nodes[binary.bi_lhs_i];

            emit_expr(parser, binary.bi_rhs_i);

            const var_t var = lhs->node_n.node_var;

            CHECK(var.va_var_node_i, >=, 0, "%d");
            CHECK(var.va_var_node_i, <, (int)buf_size(parser->par_nodes), "%d");
            const node_t* const node_def =
                &parser->par_nodes[var.va_var_node_i];

            const var_def_t var_def = node_def->node_n.node_var_def;
            const int offset =
                var_def.vd_stack_offset - sizeof(runtime_val_header);
            CHECK(offset, >=, 0, "%d");

            CHECK(stmt->node_type_i, >=, 0, "%d");
            CHECK(stmt->node_type_i, <, (int)buf_size(parser->par_types), "%d");
            const int type_size =
                parser->par_types[stmt->node_type_i].ty_header.rv_size;
            CHECK(type_size, >=, 0, "%d");

            emit_loc(parser, stmt);
            if (type_size == 1)
                println("mov %%al, -%d(%%rbp)", offset);
            else if (type_size == 2)
                println("mov %%ax, -%d(%%rbp)", offset);
            else if (type_size == 4)
                println("mov %%eax, -%d(%%rbp)", offset);
            else
                println("mov %%rax, -%d(%%rbp)", offset);

            return;
        }
        case NODE_VAR_DEF: {
            const var_def_t var_def = stmt->node_n.node_var_def;
            if (var_def.vd_init_node_i < 0) return;

            emit_expr(parser, var_def.vd_init_node_i);

            const runtime_val_header header =
                parser->par_types[stmt->node_type_i].ty_header;
            const int type_size = header.rv_size;
            CHECK(type_size, >=, 0, "%d");

            const int offset =
                var_def.vd_stack_offset - (int)sizeof(runtime_val_header);
            CHECK(offset, >=, 0, "%d");

            emit_loc(parser, stmt);

            size_t* header_val = (size_t*)&header;

            println("movl $%zu, -%d(%%rbp) # tag: size=%llu color=%u tag=%u",
                    *header_val, offset + (int)sizeof(runtime_val_header),
                    header.rv_size, header.rv_color, header.rv_tag);

            if (type_size == 1)
                println("mov %%al, -%d(%%rbp)", offset);
            else if (type_size == 2)
                println("mov %%ax, -%d(%%rbp)", offset);
            else if (type_size == 4)
                println("mov %%eax, -%d(%%rbp)", offset);
            else
                println("mov %%rax, -%d(%%rbp)", offset);

            return;
        }
        case NODE_VAR: {
            emit_expr(parser, stmt_i);
            return;
        }
        case NODE_WHILE: {
            const while_t w = stmt->node_n.node_while;
            emit_loc(parser, stmt);
            println(".Lwhile_loop_start%d:", stmt_i);
            emit_expr(parser, w.wh_cond_i);

            println("cmp $0, %%rax");
            println("je .Lwhile_loop_end%d", stmt_i);

            emit_stmt(parser, w.wh_body_i);
            println("jmp .Lwhile_loop_start%d", stmt_i);

            println(".Lwhile_loop_end%d:", stmt_i);
            return;
        }
        case NODE_FN_DECL:
            return;
        case NODE_CALL: {
            emit_expr(parser, stmt_i);
            return;
        }
    }
    log_debug("node_kind=%s", node_kind_to_str[stmt->node_kind]);
    UNREACHABLE();
}

static void emit(const parser_t* parser, FILE* asm_file) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)parser->par_nodes, !=, NULL, "%p");
    CHECK((void*)asm_file, !=, NULL, "%p");

    output_file = asm_file;
    println(".data");

    println("\n.text");

    println(".file 1 \"%s\"", parser->par_file_name0);

    println(".globl _start");
    println("_start:");
    println(".cfi_startproc");
    println(
        "push %%rbp\n"
        ".cfi_def_cfa_offset 16\n"
        ".cfi_offset %%rbp, -16\n"
        "mov %%rsp, %%rbp\n"
        "sub $0, %%rsp");

    println("call %smkt_init", name_prefix);
    println("call %smain", name_prefix);
    println(
        "addq $16, %%rsp\n"
        "popq %%rbp");
    println(".cfi_endproc");
    println("ret");

    for (int i = 0; i < (int)buf_size(parser->par_node_decls); i++) {
        const int node_i = parser->par_node_decls[i];
        CHECK(node_i, >=, 0, "%d");
        CHECK(node_i, <, (int)buf_size(parser->par_nodes), "%d");

        const node_t* const node = &parser->par_nodes[node_i];
        if (node->node_kind != NODE_FN_DECL) continue;

        const fn_decl_t fn_decl = node->node_n.node_fn_decl;
        CHECK(fn_decl.fd_name_tok_i, >=, 0, "%d");
        CHECK(fn_decl.fd_name_tok_i, <, parser->par_lexer.lex_source_len, "%d");

        const loc_t loc = parser->par_lexer.lex_locs[fn_decl.fd_name_tok_i];
        println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
                parser->par_file_name0, loc.loc_line, loc.loc_column);

        const pos_range_t pos_range =
            parser->par_lexer.lex_tok_pos_ranges[fn_decl.fd_name_tok_i];
        const char* const name =
            &parser->par_lexer.lex_source[pos_range.pr_start];
        const int name_len = pos_range.pr_end - pos_range.pr_start;
        CHECK((void*)name, !=, NULL, "%p");
        CHECK(name_len, >=, 0, "%d");
        CHECK(name_len, <, parser->par_lexer.lex_source_len, "%d");

        if (fn_decl.fd_flags & FN_FLAGS_PUBLIC)
            println(".global %.*s",
                    name_len == 0 ? (int)sizeof("_main") : name_len,
                    name_len == 0 ? "_main" : name);

        println("%.*s:", name_len == 0 ? (int)sizeof("_main") : name_len,
                name_len == 0 ? "_main" : name);

        const int caller_current_fn_i = current_fn_i;
        CHECK(current_fn_i, >=, 0, "%d");
        CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
        current_fn_i = node_i;

        const int aligned_stack_size = emit_align_to_16(fn_decl.fd_stack_size);
        log_debug("%.*s: stack_size=%d aligned_stack_size=%d", name_len, name,
                  fn_decl.fd_stack_size, aligned_stack_size);
        fn_prolog(parser, &fn_decl, aligned_stack_size);
        emit_stmt(parser, fn_decl.fd_body_node_i);
        if (i == 0) emit_program_epilog();

        fn_epilog(aligned_stack_size);

        current_fn_i = caller_current_fn_i;
        CHECK(current_fn_i, >=, 0, "%d");
        CHECK(current_fn_i, <, (int)buf_size(parser->par_nodes), "%d");
    }
}
