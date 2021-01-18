#pragma once

#include "ast.h"
#include "parse.h"

// TODO: use platform headers for that?
#ifdef __APPLE__
#define MKT_NAME_PREFIX "_"
static const i32 syscall_exit = 0x2000001;
#else
#define MKT_NAME_PREFIX ""
static const i32 syscall_exit = 60;
#endif

#ifdef __APPLE__
#define MKT_SYSCALL_MMAP 0x20000c5
#define MKT_SYSCALL_MUNMAP 0x2000049
#define MKT_SYSCALL_WRITE 0x2000004
#define MKT_SYSCALL_KILL 0x2000025
#else
#define MKT_SYSCALL_MMAP 9
#define MKT_SYSCALL_MUNMAP 11
#define MKT_SYSCALL_WRITE 1
#define MKT_SYSCALL_KILL 62
#endif

static FILE* output_file = NULL;

static const char fn_args[6][5] = {
    [0] = "%rdi", [1] = "%rsi", [2] = "%rdx",
    [3] = "%rcx", [4] = "%r8",  [5] = "%r9",
};

static void emit_stmt(const parser_t* parser, i32 stmt_i);

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
static i32 emit_align_to_16(i32 stack_size) {
    CHECK(stack_size, >=, 0, "%d");

    return (stack_size + 16 - 1) / 16 * 16;
}

static void emit_addr(const parser_t* parser, i32 node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(node_i, >=, 0, "%d");
    CHECK(node_i, <, (i32)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const node = &parser->par_nodes[node_i];
    const mkt_type_t* const type = &parser->par_types[node->no_type_i];

    switch (node->no_kind) {
        case NODE_VAR: {
            const mkt_var_t var = node->no_n.no_var;

            if (type->ty_kind == TYPE_FN) {
                CHECK(var.va_var_node_i, >=, 0, "%d");
                const i32 fn_i = var.va_var_node_i;
                const mkt_node_t* const fn_node = &parser->par_nodes[fn_i];
                const mkt_fn_t fn = fn_node->no_n.no_fn;
                const char* name = NULL;
                i32 name_len = 0;
                parser_tok_source(parser, fn.fd_name_tok_i, &name, &name_len);
                CHECK((void*)name, !=, NULL, "%p");
                CHECK(name_len, >=, 0, "%d");
                CHECK(name_len, <, parser->par_lexer.lex_source_len, "%d");

                println("lea %.*s(%%rip), %%rax", name_len, name);
                return;
            }

            println("lea -%d(%%rbp), %%rax", var.va_offset);
            return;
        }
        case NODE_MEMBER: {
            const mkt_binary_t bin = node->no_n.no_binary;

            const mkt_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const mkt_type_t* const lhs_type =
                &parser->par_types[lhs->no_type_i];
            CHECK(lhs_type->ty_kind, ==, TYPE_PTR, "%d");
            emit_addr(parser, bin.bi_lhs_i);

            const mkt_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];
            CHECK(rhs->no_kind, ==, NODE_VAR, "%d");
            const mkt_var_t var = rhs->no_n.no_var;
            CHECK(var.va_var_node_i, ==, -1, "%d");

            const char* member_src = NULL;
            i32 member_src_len = 0;
            parser_tok_source(parser, var.va_tok_i, &member_src,
                              &member_src_len);
            println("add $%d, %%rax # get `%.*s`", var.va_offset,
                    member_src_len, member_src);
            return;
        }
        default:
            UNREACHABLE();
    }
}

static void emit_load(const mkt_type_t* type) {
    CHECK((void*)type, !=, NULL, "%p");

    switch (type->ty_kind) {
        case TYPE_FN:
        case TYPE_CLASS:
            // Do not try to load the value into a register since it likely
            // would not fit. So we 'decay' to the address of the array and
            // hence also of the  first element
            return;
        default:;
    }

    if (type->ty_size == 1)
        println("movsbl (%%rax), %%eax");
    else if (type->ty_size == 2)
        println("movswl (%%rax), %%eax");
    else if (type->ty_size == 4)
        println("mov (%%rax), %%eax");
    else
        println("mov (%%rax), %%rax");
}

