#pragma once

#include "parse.h"

// TODO: use platform headers for that?
#ifdef __APPLE__
static const long long int syscall_write = 0x2000004;
static const long long int syscall_exit = 0x2000001;
#else
static const long long int syscall_write = 1;
static const long long int syscall_exit = 60;
#endif

static FILE* output_file = NULL;

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
    return (stack_size + 16 - 1) / 16 * 16;
}

static void fn_prolog(int aligned_stack_size) {
    println("# prolog");
    println("push %%rbp");
    println("mov %%rsp, %%rbp");

    println("sub $%d, %%rsp\n", aligned_stack_size);
}

// static void fn_epilog(int aligned_stack_size) {
//    println("addq $%d, %%rsp", aligned_stack_size);
//    println("popq %%rbp");
//    println("ret\n");
//}

static void emit_program_epilog() {
    println("\n# exit");
    println("mov $%lld, %%rax", syscall_exit);
    println("mov $0, %%rdi");
    println("syscall");
}

static void emit_push() { println("push %%rax"); }

static void emit_stdlib() {
    println(
        "__println_bool:\n"
        "    test %%rdi, %%rdi\n"
        "    leaq .Lfalse(%%rip), %%rax\n"
        "    leaq .Ltrue(%%rip), %%rsi\n"
        "    # len +1 to include the newline\n"
        "    movq $6, %%rdi\n"
        "    movq $5, %%rdx\n"
        "    cmoveq %%rax, %%rsi\n\n"
        "    cmoveq %%rdi, %%rdx\n\n"

        "    movq $%lld, %%rax\n"
        "    movq $1, %%rdi\n\n"

        "    syscall\n"
        "    xorq %%rax, %%rax\n"
        "    ret\n\n"

        "__println_string:\n"
        "    movq $%lld, %%rax\n"
        "    movq %%rsi, %%rdx\n"
        "    movq %%rdi, %%rsi\n"
        "    movq $1, %%rdi\n\n"

        "    # Put a newline in place of the nul terminator\n"
        "    # s[len++] =0x0a \n"
        "    movb $0x0a, (%%rsi, %%rdx)\n"
        "    incq %%rdx\n\n"

        "    syscall\n"
        "    xorq %%rax, %%rax\n"
        "    ret\n\n"

        "__println_char:\n"
        "    pushq %%rbp\n"
        "    movq %%rsp, %%rbp\n"
        "    subq $16, %%rsp # char data[2]\n"

        "    movq $%lld, %%rax\n"
        "    movq %%rdi, (%%rsp)\n"
        "    movq %%rsp, %%rsi\n"
        "    # Put a newline in place of the nul terminator\n"
        "    movq $0x0a, 1(%%rsp)\n"
        "    movq $1, %%rdi\n"
        "    movq $2, %%rdx\n"
        "    syscall\n"
        "    xorq %%rax, %%rax\n"

        "    addq $16, %%rsp\n"
        "    popq %%rbp\n"
        "    ret\n\n"

        "__println_int: \n"
        "    pushq %%rbp\n"
        "    movq %%rsp, %%rbp\n"
        "    subq $32, %%rsp # char data[23]\n"
        "  \n"

        "    movq %%rdi, %%r9 # Store original value of the argument \n"
        "    # Abs \n"
        "    negq %%rdi      \n"
        "    cmovlq %%r9, %%rdi\n"

        "    xorq %%r8, %%r8 # r8: Loop index and length\n"
        "    leaq -1(%%rsp), %%rsi # end ptr\n"
        "    movb $0x0a, (%%rsi) # Trailing newline \n"
        "    \n"
        "    __println_int_loop:\n"
        "        decq %%rsi # end--\n"
        "\n"
        "        # n / 10\n"
        "        movq $10, %%rcx \n"
        "        xorq %%rdx, %%rdx\n"
        "        movq %%rdi, %%rax\n"
        "        idiv %%rcx\n"
        "        movq %%rax, %%rdi\n"
        "    \n"
        "        # *end = rem + '0'\n"
        "        add $48, %%rdx # Convert integer to character by adding '0'\n"
        "        movb %%dl, (%%rsi)\n"
        "\n"
        "        incq %%r8 # len++\n"
        "        cmpq $0, %%rdi # Do-While n != 0\n"
        "        jz __println_int_end\n"
        "\n"
        "        jmp __println_int_loop\n"
        "\n"
        "    __println_int_end:\n"
        "      incq %%r8 # Count newline as well \n"

        "      # Print minus sign ? \n"
        "      cmpq $0, %%r9 \n"
        "      jge __println_int_end_epilog \n"

        "      incq %%r8 \n"
        "      decq %%rsi \n"
        "      movb $45, (%%rsi) \n"

        "    __println_int_end_epilog: \n"
        "      movq $%lld, %%rax\n"
        "      movq $1, %%rdi\n"
        "      movq %%r8, %%rdx\n"
        "      movq %%rsp, %%rsi\n"
        "      subq %%r8, %%rsi\n"
        "\n"
        "      syscall\n"
        "      xorq %%rax, %%rax\n"
        "\n"
        "      # Epilog\n"
        "      addq $32, %%rsp\n"
        "      popq %%rbp\n"
        "      ret\n",
        syscall_write, syscall_write, syscall_write, syscall_write);
}

