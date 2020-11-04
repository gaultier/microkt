#pragma once

#include <stdarg.h>
#include <stdint.h>

#include "ast.h"
#include "common.h"
//#include "ir.h"
//#include "macos_x86_64_stdlib.h"
#include "lex.h"
#include "parse.h"

static const int64_t syscall_write = 0x2000004;

typedef struct {
    emit_op_t* em_ops_arena;
    int em_label_id;
    emit_op_id_t* em_text_section;
    emit_op_id_t* em_data_section;
    emit_op_id_t em_current_block;
} emit_t;

static FILE* output_file = NULL;

__attribute__((format(printf, 1, 2))) static void println(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    fprintf(output_file, "\n");
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
        const global_var_t var = obj.obj.obj_global_var;
        println(".L%d: .ascii \"%.*s\"", obj.obj_tok_i, var.gl_source_len,
                var.gl_source);
    }

    println("\n.text");
    println(".global _main");
    println("_main:");

    for (int i = 0; i < (int)buf_size(parser->par_stmt_nodes); i++) {
        const int stmt_i = parser->par_stmt_nodes[i];
        const ast_node_t* stmt = &parser->par_nodes[stmt_i];

        switch (stmt->node_kind) {
            case NODE_BUILTIN_PRINT: {
                const ast_builtin_print_t builtin_print = AS_PRINT(*stmt);
                const ast_node_t arg =
                    parser->par_nodes[builtin_print.bp_arg_i];

                println("movq $%lld, %%rax", syscall_write);

                if (arg.node_kind == NODE_KEYWORD_BOOL) {
                    const token_index_t index = arg.node_n.node_boolean;
                    const token_id_t tok = parser->par_token_ids[index];

                    println("movq $1, %%rdi");
                    if (tok == LEX_TOKEN_ID_TRUE) {
                        println("leaq .Ltrue(%%rip), %%rsi");
                        println("movq $4, %%rdx");
                    } else {
                        println("leaq .Lfalse(%%rip), %%rsi");
                        println("movq $5, %%rdx");
                    }
                } else if (arg.node_kind == NODE_STRING) {
                    const int obj_i = arg.node_n.node_string;
                    const obj_t obj = parser->par_objects[obj_i];
                    PG_ASSERT_COND(obj.obj_kind, ==, OBJ_GLOBAL_VAR, "%d");

                    println("leaq .L%d(%%rip), %%rsi", obj.obj_tok_i);
                    println("movq $%d, %%rdx",
                            obj.obj.obj_global_var.gl_source_len);
                } else
                    UNREACHABLE();

                println("syscall\n");
                println("xorq %%rax, %%rax\n");
                break;
            }
            case NODE_I64:
            case NODE_CHAR:
            case NODE_STRING:
            case NODE_KEYWORD_BOOL:
                UNREACHABLE();
        }
    }
    println("ret");
}