// Pop the top of the stack and store it in rax
void emit_store(const mkt_type_t* type) {
    println("pop %%rdi");

    /* switch (type->ty_kind) { */
    /*     case TYPE_USER: */
    /*         //     for (i32 i = 0; i < type->ty_size; i++) { */
    /*         //         println("  mov %d(%%rax), %%r8b", i); */
    /*         //         println("  mov %%r8b, %d(%%rdi)", i); */
    /*         //     } */
    /*         return; */
    /*     default:; */
    /* } */

    if (type->ty_size == 1)
        println("mov %%al, (%%rdi)");
    else if (type->ty_size == 2)
        println("mov %%ax, (%%rdi)");
    else if (type->ty_size == 4)
        println("mov %%eax, (%%rdi)");
    else if (type->ty_size == 8)
        println("mov %%rax, (%%rdi)");
    else
        UNREACHABLE();
}

static void fn_prolog(const parser_t* parser, const mkt_fn_t* fn,
                      i32 aligned_stack_size) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)fn, !=, NULL, "%p");
    CHECK(aligned_stack_size, >=, 0, "%d");
    CHECK(aligned_stack_size % 16, ==, 0, "%d");

    println(".cfi_startproc");
    println("push %%rbp");
    println(".cfi_def_cfa_offset 16");
    println(".cfi_offset %%rbp, -16");
    println("mov %%rsp, %%rbp");
    println(".cfi_def_cfa_register %%rbp");

    println("sub $%d, %%rsp\n", aligned_stack_size);

    for (i32 i = 0; i < (i32)buf_size(fn->fd_arg_nodes_i); i++) {
        const i32 arg_i = fn->fd_arg_nodes_i[i];
        CHECK(arg_i, >=, 0, "%d");
        CHECK(arg_i, <, (i32)buf_size(parser->par_nodes), "%d");

        const mkt_node_t* const arg = &parser->par_nodes[arg_i];
        const i32 stack_offset = arg->no_n.no_var.va_offset;
        CHECK(stack_offset, >=, 0, "%d");

        CHECK(i, <, 6, "%d");  // FIXME: stack args

        println("movq %%rax, -%d(%%rbp)", stack_offset);

        println("mov %s, -%d(%%rbp)", fn_args[i], stack_offset);
    }
}

static void fn_epilog(i32 aligned_stack_size, i32 fn_i) {
    CHECK(aligned_stack_size, >=, 0, "%d");
    CHECK(aligned_stack_size % 16, ==, 0, "%d");

    println(".L.return.%d:", fn_i);
    println("addq $%d, %%rsp", aligned_stack_size);
    println("popq %%rbp");
    println(".cfi_endproc");
    println("ret\n");
}

static void emit_program_epilog() {
    println("\n# exit");
    println("mov $%d, %%rax", syscall_exit);
    println("mov $0, %%rdi");
    println("syscall");
}

static void emit_push() { println("push %%rax"); }

static void emit_loc(const parser_t* parser, i32 node_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(node_i, >=, 0, "%d");

    const mkt_loc_t loc =
        parser->par_lexer.lex_locs[node_first_token(parser, node_i)];
    println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
            parser->par_file_name0, loc.loc_line, loc.loc_column);
}