static void emit_expr(const parser_t* parser, const int expr_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const ast_node_t* const expr = &parser->par_nodes[expr_i];
    const type_t* const type = &parser->par_types[expr->node_type_i];
    const loc_t loc =
        parser->par_lexer.lex_locs[ast_node_first_token(parser, expr)];
    println(".loc 1 %d %d\t## %s:%d:%d", loc.loc_line, loc.loc_column,
            parser->par_file_name0, loc.loc_line, loc.loc_column);

    const char *ax, *di, *dx;
    if (type->ty_size == 8) {
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
            println("mov $%d, %s", (int8_t)expr->node_n.node_num.nu_val, ax);
            return;
        }
        case NODE_STRING: {
            const int obj_i = expr->node_n.node_string;
            println("lea .L%d(%%rip), %%rax", obj_i);

            const obj_t obj = parser->par_objects[obj_i];
            PG_ASSERT_COND(obj.obj_kind, ==, OBJ_GLOBAL_VAR, "%d");

            const char* source = NULL;
            int source_len = 0;
            parser_tok_source(parser, obj.obj_tok_i, &source, &source_len);
            println("mov $%d, %%r8", source_len);
            return;
        }
        case NODE_CHAR: {
            println("mov $%d, %s", (char)expr->node_n.node_num.nu_val, ax);
            return;
        }
        case NODE_LONG: {
            println("mov $%lld, %s", expr->node_n.node_num.nu_val, ax);
            return;
        }
        case NODE_MODULO: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
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
            println("pop %%rdi");
            println("imul %%rdi, %%rax");

            return;
        }
        case NODE_SUBTRACT: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            println("pop %%rdi");
            println("sub %%rdi, %%rax");

            return;
        }
        case NODE_ADD: {
            const binary_t bin = expr->node_n.node_binary;

            emit_expr(parser, bin.bi_rhs_i);
            emit_push();
            emit_expr(parser, bin.bi_lhs_i);
            println("pop %%rdi");
            println("add %s, %s", di, ax);

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
        case NODE_NOT: {
            emit_expr(parser, expr->node_n.node_unary);
            println("cmp $0, %%rax");
            println("sete %%al");
            return;
        }
        case NODE_IF: {
            const if_t node = expr->node_n.node_if;

            emit_expr(parser, node.if_node_cond_i);
            println("cmp $0, %%rax");
            println("je .L.else.%d", node.if_node_cond_i);

            emit_expr(parser, node.if_node_then_i);

            println("jmp .L.end.%d", node.if_node_cond_i);

            println(".L.else.%d:", node.if_node_cond_i);
            if (node.if_node_else_i >= 0)
                emit_expr(parser, node.if_node_else_i);

            println(".L.end.%d:", node.if_node_cond_i);

            return;
        }
        case NODE_BUILTIN_PRINTLN: {
            const ast_builtin_println_t builtin_println = AS_PRINTLN(*expr);
            emit_expr(parser, builtin_println.bp_arg_i);

            const ast_node_t* arg =
                &parser->par_nodes[builtin_println.bp_arg_i];
            const type_kind_t type =
                parser->par_types[arg->node_type_i].ty_kind;

            println("mov %%rax, %%rdi");

            if (type == TYPE_LONG || type == TYPE_INT || type == TYPE_SHORT ||
                type == TYPE_BYTE)
                println("call __println_int");
            else if (type == TYPE_CHAR)
                println("call __println_char");
            else if (type == TYPE_BOOL)
                println("call __println_bool");
            else if (type == TYPE_STRING) {
                println("mov %%r8, %%rsi");
                println("mov %%rax, %%rdi");
                println("call __println_string");
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
            const ast_node_t* const node_var_def =
                &parser->par_nodes[var.va_var_node_i];
            const var_def_t var_def = node_var_def->node_n.node_var_def;
            const int offset = var_def.vd_stack_offset;
            const int type_size =
                parser->par_types[node_var_def->node_type_i].ty_size;

            if (type_size == 1)
                println("mov -%d(%%rbp), %%al", offset);
            else if (type_size == 2)
                println("mov -%d(%%rbp), %%ax", offset);
            else if (type_size == 4)
                println("mov -%d(%%rbp), %%eax", offset);
            else
                println("mov -%d(%%rbp), %%rax", offset);

            return;
        }
            // Forbidden by the grammer
        case NODE_WHILE:
        case NODE_VAR_DEF:
            UNREACHABLE();
    }
    log_debug("node_kind=%s", node_kind_to_str[expr->node_kind]);
    UNREACHABLE();
}

