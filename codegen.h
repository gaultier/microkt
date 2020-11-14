#pragma once

#include "ast.h"
#include "common.h"
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
static int stack_depth = 0;  // FIXME

static void emit_stmt(const parser_t* parser, const ast_node_t* stmt);

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

static void fn_prolog() {
    println("pushq %%rbp");
    println("movq %%rsp, %%rbp");

    // Hardcoded stack size
    stack_depth = 16;
    println("subq $%d, %%rsp\n", stack_depth);
}

// static void fn_epilog() {
//    println("addq $%d, %%rsp", stack_depth);  // Hardcoded stack size
//    println("popq %%rbp");
//    println("ret\n");
//    stack_depth = 0;
//}

static void emit_program_epilog() {
    println("movq $%lld, %%rax", syscall_exit);
    println("movq $0, %%rdi");
    println("syscall");
}

static void emit_push() { println("push %%rax"); }

static void emit_print_i64() {
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
        "        cmpq $0, %%rdi # While n != 0\n"
        "        jz __println_int_end\n"
        "\n"
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

static void emit_expr(const parser_t* parser, const ast_node_t* expr) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)expr, !=, NULL, "%p");

    switch (expr->node_kind) {
        case NODE_KEYWORD_BOOL: {
            println("movb $%d, %%ah", (int8_t)expr->node_n.node_num.nu_val);
            return;
        }
        case NODE_STRING: {
            const int obj_i = expr->node_n.node_string;
            println("leaq .L%d(%%rip), %%rax", obj_i);

            const obj_t obj = parser->par_objects[obj_i];
            PG_ASSERT_COND(obj.obj_kind, ==, OBJ_GLOBAL_VAR, "%d");

            const global_var_t var = obj.obj.obj_global_var;
            println("movq $%d, %%r8", var.gl_source_len);
            return;
        }
        case NODE_CHAR: {
            println("movb $%d, %%al", (char)expr->node_n.node_num.nu_val);
            return;
        }
        case NODE_I64: {
            println("movq $%lld, %%rax", expr->node_n.node_num.nu_val);
            return;
        }
        case NODE_MODULO: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, rhs);
            emit_push();
            emit_expr(parser, lhs);
            println("popq %%rdi");
            println("cqo");  // ?
            println("xorq %%rdx, %%rdx");
            println("idivq %%rdi");
            println("movq %%rdx, %%rax");

            return;
        }
        case NODE_DIVIDE: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, rhs);
            emit_push();
            emit_expr(parser, lhs);
            println("popq %%rdi");
            println("cqo");  // ?
            println("idivq %%rdi");

            return;
        }
        case NODE_MULTIPLY: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, rhs);
            emit_push();
            emit_expr(parser, lhs);
            println("popq %%rdi");
            println("imul %%rdi, %%rax");

            return;
        }
        case NODE_SUBTRACT: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, rhs);
            emit_push();
            emit_expr(parser, lhs);
            println("popq %%rdi");
            println("subq %%rdi, %%rax");

            return;
        }
        case NODE_ADD: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, rhs);
            emit_push();
            emit_expr(parser, lhs);
            println("popq %%rdi");
            println("addq %%rdi, %%rax");

            return;
        }
        case NODE_LT:
        case NODE_EQ:
        case NODE_NEQ:
        case NODE_LE: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, rhs);
            emit_push();
            emit_expr(parser, lhs);
            println("popq %%rdi");
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

            println("movzb %%al, %%rax");

            return;
        }
        case NODE_NOT: {
            const ast_node_t* const lhs =
                &parser->par_nodes[expr->node_n.node_unary];
            emit_expr(parser, lhs);
            println("cmp $0, %%rax");
            println("sete %%al");
            println("movzx %%al, %%rax");
            return;
        }
        case NODE_IF: {
            const if_t node = expr->node_n.node_if;

            emit_expr(parser, &parser->par_nodes[node.if_node_cond_i]);
            println("cmp $0, %%rax");
            println("je .L.else.%d", node.if_node_cond_i);

            emit_expr(parser, &parser->par_nodes[node.if_node_then_i]);

            println("jmp .L.end.%d\n", node.if_node_cond_i);

            println(".L.else.%d:", node.if_node_cond_i);
            if (node.if_node_else_i >= 0)
                emit_expr(parser, &parser->par_nodes[node.if_node_else_i]);

            println(".L.end.%d:", node.if_node_cond_i);

            return;
        }
        case NODE_BUILTIN_PRINTLN: {
            const ast_builtin_println_t builtin_println = AS_PRINTLN(*expr);
            const ast_node_t* arg =
                &parser->par_nodes[builtin_println.bp_arg_i];
            emit_expr(parser, arg);

            println("movq %%rax, %%rdi");

            const type_kind_t type =
                parser->par_types[arg->node_type_i].ty_kind;
            if (type == TYPE_LONG)
                println("call __println_int");
            else if (type == TYPE_CHAR)
                println("call __println_char");
            else if (type == TYPE_BOOL)
                println("call __println_bool");
            else if (type == TYPE_STRING) {
                println("movq %%r8, %%rsi");
                println("movq %%rax, %%rdi");
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
                const ast_node_t* const stmt = &parser->par_nodes[stmt_node_i];
                emit_stmt(parser, stmt);
            }

            return;
        }
        case NODE_VAR_DEF:
            UNREACHABLE();
    }
    log_debug("node_kind=%s", ast_node_kind_t_to_str[expr->node_kind]);
    UNREACHABLE();
}

static void emit_stmt(const parser_t* parser, const ast_node_t* stmt) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)stmt, !=, NULL, "%p");

    switch (stmt->node_kind) {
        case NODE_BUILTIN_PRINTLN:
        case NODE_BLOCK:
        case NODE_I64:
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
            emit_expr(parser, stmt);
            println("\n");
            return;
        }
        case NODE_VAR_DEF:
            UNIMPLEMENTED();
    }
    log_debug("node_kind=%s", ast_node_kind_t_to_str[stmt->node_kind]);
    UNREACHABLE();
}

static void emit(const parser_t* parser, FILE* asm_file) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_nodes, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_stmt_nodes, !=, NULL, "%p");

    output_file = asm_file;
    println(".data");
    println(".Ltrue: .ascii \"true\\n\"");
    println(".Lfalse: .ascii \"false\\n\"");

    for (int i = 0; i < (int)buf_size(parser->par_objects); i++) {
        const obj_t obj = parser->par_objects[i];
        if (obj.obj_kind != OBJ_GLOBAL_VAR) UNIMPLEMENTED();

        const global_var_t var = obj.obj.obj_global_var;
        println(".L%d: .asciz \"%.*s\"", i, var.gl_source_len, var.gl_source);
    }

    println("\n.text");
    emit_print_i64();
    println(".global _main");
    println("_main:");
    fn_prolog();

    for (int i = 0; i < (int)buf_size(parser->par_stmt_nodes); i++) {
        const int stmt_i = parser->par_stmt_nodes[i];
        const ast_node_t* stmt = &parser->par_nodes[stmt_i];
        emit_stmt(parser, stmt);
    }
    emit_program_epilog();
}