static void emit_expr(const parser_t* parser, const i32 expr_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    if (expr_i < 0) return;
    CHECK(expr_i, <, (i32)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const expr = &parser->par_nodes[expr_i];

    CHECK(expr->no_type_i, >=, 0, "%d");
    CHECK(expr->no_type_i, <, (i32)buf_size(parser->par_types), "%d");
    const mkt_type_t* const type = &parser->par_types[expr->no_type_i];

    const char *ax, *di /*, *dx */;
    if (type->ty_size == 8) {
        ax = "%rax";
        di = "%rdi";
        /* dx = "%rdx"; */
    } else {
        ax = "%eax";
        di = "%edi";
        /* dx = "%edx"; */
    }

    switch (expr->no_kind) {
        case NODE_KEYWORD_BOOL: {
            emit_loc(parser, expr_i);
            println("mov $%d, %s", (int8_t)expr->no_n.no_num.nu_val, ax);
            return;
        }
        case NODE_STRING: {
            const char* source = NULL;
            i32 source_len = 0;
            parser_tok_source(parser, expr->no_n.no_string.st_tok_i, &source,
                              &source_len);
            CHECK((void*)source, !=, NULL, "%p");
            CHECK(source_len, >=, 0, "%d");
            CHECK(source_len, <, parser->par_lexer.lex_source_len, "%d");

            emit_loc(parser, expr_i);
            println("mov $%d, %s # string len=%d", source_len, fn_args[0],
                    source_len);
            println("push %%rbx");
            println("push %%rcx");
            println("push %%rdx");
            println("push %%rsi");
            println("push %%r8");
            println("push %%r9");
            println("push %%r10");
            println("push %%r11");
            println("push %%r12");
            println("push %%r13");
            println("push %%r14");
            println("push %%r15");
            println("call " MKT_NAME_PREFIX "mkt_string_make");
            println("pop %%rbx");
            println("pop %%rcx");
            println("pop %%rdx");
            println("pop %%rsi");
            println("pop %%r8");
            println("pop %%r9");
            println("pop %%r10");
            println("pop %%r11");
            println("pop %%r12");
            println("pop %%r13");
            println("pop %%r14");
            println("pop %%r15");

            for (i32 i = 0; i < source_len; i++)
                println("movb $%d, %d(%%rax) # string[%d]", source[i], i, i);

            return;
        }
        case NODE_CHAR: {
            emit_loc(parser, expr_i);
            println("mov $%d, %s", (char)expr->no_n.no_num.nu_val, ax);
            return;
        }
        case NODE_NUM: {
            emit_loc(parser, expr_i);
            println("mov $%lld, %s", expr->no_n.no_num.nu_val, ax);
            return;
        }
        case NODE_MODULO: {
            const mkt_binary_t bin = expr->no_n.no_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr_i);
            println("pop %%rdi");
            println("cqo");  // ?
            println("xor %%rdx, %%rdx");
            println("idiv %%rdi");
            println("mov %%rdx, %%rax");

            return;
        }
        case NODE_DIVIDE: {
            const mkt_binary_t bin = expr->no_n.no_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr_i);
            println("pop %%rdi");
            println("cqo");  // ?
            println("idiv %%rdi");

            return;
        }
        case NODE_MULTIPLY: {
            const mkt_binary_t bin = expr->no_n.no_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr_i);
            println("pop %%rdi");
            println("imul %%rdi, %%rax");

            return;
        }
        case NODE_SUBTRACT: {
            const mkt_binary_t bin = expr->no_n.no_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr_i);
            println("pop %%rdi");
            println("sub %%rdi, %%rax");

            return;
        }
        case NODE_ADD: {
            const mkt_binary_t bin = expr->no_n.no_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr_i);

            const mkt_type_kind_t type_kind =
                parser->par_types[expr->no_type_i].ty_kind;
            if (type_kind == TYPE_STRING) {
                println("movq %%rax, %s", fn_args[0]);
                println("movq %%rax, %s", fn_args[1]);
                println("subq $8, %s", fn_args[1]);
                println("popq %s", fn_args[2]);
                println("movq %s, %s", fn_args[2], fn_args[3]);
                println("subq $8, %s", fn_args[3]);
                println("push %%rbx");
                println("push %%rbx");  // For 16 bytes alignment
                println("push %%r8");
                println("push %%r9");
                println("push %%r10");
                println("push %%r11");
                println("push %%r12");
                println("push %%r13");
                println("push %%r14");
                println("push %%r15");
                println("call " MKT_NAME_PREFIX "mkt_string_concat");
                println("pop %%rbx");
                println("pop %%rbx");  // For 16 bytes alignment
                println("pop %%r8");
                println("pop %%r9");
                println("pop %%r10");
                println("pop %%r11");
                println("pop %%r12");
                println("pop %%r13");
                println("pop %%r14");
                println("pop %%r15");
            } else {
                println("pop %%rdi");
                println("add %s, %s", di, ax);
            }

            return;
        }
        case NODE_MEMBER: {
            const mkt_binary_t bin = expr->no_n.no_binary;
            emit_expr(parser, bin.bi_lhs_i);
            emit_load(type);

            return;
        }
        case NODE_ASSIGN:
            UNREACHABLE();
        case NODE_LT:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_LE: {
            const mkt_binary_t bin = expr->no_n.no_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            emit_loc(parser, expr_i);
            println("pop %%rdi");
            println("cmp %%rdi, %%rax");

            if (expr->no_kind == NODE_LT)
                println("setl %%al");
            else if (expr->no_kind == NODE_LE)
                println("setle %%al");
            else if (expr->no_kind == NODE_EQ)
                println("sete %%al");
            else if (expr->no_kind == NODE_NEQ)
                println("setne %%al");
            else
                UNREACHABLE();

            // Required to be able to do later `push %rax`
            println("movzb %%al, %%rax");

            return;
        }
        case NODE_RETURN: {
            const mkt_return_t ret = expr->no_n.no_return;
            emit_expr(parser, ret.re_node_i);
            emit_loc(parser, expr_i);
            println("jmp .L.return.%d", ret.re_fn_i);
            return;
        }
        case NODE_NOT: {
            emit_expr(parser, expr->no_n.no_unary.un_node_i);
            emit_loc(parser, expr_i);
            println("cmp $0, %%rax");
            println("sete %%al");
            return;
        }
        case NODE_IF: {
            const mkt_if_t node = expr->no_n.no_if;

            emit_expr(parser, node.if_node_cond_i);
            emit_loc(parser, expr_i);
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
            const mkt_builtin_println_t builtin_println =
                expr->no_n.no_builtin_println;
            emit_expr(parser, builtin_println.bp_arg_i);

            const mkt_node_t* arg =
                &parser->par_nodes[builtin_println.bp_arg_i];
            const mkt_type_kind_t type =
                parser->par_types[arg->no_type_i].ty_kind;

            emit_loc(parser, expr_i);
            println("mov %%rax, %s", fn_args[0]);

            if (type == TYPE_LONG || type == TYPE_INT || type == TYPE_SHORT ||
                type == TYPE_BYTE)
                println("call " MKT_NAME_PREFIX "mkt_int_println");
            else if (type == TYPE_CHAR)
                println("call " MKT_NAME_PREFIX "mkt_char_println");
            else if (type == TYPE_BOOL)
                println("call " MKT_NAME_PREFIX "mkt_bool_println");
            else if (type == TYPE_STRING) {
                println("mov %%rax, %s", fn_args[1]);
                println("sub $8, %s", fn_args[1]);
                println("call " MKT_NAME_PREFIX "mkt_string_println");
            } else if (type == TYPE_PTR) {
                println("call " MKT_NAME_PREFIX "mkt_instance_println");
            } else {
                log_debug("Type %s unimplemented", mkt_type_to_str[type]);
                UNIMPLEMENTED();
            }
            println("movq $0, %%rax");  // Return value 0

            return;
        }
        case NODE_SYSCALL: {
            const mkt_syscall_t syscall = expr->no_n.no_syscall;
            const i32 len = (i32)buf_size(syscall.sy_arg_nodes_i);
            CHECK(len, >, 0, "%d");

            for (i32 i = len - 1; i > 0; i--) {
                emit_expr(parser, syscall.sy_arg_nodes_i[i]);
                println("push %%rax");
            }
            for (i32 i = 1; i < len; i++) println("pop %s", fn_args[i - 1]);

            emit_expr(parser, syscall.sy_arg_nodes_i[0]);
            println("syscall");

            return;
        }
        case NODE_BLOCK: {
            const mkt_block_t block = expr->no_n.no_block;

            for (i32 i = 0; i < (i32)buf_size(block.bl_nodes_i); i++) {
                const i32 stmt_node_i = block.bl_nodes_i[i];
                emit_stmt(parser, stmt_node_i);
            }

            return;
        }
        case NODE_VAR: {
            emit_loc(parser, expr_i);
            emit_addr(parser, expr_i);
            emit_load(type);
            return;
        }
        case NODE_CALL: {
            const mkt_call_t call = expr->no_n.no_call;
            CHECK(buf_size(call.ca_arg_nodes_i), <=, 6UL, "%zu");  // TODO

            for (i32 i = 0; i < (i32)buf_size(call.ca_arg_nodes_i); i++) {
                emit_expr(parser, call.ca_arg_nodes_i[i]);
                emit_loc(parser, expr_i);
                println("mov %%rax, %s", fn_args[i]);

                // TODO: preserver registers by spilling
                // TODO: use the stack if # args > 6
            }
            emit_expr(parser, call.ca_lhs_node_i);
            println("mov %%rax, %%r10");
            println("call *%%r10");

            return;
        }
        case NODE_INSTANCE: {
            CHECK(type->ty_kind, ==, TYPE_PTR, "%d");
            CHECK(type->ty_ptr_type_i, >=, 0, "%d");
            CHECK(type->ty_ptr_type_i, <, (i32)buf_size(parser->par_types),
                  "%d");

            const mkt_type_t* const instance_type =
                &parser->par_types[type->ty_ptr_type_i];
            println("mov $%d, %s", instance_type->ty_size, fn_args[0]);
            println("push %%rbx");
            println("push %%rbx");  // For alignment
            println("push %%rcx");
            println("push %%rdi");
            println("push %%rdx");
            println("push %%rsi");
            println("push %%r8");
            println("push %%r9");
            println("push %%r10");
            println("push %%r11");
            println("push %%r12");
            println("push %%r13");
            println("push %%r14");
            println("push %%r15");
            println("call " MKT_NAME_PREFIX "mkt_instance_make");
            println("pop %%rbx");
            println("pop %%rbx");  // For alignment
            println("pop %%rcx");
            println("pop %%rdi");
            println("pop %%rdx");
            println("pop %%rsi");
            println("pop %%r8");
            println("pop %%r9");
            println("pop %%r10");
            println("pop %%r11");
            println("pop %%r12");
            println("pop %%r13");
            println("pop %%r14");
            println("pop %%r15");

            // Push the pointer to the instance on the stack to avoid losing it
            // when handling the members
            // println("push %%rax");

            // mkt_instance_t instance = expr->no_n.no_instance;
            // const mkt_node_t* const class_node =
            //    &parser->par_nodes[instance.in_class];
            // mkt_class_t class = class_node->no_n.no_class;
            // for (i32 i = 0; i < (i32)buf_size(class.cl_members); i++) {
            //    const i32 m_i = class.cl_members[i];
            //    /* const mkt_type_t* const type = &parser->par_types[m_i]; */
            //    // FIXME
            //    emit_stmt(parser, m_i);
            //}

            return;
        }
        case NODE_FN:
            return;  // Already generated in the .data section

            // Forbidden by the grammar or simply impossible
        default:
            log_debug("no_kind=%s", mkt_node_kind_to_str[expr->no_kind]);
            UNREACHABLE();
    }
}

