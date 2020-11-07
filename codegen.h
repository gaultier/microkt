#pragma once

#include <stdarg.h>
#include <stdint.h>

#include "ast.h"
#include "common.h"
#include "parse.h"

// TODO: use platform headers for that?
#ifdef __APPLE__
static const int64_t syscall_write = 0x2000004;
#else
static const int64_t syscall_write = 0;  // FIXME
#endif

static FILE* output_file = NULL;
static int stack_depth = 0;  // FIXME

__attribute__((format(printf, 1, 2))) static void println(char* fmt, ...) {
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

static void fn_epilog() {
    println("addq $%d, %%rsp", stack_depth);  // Hardcoded stack size
    println("popq %%rbp");
    println("ret\n");
    stack_depth = 0;
}

static void emit_push() { println("push %%rax"); }

static void emit_print_i64() {
    println(
        "__println_int: \n"
        "    pushq %%rbp\n"
        "    movq %%rsp, %%rbp\n"
        "    subq $32, %%rsp # char data[22]\n"
        "  \n"
        "    xorq %%r8, %%r8 # r8: Loop index and length\n"
        "    leaq -1(%%rsp), %%rsi # end ptr\n"
        "    movb $0x0a, (%%rsi) \n"
        "    \n"
        "    int_to_string_loop:\n"
        "        cmpq $0, %%rax # While n != 0\n"
        "        jz int_to_string_end\n"
        "\n"
        "        decq %%rsi # end--\n"
        "\n"
        "        # n / 10\n"
        "        movq $10, %%rcx \n"
        "        xorq %%rdx, %%rdx\n"
        "        idiv %%rcx\n"
        "    \n"
        "        # *end = rem + '0'\n"
        "        add $48, %%rdx # Convert integer to character by adding '0'\n"
        "        movb %%dl, (%%rsi)\n"
        "\n"
        "        incq %%r8 # len++\n"
        "        jmp int_to_string_loop\n"
        "\n"
        "    int_to_string_end:\n"
        "      incq %%r8 # Count newline as well \n"
        "      movq $0x2000004, %%rax\n"
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
        "      ret\n");
}

static void emit_expr(const parser_t* parser, const ast_node_t* expr) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)expr, !=, NULL, "%p");

    switch (expr->node_kind) {
        case NODE_KEYWORD_BOOL:
        case NODE_STRING:
        case NODE_CHAR:
            UNIMPLEMENTED();
        case NODE_I64: {
            println("movq $%lld, %%rax", expr->node_n.node_num.nu_val);
            return;
        }
        case NODE_SUBTRACT: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, lhs);
            emit_push();
            emit_expr(parser, rhs);
            println("popq %%rdi");
            println("subq %%rdi, %%rax");

            break;
        }
        case NODE_ADD: {
            const binary_t bin = expr->node_n.node_binary;
            const ast_node_t* const lhs = &parser->par_nodes[bin.bi_lhs_i];
            const ast_node_t* const rhs = &parser->par_nodes[bin.bi_rhs_i];

            emit_expr(parser, lhs);
            emit_push();
            emit_expr(parser, rhs);
            println("popq %%rdi");
            println("addq %%rdi, %%rax");

            break;
        }
        default:
            UNREACHABLE();
    }
}

static void emit_stmt(const parser_t* parser, const ast_node_t* stmt) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)stmt, !=, NULL, "%p");

    switch (stmt->node_kind) {
        case NODE_BUILTIN_PRINTLN: {
            const ast_builtin_println_t builtin_println = AS_PRINTLN(*stmt);
            const ast_node_t* arg =
                &parser->par_nodes[builtin_println.bp_arg_i];
            emit_expr(parser, arg);

            println("call __println_int");

            break;
        }
        case NODE_I64:
        case NODE_CHAR:
        case NODE_STRING:
        case NODE_KEYWORD_BOOL:
        case NODE_SUBTRACT:
        case NODE_ADD:
            UNREACHABLE();
    }
}

static void emit(const parser_t* parser, FILE* asm_file) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_nodes, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_stmt_nodes, !=, NULL, "%p");

    output_file = asm_file;
    println(".data");
    println(".Ltrue: .ascii \"true\"");
    println(".Lfalse: .ascii \"false\"");

    for (int i = 0; i < (int)buf_size(parser->par_objects); i++) {
        const obj_t obj = parser->par_objects[i];
        if (obj.obj_kind != OBJ_GLOBAL_VAR) UNIMPLEMENTED();

        const global_var_t var = obj.obj.obj_global_var;
        println(".L%d: .ascii \"%.*s\"", obj.obj_tok_i, var.gl_source_len,
                var.gl_source);
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
    fn_epilog();
}