static void emit_stmt(const parser_t* parser, int stmt_i) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");

    const ast_node_t* const stmt = &parser->par_nodes[stmt_i];
    const loc_t loc =
        parser->par_lexer.lex_locs[ast_node_first_token(parser, stmt)];
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
        case NODE_NOT:
        case NODE_IF: {
            emit_expr(parser, stmt_i);
            return;
        }
        case NODE_ASSIGN: {
            const binary_t binary = stmt->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[binary.bi_lhs_i];

            emit_expr(parser, binary.bi_rhs_i);

            const var_t var = lhs->node_n.node_var;
            const ast_node_t* const node_var_def =
                &parser->par_nodes[var.va_var_node_i];
            const var_def_t var_def = node_var_def->node_n.node_var_def;
            const int offset = var_def.vd_stack_offset;

            const int type_size = parser->par_types[stmt->node_type_i].ty_size;

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
            if (var_def.vd_init_node_i < 0) UNIMPLEMENTED();

            emit_expr(parser, var_def.vd_init_node_i);

            const int type_size = parser->par_types[stmt->node_type_i].ty_size;
            const int offset = var_def.vd_stack_offset;

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
            println(".Lwhile_loop_start%d:", w.wh_cond_i);
            emit_expr(parser, w.wh_cond_i);

            println("cmp $0, %%rax");
            println("je .Lwhile_loop_end%d", w.wh_cond_i);

            emit_stmt(parser, w.wh_body_i);
            println("jmp .Lwhile_loop_start%d", w.wh_cond_i);

            println(".Lwhile_loop_end%d:", w.wh_cond_i);
            return;
        }
    }
    log_debug("node_kind=%s", node_kind_to_str[stmt->node_kind]);
    UNREACHABLE();
}

static void emit(const parser_t* parser, FILE* asm_file) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_nodes, !=, NULL, "%p");

    output_file = asm_file;
    println(".data");
    println(".Ltrue: .ascii \"true\\n\"");
    println(".Lfalse: .ascii \"false\\n\"");

    for (int i = 0; i < (int)buf_size(parser->par_objects); i++) {
        const obj_t obj = parser->par_objects[i];
        if (obj.obj_kind != OBJ_GLOBAL_VAR) UNIMPLEMENTED();

        const char* source = NULL;
        int source_len = 0;
        parser_tok_source(parser, obj.obj_tok_i, &source, &source_len);
        println(".L%d: .asciz \"%.*s\"", i, source_len, source);
    }

    println("\n.text");
    emit_stdlib();
    println(".global _main");
    println("_main:");
    println(".file 1 \"%s\"", parser->par_file_name0);

    log_debug("offset=%d", parser->par_offset);

    const int aligned_stack_size = emit_align_to_16(parser->par_offset);
    fn_prolog(aligned_stack_size);

    // Initial scope is at index=0
    emit_stmt(parser, 0);

    emit_program_epilog();
}