static void emit_stmt(const parser_t* parser, i32 stmt_i) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK(stmt_i, >=, 0, "%d");
    CHECK(stmt_i, <, (i32)buf_size(parser->par_nodes), "%d");

    const mkt_node_t* const stmt = &parser->par_nodes[stmt_i];
    const mkt_type_t* const type = &parser->par_types[stmt->no_type_i];
    const mkt_loc_t loc =
        parser->par_lexer.lex_locs[node_first_token(parser, stmt_i)];
    println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
            parser->par_file_name0, loc.loc_line, loc.loc_column);

    switch (stmt->no_kind) {
        case NODE_BUILTIN_PRINTLN:
        case NODE_SYSCALL:
        case NODE_BLOCK:
        case NODE_NUM:
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
        case NODE_INSTANCE:
        case NODE_MEMBER:
        case NODE_IF: {
            emit_expr(parser, stmt_i);
            return;
        }
        case NODE_ASSIGN: {
            const mkt_binary_t binary = stmt->no_n.no_binary;

            emit_addr(parser, binary.bi_lhs_i);
            println("push %%rax");
            emit_expr(parser, binary.bi_rhs_i);
            emit_store(type);

            return;
        }
        case NODE_VAR: {
            emit_expr(parser, stmt_i);
            return;
        }
        case NODE_WHILE: {
            const mkt_while_t w = stmt->no_n.no_while;
            emit_loc(parser, stmt_i);
            println(".Lwhile_loop_start%d:", stmt_i);
            emit_expr(parser, w.wh_cond_i);

            println("cmp $0, %%rax");
            println("je .Lwhile_loop_end%d", stmt_i);

            emit_stmt(parser, w.wh_body_i);
            println("jmp .Lwhile_loop_start%d", stmt_i);

            println(".Lwhile_loop_end%d:", stmt_i);
            return;
        }
            // No-op: Already generated in the `.data` section
        case NODE_FN:

            // No-op: Generated for NODE_INSTANCE, does not
            // exist at runtime
        case NODE_CLASS:
            return;
        case NODE_CALL: {
            emit_expr(parser, stmt_i);
            return;
        }
        default:
            log_debug("no_kind=%s", mkt_node_kind_to_str[stmt->no_kind]);
            UNREACHABLE();
    }
}

