#pragma once

#include "ast.h"
#include "parse.h"

typedef enum {
    REG_RAX,
    REG_RBX,
    REG_RCX,
    REG_RDX,
    REG_RBP,
    REG_RSP,
    REG_RSI,
    REG_RDI,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_RIP,
} reg_t;

const u8 reg_t_to_str[][4] = {
    [REG_RAX] = "%rax", [REG_RBX] = "%rbx", [REG_RCX] = "%rcx",
    [REG_RDX] = "%rdx", [REG_RBP] = "%rbp", [REG_RSP] = "%rsp",
    [REG_RSI] = "%rsi", [REG_RDI] = "%rdi", [REG_R8] = "%r8",
    [REG_R9] = "%r9",   [REG_R10] = "%r10", [REG_R11] = "%r11",
    [REG_R12] = "%r12", [REG_R13] = "%r13", [REG_R14] = "%r14",
    [REG_R15] = "%r15", [REG_RIP] = "%rip"};

reg_t emit_fn_arg(u16 position) {
    switch (position) {
        case 0:
            return REG_RAX;
        case 1:
            return REG_RDI;
        case 2:
            return REG_RSI;
        case 3:
            return REG_RDX;
        case 4:
            return REG_RCX;
        case 5:
            return REG_R8;
        case 6:
            return REG_R9;
        case 7:
            return REG_R10;
        case 8:
            return REG_R11;
        default:
            UNIMPLEMENTED();
    }
}

typedef enum {
    OP_KIND_SYSCALL,
    OP_KIND_INTEGER_LITERAL,
    OP_KIND_LABEL_ADDRESS,
} emit_op_kind_t;

typedef struct emit_op_t emit_op_t;

typedef struct {
    emit_op_t* args;
} emit_op_syscall_t;

struct emit_op_t {
    emit_op_kind_t op_kind;
    union {
        emit_op_syscall_t op_syscall;
        usize op_integer_literal;
        usize op_label_address;
    } op_o;
};

typedef struct {
    emit_op_t* asm_text_section;
    emit_op_t* asm_data_section;
} emit_asm_t;

const usize syscall_exit_osx = (usize)0x2000001;
const usize syscall_write_osx = (usize)0x2000004;

const usize STIN = 0;
const usize STDOUT = 1;
const usize STDERR = 2;

void emit_emit(parser_t* parser, emit_asm_t* a) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_nodes, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_stmt_nodes, !=, NULL, "%p");
    PG_ASSERT_COND(a, !=, NULL, "%p");

    emit_op_t* text_section = NULL;
    emit_op_t* data_section = NULL;

    usize label_id = 0;

    for (usize i = 0; i < buf_size(parser->par_stmt_nodes); i++) {
        const usize stmt_i = parser->par_stmt_nodes[i];
        const ast_node_t* stmt = &parser->par_nodes[stmt_i];

        switch (stmt->node_kind) {
            case NODE_BUILTIN_PRINT: {
                const ast_builtin_print_t builtin_print =
                    stmt->node_n.node_builtin_print;
                const ast_node_t arg =
                    parser->par_nodes[builtin_print.bp_arg_i];

                const u8* source = NULL;
                usize source_len = 0;
                parser_ast_node_source(parser, &arg, &source, &source_len);
                const usize new_label_id = ++label_id;

                emit_op_t* args = NULL;
                buf_grow(args, 4);
                buf_push(args, ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                            .op_o = {.op_integer_literal =
                                                         syscall_write_osx}}));
                buf_push(args,
                         ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                      .op_o = {.op_integer_literal = STDOUT}}));
                buf_push(
                    args,
                    ((emit_op_t){.op_kind = OP_KIND_LABEL_ADDRESS,
                                 .op_o = {.op_label_address = new_label_id}}));
                buf_push(
                    args,
                    ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                 .op_o = {.op_integer_literal = source_len}}));

                const emit_op_t syscall = {
                    .op_kind = OP_KIND_SYSCALL,
                    .op_o = {.op_syscall = {.args = args}}};
                buf_push(text_section, syscall);
                break;
            }
            default:
                abort();  // Unreachable
        }
    }

    emit_op_t* args = NULL;
    buf_grow(args, 4);
    buf_push(args,
             ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                          .op_o = {.op_integer_literal = syscall_exit_osx}}));
    buf_push(args, ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                .op_o = {.op_integer_literal = 0}}));

    const emit_op_t syscall = {.op_kind = OP_KIND_SYSCALL,
                               .op_o = {.op_syscall = {.args = args}}};
    buf_push(text_section, syscall);

    *a = (emit_asm_t){.asm_text_section = text_section,
                      .asm_data_section = data_section};
}

void emit_asm_dump(emit_asm_t* a, FILE* file) {
    PG_ASSERT_COND(a, !=, NULL, "%p");
    PG_ASSERT_COND(file, !=, NULL, "%p");
}