static void emit(const parser_t* parser, FILE* asm_file) {
    CHECK((void*)parser, !=, NULL, "%p");
    CHECK((void*)parser->par_nodes, !=, NULL, "%p");
    CHECK((void*)asm_file, !=, NULL, "%p");

    output_file = asm_file;

    println(".file 1 \"%s\"", parser->par_file_name0);

    println(".globl " MKT_NAME_PREFIX "mkt_mmap\n" MKT_NAME_PREFIX
            "mkt_mmap:\n"
            ".cfi_startproc\n"
            "push %%rbp\n"
            "mov $%d, %%eax\n"
            "mov %%ecx, %%r10d\n"  // r10 is used for the fourth parameter
                                   // instead of rcx for syscalls
            "syscall\n"
            "pop %%rbp\n"
            ".cfi_endproc\n"
            "ret\n",
            MKT_SYSCALL_MMAP);

    println(".globl " MKT_NAME_PREFIX "mkt_munmap\n" MKT_NAME_PREFIX
            "mkt_munmap:\n"
            ".cfi_startproc\n"
            "push %%rbp\n"
            "mov $%d, %%eax\n"
            "syscall\n"
            "pop %%rbp\n"
            ".cfi_endproc\n"
            "ret\n",
            MKT_SYSCALL_MUNMAP);

    println(".globl " MKT_NAME_PREFIX "mkt_write\n" MKT_NAME_PREFIX
            "mkt_write:\n"
            ".cfi_startproc\n"
            "push %%rbp\n"
            "mov $%d, %%eax\n"
            "syscall\n"
            "pop %%rbp\n"
            ".cfi_endproc\n"
            "ret\n",
            MKT_SYSCALL_WRITE);

    println(".globl " MKT_NAME_PREFIX "mkt_kill\n" MKT_NAME_PREFIX
            "mkt_kill:\n"
            ".cfi_startproc\n"
            "push %%rbp\n"
            "mov $%d, %%eax\n"
            "syscall\n"
            "pop %%rbp\n"
            ".cfi_endproc\n"
            "ret\n",
            MKT_SYSCALL_KILL);

    println(".globl " MKT_NAME_PREFIX "start");
    println(MKT_NAME_PREFIX "start:");
    println(".cfi_startproc");
    println(
        "push %%rbp\n"
        ".cfi_def_cfa_offset 16\n"
        ".cfi_offset %%rbp, -16\n"
        "mov %%rsp, %%rbp\n"
        ".cfi_def_cfa_register %%rbp\n");

    println("call " MKT_NAME_PREFIX "mkt_init");
    println("call main");
    println("movq $0, %%rax");  // Return value 0
    println("popq %%rbp");
    println(".cfi_endproc");
    println("ret\n");

    for (i32 c = 0; c < (i32)buf_size(parser->par_class_decls); c++) {
        const i32 node_class_i = parser->par_class_decls[c];
        CHECK(node_class_i, >=, 0, "%d");
        CHECK(node_class_i, <, (i32)buf_size(parser->par_nodes), "%d");

        const mkt_node_t* const node_class = &parser->par_nodes[node_class_i];
        CHECK(node_class->no_kind, ==, NODE_CLASS, "%d");
        const mkt_class_t* const class = &node_class->no_n.no_class;

        for (i32 f = 0; f < (i32)buf_size(class->cl_methods); f++) {
            const i32 node_fn_i = class->cl_methods[f];
            CHECK(node_fn_i, >=, 0, "%d");
            CHECK(node_fn_i, <, (i32)buf_size(parser->par_nodes), "%d");

            const mkt_fn_t fn = parser->par_nodes[node_fn_i].no_n.no_fn;
            CHECK(fn.fd_name_tok_i, >=, 0, "%d");
            CHECK(fn.fd_name_tok_i, <, parser->par_lexer.lex_source_len, "%d");

            const mkt_loc_t loc = parser->par_lexer.lex_locs[fn.fd_name_tok_i];
            println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
                    parser->par_file_name0, loc.loc_line, loc.loc_column);

            const mkt_pos_range_t pos_range =
                parser->par_lexer.lex_tok_pos_ranges[fn.fd_name_tok_i];
            const char* const name =
                &parser->par_lexer.lex_source[pos_range.pr_start];
            const i32 name_len = pos_range.pr_end - pos_range.pr_start;
            CHECK((void*)name, !=, NULL, "%p");
            CHECK(name_len, >=, 0, "%d");
            CHECK(name_len, <, parser->par_lexer.lex_source_len, "%d");

            if (fn.fd_flags & FN_FLAGS_PUBLIC)
                println(".global %.*s", name_len, name);

            println("%.*s:", name_len, name);

            const i32 aligned_stack_size = emit_align_to_16(fn.fd_stack_size);
            log_debug("%.*s: stack_size=%d aligned_stack_size=%d", name_len,
                      name, fn.fd_stack_size, aligned_stack_size);
            fn_prolog(parser, &fn, aligned_stack_size);
            emit_stmt(parser, fn.fd_body_node_i);

            if (node_fn_i == parser->par_main_fn_i) emit_program_epilog();

            fn_epilog(aligned_stack_size, node_fn_i);
        }
    }
}
